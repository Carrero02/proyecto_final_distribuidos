#include "server_functions.h"


int user_exists(const char *username) {
    FILE *users_file = fopen("users.txt", "a+");
    if (users_file == NULL) {
        perror("Error opening users file\n");
        return -1;
    }

    // Line size: 255 (username) + 1 (newline) + 1 (null terminator) = 257
    char line[257];
    while (fgets(line, sizeof(line), users_file) != NULL) {
        // Remove newline character from line
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, username) == 0) {
            fclose(users_file);
            return 1;
        }
    }

    fclose(users_file);
    return 0;
}


int is_user_connected(const char *username) {
    FILE *connected_users_file = fopen("connected_users.txt", "a+");
    if (connected_users_file == NULL) {
        perror("Error opening connected users file\n");
        return -1;
    }

    // Line size: 255 (username) + 15 (IP) + 5 (port) + 2 (spaces) + 1 (newline) + 1 (null terminator) = 279
    char line[279];
    int found = 0;
    while (fgets(line, sizeof(line), connected_users_file) != NULL) {
        char *line_username = strtok(line, " ");
        if (strcmp(line_username, username) == 0) {
            found = 1;
            break;
        }
    }

    fclose(connected_users_file);
    return found;
}


/**
 * @retval -1 if error
 * @retval 0 if user has already published the file
 * @retval 1 if user has not published the file
*/
int is_file_published(const char *username, const char *filename) {
    FILE *published_files_file = fopen("published_files.txt", "a+");
    if (published_files_file == NULL) {
        perror("Error opening published files file\n");
        return -1;
    }

    // Line size: 255 (username) + 255 (filename) + 255 (description) + 2 (spaces) + 1 (newline) + 1 (null terminator) = 769
    char line[769];
    int found = 0;
    while (fgets(line, sizeof(line), published_files_file) != NULL) {
        char *line_username = strtok(line, " ");
        char *line_filename = strtok(NULL, " ");
        // File found if both username and filename match
        if (strcmp(line_username, username) == 0 && strcmp(line_filename, filename) == 0) {
            found = 1;
            break;
        }
    }

    fclose(published_files_file);
    return found;
}


/**
 * @brief This function receives a user's username and deletes all the files
 * published by the user
 * 
 * @return int error code
 * @retval 0 No error
 * @retval 2 Other error
 */
int delete_all_user_files(char *username) {
    // Remove the files from the published files file
    FILE *published_files_file = fopen("published_files.txt", "a+");
    if (published_files_file == NULL) {
        perror("Error opening published files file\n");
        return 2;
    }

    // Create a temporary file
    FILE *temp_file = fopen("temp_pub.txt", "w");
    if (temp_file == NULL) {
        perror("Error creating temporary file\n");
        fclose(published_files_file);
        return 2;
    }
    
    // Write all lines except the one to be removed to the temporary file
    // Line size: 255 (username) + 255 (filename) + 255 (description) + 2 (spaces) + 1 (newline) + 1 (null terminator) = 769
    char line[769];
    while (fgets(line, sizeof(line), published_files_file) != NULL) {
        char line_copy[769];
        strcpy(line_copy, line);
        char *line_username = strtok(line, " ");
        if (strcmp(line_username, username) != 0) {
            fprintf(temp_file, "%s", line_copy);
        }
    }

    fclose(published_files_file);
    fclose(temp_file);

    // Replace the published files file with the temporary file
    if (rename("temp_pub.txt", "published_files.txt") != 0) {
        perror("Error renaming temporary file\n");
        return 2;
    }

    return 0;
}


int init_server() {
    if (remove("connected_users.txt") != 0) {
        if (errno != ENOENT) {
            perror("Error deleting connected users file\n");
            return 1;
        }
    }
    return 0;
}


int register_user(char *username) {
    // Check if the username already exists
    int flag = user_exists(username);
    if (flag == 1) {
        return 1;
    }
    else if (flag == -1) {
        return 2;
    }

    // Save the username in the file
    FILE *users_file = fopen("users.txt", "a+");
    if (users_file == NULL) {
        perror("Error opening users file\n");
        return 2;
    }
    fprintf(users_file, "%s\n", username);
    fclose(users_file);
    return 0;
}


int unregister_user(char *username) {
    // Check that the user exists
    int flag = user_exists(username);
    if (flag == 0) {
        return 1;
    }
    else if (flag == -1) {
        return 2;
    }
    // Check that the user is not connected
    flag = is_user_connected(username);
    if (flag != 0) {
        return 2;
    }

    // Delete all files published by the user
    if (delete_all_user_files(username) != 0) {
        return 2;
    }
    
    // Remove the username from the users file
    FILE *users_file = fopen("users.txt", "r");
    if (users_file == NULL) {
        perror("Error opening users file\n");
        return 2;
    }

    // Create a temporary file
    FILE *temp_file = fopen("temp_us.txt", "w");
    if (temp_file == NULL) {
        perror("Error creating temporary file\n");
        fclose(users_file);
        return 2;
    }

    // Line size: 255 (username) + 1 (newline) + 1 (null terminator) = 257
    char line[257];
    while (fgets(line, sizeof(line), users_file) != NULL) {
        // Remove newline character from line
        line[strcspn(line, "\n")] = '\0';
        // Write the line to the temporary file if it is not the username to be removed
        if (strcmp(line, username) != 0) {
            fprintf(temp_file, "%s\n", line);
        }
    }

    fclose(users_file);
    fclose(temp_file);

    // Replace the users file with the temporary file
    if (rename("temp_us.txt", "users.txt") != 0) {
        perror("Error renaming temporary file\n");
        return 2;
    }

    return 0;

}


int connect_user(char *username, char *ip, char *port) {

    // Check if the user exists
    int flag = user_exists(username);
    if (flag == 0) {
        return 1;
    }
    else if (flag == -1) {
        return 3;
    }

    FILE *connected_users_file = fopen("connected_users.txt", "a+");
    if (connected_users_file == NULL) {
        perror("Error opening connected users file\n");
        return 3;
    }

    // Check if the username is already connected
    // Line size: 255 (username) + 15 (IP) + 5 (port) + 2 (spaces) + 1 (newline) + 1 (null terminator) = 279
    char line[279];
    while (fgets(line, sizeof(line), connected_users_file) != NULL) {
        char *line_username = strtok(line, " ");
        if (strcmp(line_username, username) == 0) {
            fclose(connected_users_file);
            return 2;
        }
    }

    // Save the username, IP and port in the file
    fprintf(connected_users_file, "%s %s %s\n", username, ip, port);
    fclose(connected_users_file);
    return 0;
}


int disconnect_user(char *username) {

    // Check if the user exists
    int flag = user_exists(username);
    if (flag == 0) {
        return 1;
    }
    else if (flag == -1) {
        return 3;
    }

    FILE *connected_users_file = fopen("connected_users.txt", "a+");
    if (connected_users_file == NULL) {
        perror("Error opening connected users file\n");
        return 3;
    }

    // Create a temporary file
    FILE *temp_file = fopen("temp_con.txt", "w");
    if (temp_file == NULL) {
        perror("Error creating temporary file\n");
        fclose(connected_users_file);
        return 3;
    }

    // Check if the username is connected
    // Line size: 255 (username) + 15 (IP) + 5 (port) + 2 (spaces) + 1 (newline) + 1 (null terminator) = 279
    char line[279];
    int found = 0;
    while (fgets(line, sizeof(line), connected_users_file) != NULL) {
        char line_copy[279];
        strcpy(line_copy, line);
        // printf("%s\n", line);
        char *line_username = strtok(line, " ");
        if (strcmp(line_username, username) == 0) {
            found = 1;
        } else {
            // printf("%s\n", line);
            fprintf(temp_file, "%s", line_copy);
        }
    }

    fclose(connected_users_file);
    fclose(temp_file);

    // If the username was not found
    if (!found) {
        remove("temp_con.txt");
        return 2;
    }

    // Replace the connected users file with the temporary file
    if (rename("temp_con.txt", "connected_users.txt") != 0) {
        perror("Error renaming temporary file\n");
        return 3;
    }

    return 0;
}


int publish(char *username, char *filename, char *description) {
    // Check if the user exists
    int flag = user_exists(username);
    if (flag == 0) {
        return 1;
    }
    else if (flag == -1) {
        return 4;
    }
    // Check if the user is connected
    flag = is_user_connected(username);
    if (flag == 0) {
        return 2;
    }
    else if (flag == -1) {
        return 4;
    }
    // Check if the file is already published
    flag = is_file_published(username, filename);
    if (flag == 1) {
        return 3;
    }
    else if (flag == -1) {
        return 4;
    }
    // printf("Saving file\n");
    // Save the information in the file as <username> <filename> <description>
    FILE *published_files_file = fopen("published_files.txt", "a+");
    if (published_files_file == NULL) {
        perror("Error opening published files file\n");
        return 4;
    }

    fprintf(published_files_file, "%s %s %s\n", username, filename, description);
    fclose(published_files_file);
    return 0;
}


int delete(char *username, char *filename) {
    // Check if the user exists
    int flag = user_exists(username);
    if (flag == 0) {
        return 1;
    }
    else if (flag == -1) {
        return 4;
    }
    // Check if the user is connected
    flag = is_user_connected(username);
    if (flag == 0) {
        return 2;
    }
    else if (flag == -1) {
        return 4;
    }
    // Check if the file is published
    flag = is_file_published(username, filename);
    if (flag == 0) {
        return 3;
    }
    else if (flag == -1) {
        return 4;
    }

    // Remove the file from the published files file
    FILE *published_files_file = fopen("published_files.txt", "a+");
    if (published_files_file == NULL) {
        perror("Error opening published files file\n");
        return 4;
    }

    // Create a temporary file
    FILE *temp_file = fopen("temp_pub.txt", "w");
    if (temp_file == NULL) {
        perror("Error creating temporary file\n");
        fclose(published_files_file);
        return 4;
    }
    
    // Write all lines except the one to be removed to the temporary file
    // Line size: 255 (username) + 255 (filename) + 255 (description) + 2 (spaces) + 1 (newline) + 1 (null terminator) = 769
    char line[769];
    while (fgets(line, sizeof(line), published_files_file) != NULL) {
        char line_copy[769];
        // printf("%s\n", line);
        strcpy(line_copy, line);
        char *line_username = strtok(line, " ");
        char *line_filename = strtok(NULL, " ");
        if (strcmp(line_username, username) != 0 || strcmp(line_filename, filename) != 0) {
            fprintf(temp_file, "%s", line_copy);
        }
    }

    fclose(published_files_file);
    fclose(temp_file);

    // Replace the published files file with the temporary file
    if (rename("temp_pub.txt", "published_files.txt") != 0) {
        perror("Error renaming temporary file\n");
        return 4;
    }

    return 0;
}
