#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>   /* For mode constants */
#include <string.h>
#include <pthread.h>    /* For threads */
#include <signal.h>    /* For signal handling */
#include <sys/socket.h> /* For sockets */
#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>    /* For INT_MAX */
#include <sys/time.h>   /* For timeval structure */
#include <unistd.h>

#include "server_functions/server_functions.h"
#include "rpc_server/rpc_server.h"

pthread_cond_t cond_copied_client_sd;   // Condition variable to signal the copied client socket descriptor
pthread_mutex_t mutex_copy_client_sd;   // Mutex to protect the copying of the client socket descriptor
int copied_client_sd_flag = 0;          // Copied client socket descriptor flag

pthread_mutex_t mutex_access_users_file;            // Mutex to protect the access to the users file
pthread_mutex_t mutex_access_connected_users_file;  // Mutex to protect the access to the connected users file
pthread_mutex_t mutex_access_published_files_file;  // Mutex to protect the access to the published files file

int server_sd;  // Server socket descriptor


void end(int signum) {
    printf("\nExiting the server...\n");
    // Perform cleanup
    // Close the server socket
    close(server_sd);
    // Destroy the mutexes
    pthread_mutex_destroy(&mutex_copy_client_sd);
    pthread_mutex_destroy(&mutex_access_users_file);
    pthread_mutex_destroy(&mutex_access_connected_users_file);
    pthread_mutex_destroy(&mutex_access_published_files_file);
    // Destroy the condition variable
    pthread_cond_destroy(&cond_copied_client_sd);
    // Exit the program
    exit(0);
}

// Send a message of length len to a socket
int sendMessage(int socket, char * buffer, int len) {
	int r;
	int l = len;
	do {	
		r = write(socket, buffer, l);
		l = l - r;
		buffer = buffer + r;
	} while ((l>0) && (r>=0));
	
	if (r < 0)
		return (-1);   /* fail */
	else
		return(0);	/* full length has been sent */
}

// Receive a message of length len from a socket
int recvMessage(int socket, char *buffer, int len) {
	int r;
	int l = len;
	do {	
		r = read(socket, buffer, l);
		l = l - r ;
		buffer = buffer + r;
	} while ((l>0) && (r>=0));
	
	if (r < 0)
		return (-1);   /* fallo */
	else
		return(0);	/* full length has been receive */
}

// Read a line of arbitrary length (maximum length n) from a file descriptor
ssize_t readLine(int fd, void *buffer, size_t n)
{
	ssize_t numRead;  /* num of bytes fetched by last read() */
	size_t totRead;	  /* total bytes read so far */
	char *buf;
	char ch;


	if (n <= 0 || buffer == NULL) { 
		errno = EINVAL;
		return -1; 
	}

	buf = buffer;
	totRead = 0;
	
	for (;;) {
        	numRead = read(fd, &ch, 1);	/* read a byte */

        	if (numRead == -1) {	
            		if (errno == EINTR)	/* interrupted -> restart read() */
                		continue;
            	else
			return -1;		/* some other error */
        	} else if (numRead == 0) {	/* EOF */
            		if (totRead == 0)	/* no byres read; return 0 */
                		return 0;
			else
                		break;
        	} else {			/* numRead must be 1 if we get here*/
            		if (ch == '\n')
                		break;
            		if (ch == '\0')
                		break;
            		if (totRead < n - 1) {		/* discard > (n-1) bytes */
				totRead++;
				*buf++ = ch; 
			}
		} 
	}
	
	*buf = '\0';
    	return totRead;
}


int call_rpc_log_operation(const char* username, const char* operation_name, const char* time) {
    CLIENT *clnt;

    clnt = clnt_create(RPC_SERVER_IP, OPERATIONLOG, OPERATIONLOG_V1, "tcp");
    if (clnt == NULL) {
        clnt_pcreateerror("Error creating RPC client\n");
        return -1;
    }

    operation op;
    op.username = (char *)username;
    op.operation = (char *)operation_name;
    op.timestamp = (char *)time;

    log_operation_1(op, clnt);

    clnt_destroy(clnt);
    return 0;
}



// Handle a command from a client
void *handle_command(void *arg) {

    // Lock the mutex to protect the copied client socket descriptor
    pthread_mutex_lock(&mutex_copy_client_sd);
    int client_sd = *(int *)arg;
    free(arg);
    // Signal that the client socket descriptor has been copied
    copied_client_sd_flag = 1;
    // Signal the condition variable
    pthread_cond_signal(&cond_copied_client_sd);
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_copy_client_sd);


    // Receive command from the client
    char command[13];  // Maximum command size is 12 characters + '\0'
    if (readLine(client_sd, command, sizeof(command)) < 0) {
        perror("Error receiving command from the client\n");
        char response[2];
        sprintf(response, "%d", 2);
        sendMessage(client_sd, response, strlen(response) + 1);
        shutdown(client_sd, SHUT_RDWR);
        close(client_sd);
        return NULL;
    }
    // Print the command
    // printf("s> COMMAND: %s\n", command);

    // Check if the command is "REGISTER"
    if (strcmp(command, "REGISTER") == 0) {
        // Receive the time from the client
        // Format: DD/MM/YYYY HH:MM:SS
        char time[20];  // Maximum time size is 19 bytes + '\0'
        if (readLine(client_sd, time, sizeof(time)) < 0) {
            perror("Error receiving time from the client\n");
            char response[2];
            sprintf(response, "%d", 2);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // printf("s> TIME: %s\n", time);

        // Receive username from the client
        char username[256];  // Maximum username size is 255 bytes + '\0'
        if (readLine(client_sd, username, sizeof(username)) < 0) {
            perror("Error receiving username from the client\n");
            char response[2];
            sprintf(response, "%d", 2);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Print OPERATION FROM <username>
        printf("s> OPERATION FROM %s\n", username);

        // Call the RPC function log_operation
        int rpc_log_result = call_rpc_log_operation(username, "REGISTER", time);
        if (rpc_log_result < 0) {
            char response[2];
            sprintf(response, "%d", 2);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Lock the mutex to protect the access to the users file
        pthread_mutex_lock(&mutex_access_users_file);
        // Perform the operation
        int result = register_user(username);
        // Unlock the mutex
        pthread_mutex_unlock(&mutex_access_users_file);

        char response[2];
        sprintf(response, "%d", result);

        // printf("RESPONSE: %s\n", response);

        // Send the response to the client
        if (sendMessage(client_sd, response, strlen(response) + 1) < 0) {
            perror("Error sending response to the client\n");
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }
    }

    // Check if the command is "UNREGISTER"
    else if (strcmp(command, "UNREGISTER") == 0) {
        // Receive the time from the client
        // Format: DD/MM/YYYY HH:MM:SS
        char time[20];  // Maximum time size is 19 bytes + '\0'
        if (readLine(client_sd, time, sizeof(time)) < 0) {
            perror("Error receiving time from the client\n");
            char response[2];
            sprintf(response, "%d", 2);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // printf("s> TIME: %s\n", time);

        // Receive username from the client
        char username[256];  // Maximum username size is 255 bytes + '\0'
        if (readLine(client_sd, username, sizeof(username)) < 0) {
            perror("Error receiving username from the client\n");
            char response[2];
            sprintf(response, "%d", 2);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Print OPERATION FROM <username>
        printf("s> OPERATION FROM %s\n", username);

        // Call the RPC function log_operation
        int rpc_log_result = call_rpc_log_operation(username, "UNREGISTER", time);
        if (rpc_log_result < 0) {
            char response[2];
            sprintf(response, "%d", 2);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Lock the mutexes to protect the access to the users, connected users and published files files
        pthread_mutex_lock(&mutex_access_users_file);
        pthread_mutex_lock(&mutex_access_connected_users_file);
        pthread_mutex_lock(&mutex_access_published_files_file);

        // Perform the operation
        int result = unregister_user(username);

        // Unlock the mutexes
        pthread_mutex_unlock(&mutex_access_users_file);
        pthread_mutex_unlock(&mutex_access_connected_users_file);
        pthread_mutex_unlock(&mutex_access_published_files_file);

        char response[2];
        sprintf(response, "%d", result);

        // printf("RESPONSE: %s\n", response);

        // Send the response to the client
        if (sendMessage(client_sd, response, strlen(response) + 1) < 0) {
            perror("Error sending response to the client\n");
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }
    }
    // Check if the command is "CONNECT"
    else if (strcmp(command, "CONNECT") == 0) {
        // Receive the time from the client
        // Format: DD/MM/YYYY HH:MM:SS
        char time[20];  // Maximum time size is 19 bytes + '\0'
        if (readLine(client_sd, time, sizeof(time)) < 0) {
            perror("Error receiving time from the client\n");
            char response[2];
            sprintf(response, "%d", 3);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // printf("s> TIME: %s\n", time);

        // Receive username from the client
        char username[256];  // Maximum username size is 255 bytes + '\0'
        if (readLine(client_sd, username, sizeof(username)) < 0) {
            perror("Error receiving username from the client\n");
            char response[2];
            sprintf(response, "%d", 3);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Receive the Port from the client
        char port[6];  // Maximum port size is 5 bytes + '\0'
        if (readLine(client_sd, port, sizeof(port)) < 0) {
            perror("Error receiving port from the client\n");
            char response[2];
            sprintf(response, "%d", 3);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Print OPERATION FROM <username>
        printf("s> OPERATION FROM %s\n", username);

        // Call the RPC function log_operation
        int rpc_log_result = call_rpc_log_operation(username, "CONNECT", time);
        if (rpc_log_result < 0) {
            char response[2];
            sprintf(response, "%d", 3);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Get the IP of the client
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        if (getpeername(client_sd, (struct sockaddr *)&client_addr, &client_addr_len) == -1) {
            perror("Error getting the client IP\n");
            char response[2];
            sprintf(response, "%d", 3);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Lock the mutexes to protect the access to the users and connected users files
        pthread_mutex_lock(&mutex_access_users_file);
        pthread_mutex_lock(&mutex_access_connected_users_file);

        // Perform the operation
        int result = connect_user(username, inet_ntoa(client_addr.sin_addr), port);

        // Unlock the mutexes
        pthread_mutex_unlock(&mutex_access_users_file);
        pthread_mutex_unlock(&mutex_access_connected_users_file);

        char response[2];
        sprintf(response, "%d", result);

        // printf("RESPONSE: %s\n", response);

        // Send the response to the client
        if (sendMessage(client_sd, response, strlen(response) + 1) < 0) {
            perror("Error sending response to the client\n");
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }
    }

    // Check if the command is "DISCONNECT"
    else if (strcmp(command, "DISCONNECT") == 0) {
        // Receive the time from the client
        // Format: DD/MM/YYYY HH:MM:SS
        char time[20];  // Maximum time size is 19 bytes + '\0'
        if (readLine(client_sd, time, sizeof(time)) < 0) {
            perror("Error receiving time from the client\n");
            char response[2];
            sprintf(response, "%d", 3);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // printf("s> TIME: %s\n", time);

        // Receive username from the client
        char username[256];  // Maximum username size is 255 bytes + '\0'
        if (readLine(client_sd, username, sizeof(username)) < 0) {
            perror("Error receiving username from the client\n");
            char response[2];
            sprintf(response, "%d", 3);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Print OPERATION FROM <username>
        printf("s> OPERATION FROM %s\n", username);

        // Call the RPC function log_operation
        int rpc_log_result = call_rpc_log_operation(username, "DISCONNECT", time);
        if (rpc_log_result < 0) {
            char response[2];
            sprintf(response, "%d", 3);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Lock the mutexes to protect the access to the users and connected users files
        pthread_mutex_lock(&mutex_access_users_file);
        pthread_mutex_lock(&mutex_access_connected_users_file);

        // Perform the operation
        int result = disconnect_user(username);

        // Unlock the mutexes
        pthread_mutex_unlock(&mutex_access_users_file);
        pthread_mutex_unlock(&mutex_access_connected_users_file);

        char response[2];
        sprintf(response, "%d", result);

        // printf("RESPONSE: %s\n", response);

        // Send the response to the client
        if (sendMessage(client_sd, response, strlen(response) + 1) < 0) {
            perror("Error sending response to the client\n");
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }
    }

    // Check if the command is "LIST_USERS"
    else if (strcmp(command, "LIST_USERS") == 0) {
        // Receive the time from the client
        // Format: DD/MM/YYYY HH:MM:SS
        char time[20];  // Maximum time size is 19 bytes + '\0'
        if (readLine(client_sd, time, sizeof(time)) < 0) {
            perror("Error receiving time from the client\n");
            char response[2];
            sprintf(response, "%d", 3);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // printf("s> TIME: %s\n", time);

        // Receive username from the client
        char username[256];  // Maximum username size is 255 bytes + '\0'
        if (readLine(client_sd, username, sizeof(username)) < 0) {
            perror("Error receiving username from the client\n");
            char response[2];
            sprintf(response, "%d", 3);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Print OPERATION FROM <username>
        printf("s> OPERATION FROM %s\n", username);

        // Call the RPC function log_operation
        int rpc_log_result = call_rpc_log_operation(username, "LIST_USERS", time);
        if (rpc_log_result < 0) {
            char response[2];
            sprintf(response, "%d", 3);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        {
            // Lock the mutex to protect the access to the users file
            pthread_mutex_lock(&mutex_access_users_file);

            // Check if the user exists
            FILE *users_file = fopen("users.txt", "a+");
            if (users_file == NULL) {
                perror("Error opening users file\n");
                char response[2];
                sprintf(response, "%d", 3);
                sendMessage(client_sd, response, strlen(response) + 1);
                shutdown(client_sd, SHUT_RDWR);
                close(client_sd);
                // Unlock the mutex
                pthread_mutex_unlock(&mutex_access_users_file);
                return NULL;
            }

            // Line size: 255 (username) + 1 (newline) + 1 (null terminator) = 257
            char line[257];
            int found = 0;
            while (fgets(line, sizeof(line), users_file) != NULL) {
                // Remove newline character from line
                line[strcspn(line, "\n")] = '\0';
                if (strcmp(line, username) == 0) {
                    found = 1;
                    break;
                }
            }

            fclose(users_file);
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_access_users_file);

            if (!found) {
                char response[2];
                sprintf(response, "%d", 1);
                sendMessage(client_sd, response, strlen(response) + 1);
                shutdown(client_sd, SHUT_RDWR);
                close(client_sd);
                return NULL;
            }
        }
        {
            // Lock the mutex to protect the access to the connected users file
            pthread_mutex_lock(&mutex_access_connected_users_file);

            FILE *connected_users_file = fopen("connected_users.txt", "a+");
            if (connected_users_file == NULL) {
                perror("Error opening connected users file\n");
                char response[2];
                sprintf(response, "%d", 3);
                sendMessage(client_sd, response, strlen(response) + 1);
                shutdown(client_sd, SHUT_RDWR);
                close(client_sd);
                // Unlock the mutex
                pthread_mutex_unlock(&mutex_access_connected_users_file);
                return NULL;
            }

            // Check if the username is connected and count the number of connected users
            // Line size: 255 (username) + 15 (IP) + 5 (port) + 3 (spaces) + 1 (newline) + 1 (null terminator) = 280
            char line[280];
            int found = 0;
            int connected_users_count = 0;
            while (fgets(line, sizeof(line), connected_users_file) != NULL) {
                // Remove newline character from line
                line[strcspn(line, "\n")] = '\0';
                char *line_username = strtok(line, " ");
                if (strcmp(line_username, username) == 0) {
                    found = 1;
                }
                connected_users_count++;
            }

            fclose(connected_users_file);

            // Unlock the mutex
            pthread_mutex_unlock(&mutex_access_connected_users_file);

            // If the username was not found
            if (!found) {
                char response[2];
                sprintf(response, "%d", 2);
                sendMessage(client_sd, response, strlen(response) + 1);
                shutdown(client_sd, SHUT_RDWR);
                close(client_sd);
                return NULL;
            }

            // Send a 0 response to the client
            char response[2];
            sprintf(response, "%d", 0);
            if (sendMessage(client_sd, response, strlen(response) + 1) < 0) {
                perror("Error sending response to the client\n");
                shutdown(client_sd, SHUT_RDWR);
                close(client_sd);
                return NULL;
            }

            // Send the number of connected users to the client
            char num_users[7];
            sprintf(num_users, "%d", connected_users_count);
            if (sendMessage(client_sd, num_users, strlen(num_users) + 1) < 0) {
                perror("Error sending response to the client\n");
                shutdown(client_sd, SHUT_RDWR);
                close(client_sd);
                return NULL;
            }

            // Lock the mutex to protect the access to the connected users file
            pthread_mutex_lock(&mutex_access_connected_users_file);

            // Open the connected users file again
            connected_users_file = fopen("connected_users.txt", "a+");
            if (connected_users_file == NULL) {
                perror("Error opening connected users file\n");
                char response[2];
                sprintf(response, "%d", 3);
                sendMessage(client_sd, response, strlen(response) + 1);
                shutdown(client_sd, SHUT_RDWR);
                close(client_sd);
                // Unlock the mutex
                pthread_mutex_unlock(&mutex_access_connected_users_file);
                return NULL;
            }

            // Send the connected users to the client line by line,
            // each line sending the username, IP and port independently
            while (fgets(line, sizeof(line), connected_users_file) != NULL) {
                // printf("Line: %s", line);
                // Remove newline character from line
                char *line_username = strtok(line, " ");
                char *line_ip = strtok(NULL, " ");
                char *line_port = strtok(NULL, "\n");

                // Send the username to the client
                if (sendMessage(client_sd, line_username, strlen(line_username) + 1) < 0) {
                    perror("Error sending response to the client\n");
                    shutdown(client_sd, SHUT_RDWR);
                    close(client_sd);
                    // Close the file
                    fclose(connected_users_file);
                    // Unlock the mutex
                    pthread_mutex_unlock(&mutex_access_connected_users_file);
                    return NULL;
                }

                // Send the IP to the client
                if (sendMessage(client_sd, line_ip, strlen(line_ip) + 1) < 0) {
                    perror("Error sending response to the client\n");
                    shutdown(client_sd, SHUT_RDWR);
                    close(client_sd);
                    // Close the file
                    fclose(connected_users_file);
                    // Unlock the mutex
                    pthread_mutex_unlock(&mutex_access_connected_users_file);
                    return NULL;
                }

                // Send the port to the client
                if (sendMessage(client_sd, line_port, strlen(line_port) + 1) < 0) {
                    perror("Error sending response to the client\n");
                    shutdown(client_sd, SHUT_RDWR);
                    close(client_sd);
                    // Close the file
                    fclose(connected_users_file);
                    // Unlock the mutex
                    pthread_mutex_unlock(&mutex_access_connected_users_file);
                    return NULL;
                }
            }

            fclose(connected_users_file);

            // Unlock the mutex
            pthread_mutex_unlock(&mutex_access_connected_users_file);
        }
    }
    
    // Check if the command is "PUBLISH"
    else if (strcmp(command, "PUBLISH") == 0) {
        // Receive the time from the client
        // Format: DD/MM/YYYY HH:MM:SS
        char time[20];  // Maximum time size is 19 bytes + '\0'
        if (readLine(client_sd, time, sizeof(time)) < 0) {
            perror("Error receiving time from the client\n");
            char response[2];
            sprintf(response, "%d", 4);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // printf("s> TIME: %s\n", time);

        // Receive username from the client
        char username[256];  // Maximum username size is 255 bytes + '\0'
        if (readLine(client_sd, username, sizeof(username)) < 0) {
            perror("Error receiving username from the client\n");
            char response[2];
            sprintf(response, "%d", 4);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Print OPERATION FROM <username>
        printf("s> OPERATION FROM %s\n", username);

        // Receive the filename from the client
        char filename[256];  // Maximum filename size is 255 bytes + '\0'
        if (readLine(client_sd, filename, sizeof(filename)) < 0) {
            perror("Error receiving filename from the client\n");
            char response[2];
            sprintf(response, "%d", 4);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Call the RPC function log_operation
        // In this case, the operation is "PUBLISH" <filename>
        char operation[269];    // 12 Bytes for operation name, 255 for filename, 1 for space, 1 for null terminator
        sprintf(operation, "PUBLISH %s", filename);
        int rpc_log_result = call_rpc_log_operation(username, operation, time);
        if (rpc_log_result < 0) {
            char response[2];
            sprintf(response, "%d", 4);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Receive the description from the client
        char description[256];  // Maximum description size is 255 bytes + '\0'
        if (readLine(client_sd, description, sizeof(description)) < 0) {
            perror("Error receiving description from the client\n");
            char response[2];
            sprintf(response, "%d", 4);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Lock the mutexes to protect the access to the users, connected users and published files files
        pthread_mutex_lock(&mutex_access_users_file);
        pthread_mutex_lock(&mutex_access_connected_users_file);
        pthread_mutex_lock(&mutex_access_published_files_file);

        // Perform the operation
        int result = publish(username, filename, description);

        // Unlock the mutexes
        pthread_mutex_unlock(&mutex_access_users_file);
        pthread_mutex_unlock(&mutex_access_connected_users_file);
        pthread_mutex_unlock(&mutex_access_published_files_file);

        char response[2];
        sprintf(response, "%d", result);

        // printf("RESPONSE: %s\n", response);

        // Send the response to the client
        if (sendMessage(client_sd, response, strlen(response) + 1) < 0) {
            perror("Error sending response to the client\n");
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }
    }

    // Check if the command is "DELETE"
    else if (strcmp(command, "DELETE") == 0) {
        // Receive the time from the client
        // Format: DD/MM/YYYY HH:MM:SS
        char time[20];  // Maximum time size is 19 bytes + '\0'
        if (readLine(client_sd, time, sizeof(time)) < 0) {
            perror("Error receiving time from the client\n");
            char response[2];
            sprintf(response, "%d", 4);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // printf("s> TIME: %s\n", time);

        // Receive username from the client
        char username[256];  // Maximum username size is 255 bytes + '\0'
        if (readLine(client_sd, username, sizeof(username)) < 0) {
            perror("Error receiving username from the client\n");
            char response[2];
            sprintf(response, "%d", 4);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Print OPERATION FROM <username>
        printf("s> OPERATION FROM %s\n", username);

        // Receive the filename from the client
        char filename[256];  // Maximum filename size is 255 bytes + '\0'
        if (readLine(client_sd, filename, sizeof(filename)) < 0) {
            perror("Error receiving filename from the client\n");
            char response[2];
            sprintf(response, "%d", 4);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Call the RPC function log_operation
        // In this case, the operation is "DELETE" <filename>
        char operation[269];    // 12 Bytes for operation name, 255 for filename, 1 for space, 1 for null terminator
        sprintf(operation, "DELETE %s", filename);
        int rpc_log_result = call_rpc_log_operation(username, operation, time);
        if (rpc_log_result < 0) {
            char response[2];
            sprintf(response, "%d", 4);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Lock the mutexes to protect the access to the users, connected users and published files files
        pthread_mutex_lock(&mutex_access_users_file);
        pthread_mutex_lock(&mutex_access_connected_users_file);
        pthread_mutex_lock(&mutex_access_published_files_file);

        // Perform the operation
        int result = delete(username, filename);

        // Unlock the mutexes
        pthread_mutex_unlock(&mutex_access_users_file);
        pthread_mutex_unlock(&mutex_access_connected_users_file);
        pthread_mutex_unlock(&mutex_access_published_files_file);

        char response[2];
        sprintf(response, "%d", result);

        // printf("RESPONSE: %s\n", response);

        // Send the response to the client
        if (sendMessage(client_sd, response, strlen(response) + 1) < 0) {
            perror("Error sending response to the client\n");
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }    
    }

    // Check if the command is "LIST_CONTENT"
    else if (strcmp(command, "LIST_CONTENT") == 0) {
        // Receive the time from the client
        // Format: DD/MM/YYYY HH:MM:SS
        char time[20];  // Maximum time size is 19 bytes + '\0'
        if (readLine(client_sd, time, sizeof(time)) < 0) {
            perror("Error receiving time from the client\n");
            char response[2];
            sprintf(response, "%d", 5);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // printf("s> TIME: %s\n", time);

        // Receive username from the client
        char username[256];  // Maximum username size is 255 bytes + '\0'
        if (readLine(client_sd, username, sizeof(username)) < 0) {
            perror("Error receiving username from the client\n");
            char response[2];
            sprintf(response, "%d", 5);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Print OPERATION FROM <username>
        printf("s> OPERATION FROM %s\n", username);

        // Call the RPC function log_operation
        int rpc_log_result = call_rpc_log_operation(username, "LIST_CONTENT", time);
        if (rpc_log_result < 0) {
            char response[2];
            sprintf(response, "%d", 5);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Lock the mutex to protect the access to the users file
        pthread_mutex_lock(&mutex_access_users_file);

        // Check if the user exists
        int flag = user_exists(username);
        if (flag == 0) {
            char response[2];
            sprintf(response, "%d", 1);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_access_users_file);
            return NULL;
        }

        // Unlock the mutex
        pthread_mutex_unlock(&mutex_access_users_file);

        // Lock the mutex to protect the access to the connected users file
        pthread_mutex_lock(&mutex_access_connected_users_file);

        // Check if the user is connected
        flag = is_user_connected(username);
        if (flag == 0) {
            char response[2];
            sprintf(response, "%d", 2);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_access_connected_users_file);
            return NULL;
        }

        // Unlock the mutex
        pthread_mutex_unlock(&mutex_access_connected_users_file);

        // Receive the username of the user whose content is to be listed
        char user_to_list[256];  // Maximum username size is 255 bytes + '\0'
        if (readLine(client_sd, user_to_list, sizeof(user_to_list)) < 0) {
            perror("Error receiving username from the client\n");
            char response[2];
            sprintf(response, "%d", 5);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            return NULL;
        }

        // Lock the mutex to protect the access to the users file
        pthread_mutex_lock(&mutex_access_users_file);

        // Check if the user exists
        flag = user_exists(user_to_list);
        if (flag == 0) {
            char response[2];
            sprintf(response, "%d", 3);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_access_users_file);
            return NULL;
        }

        // Unlock the mutex
        pthread_mutex_unlock(&mutex_access_users_file);

        // Lock the mutex to protect the access to the published files file
        pthread_mutex_lock(&mutex_access_published_files_file);

        // Get the content of the user_to_list
        // Each line of the file is: <username> <filename> <description>
        FILE *content_file = fopen("published_files.txt", "a+");
        if (content_file == NULL) {
            perror("Error opening content file\n");
            char response[2];
            sprintf(response, "%d", 4);
            sendMessage(client_sd, response, strlen(response) + 1);
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_access_published_files_file);
            return NULL;
        }

        // Send a 0 response to the client
        char response[2];
        sprintf(response, "%d", 0);
        if (sendMessage(client_sd, response, strlen(response) + 1) < 0) {
            perror("Error sending response to the client\n");
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            // Close the file
            fclose(content_file);
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_access_published_files_file);
            return NULL;
        }

        // Line size: 255 (username) + 255 (filename) + 255 (description) + 2 (spaces) + 1 (newline) + 1 (null terminator) = 769
        char line[769];
        int user_files_count = 0;

        // First count the number of files and send it to the client
        while (fgets(line, sizeof(line), content_file) != NULL) {
            // Remove newline character from line
            line[strcspn(line, "\n")] = '\0';
            char *line_username = strtok(line, " ");
            char *line_filename = strtok(NULL, " ");
            char *line_description = strtok(NULL, "\n");

            if (strcmp(line_username, user_to_list) == 0) {
                user_files_count++;
            }
        }

        // Then send the number of files to the client line by line,
        // each line sending the filename and description independently
        char num_files[7];
        sprintf(num_files, "%d", user_files_count);
        if (sendMessage(client_sd, num_files, strlen(num_files) + 1) < 0) {
            perror("Error sending response to the client\n");
            shutdown(client_sd, SHUT_RDWR);
            close(client_sd);
            // Close the file
            fclose(content_file);
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_access_published_files_file);
            return NULL;
        }

        // Reset the file pointer to the beginning of the file
        fseek(content_file, 0, SEEK_SET);

        // Send the files to the client
        while (fgets(line, sizeof(line), content_file) != NULL) {
            // Remove newline character from line
            line[strcspn(line, "\n")] = '\0';
            char *line_username = strtok(line, " ");
            char *line_filename = strtok(NULL, " ");
            char *line_description = strtok(NULL, "\n");

            if (strcmp(line_username, user_to_list) == 0) {
                // Send the filename to the client
                if (sendMessage(client_sd, line_filename, strlen(line_filename) + 1) < 0) {
                    perror("Error sending response to the client\n");
                    shutdown(client_sd, SHUT_RDWR);
                    close(client_sd);
                    // Close the file
                    fclose(content_file);
                    // Unlock the mutex
                    pthread_mutex_unlock(&mutex_access_published_files_file);
                    return NULL;
                }

                // Send the description to the client
                if (sendMessage(client_sd, line_description, strlen(line_description) + 1) < 0) {
                    perror("Error sending response to the client\n");
                    shutdown(client_sd, SHUT_RDWR);
                    close(client_sd);
                    // Close the file
                    fclose(content_file);
                    // Unlock the mutex
                    pthread_mutex_unlock(&mutex_access_published_files_file);
                    return NULL;
                }
            }
        }

        fclose(content_file);
        // Unlock the mutex
        pthread_mutex_unlock(&mutex_access_published_files_file);
    }

    shutdown(client_sd, SHUT_RDWR);
    close(client_sd);
    return NULL;
}



int main(int argc, char *argv[])
{
    pthread_t thread_id;
    pthread_attr_t t_attr;

    int port;
    int *client_sd = malloc(sizeof(int));

    // The maximum possible length of a command is 12 characters + '\0'
    char command[13];

    if (argc != 3 || strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
        exit(1);
    }

    port = atoi(argv[2]);

    signal(SIGINT, end);

    // Reset the server files
    if (init_server() != 0) {
        perror("Error initializing the server\n");
        return -1;
    }

    struct sockaddr_in server_addr, client_addr = {0};  // Server and client addresses
    socklen_t client_addr_len = sizeof(client_addr);    // Length of the client address

    // Timeout for the client socket
    int CLIENT_TIMEOUT_SEC = 5;     // Timeout for the client socket (seconds)
    int CLIENT_TIMEOUT_USEC = 0;    // Timeout for the client socket (microseconds)
    struct timeval timeout;
    timeout.tv_sec = CLIENT_TIMEOUT_SEC;
    timeout.tv_usec = CLIENT_TIMEOUT_USEC;

    // Create the server socket
    if ((server_sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
        perror("Error creating the server socket\n");
        return -1;
    }

    // Set the SO_REUSEADDR option
    int enable = 1;
    if (setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1){
        perror("Error setting the SO_REUSEADDR option\n");
        return -1;
    }
    // Fill the server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);    // Listen on any address

    // Bind the server socket to the server address
    if (bind(server_sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1){
        perror("Error binding the server socket\n");
        shutdown(server_sd, SHUT_RDWR);
        close(server_sd);
        return -1;
    }

    // Initialize the mutex and condition variable
    pthread_attr_init(&t_attr); // IMPORTANT: Initialize the thread attributes (the thread creation failed sometimes without this line)
    pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);

    // Listen for connections
    if (listen(server_sd, SOMAXCONN) == -1){    // SOMAXCONN is the maximum number of pending connections (1024 by default)
        perror("Error listening for connections\n");
        shutdown(server_sd, SHUT_RDWR);
        close(server_sd);
        return -1;
    }

    // s> init server <local IP>:<port>
    printf("s> init server %s:%d\n", inet_ntoa(server_addr.sin_addr), port);

    while (1)
    {
        printf("s>\n");

        // Connect with the client
        int *client_sd = malloc(sizeof(int));
        *client_sd = accept(server_sd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (*client_sd == -1){
            perror("Error accepting the connection\n");
            free(client_sd);
            continue;
        }

        // printf("Connection accepted from IP: %s, Port: %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Set the timeout for receive operations on the client socket
        if (setsockopt(*client_sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0){
            perror("Error setting the timeout for the client socket\n");
            shutdown(*client_sd, SHUT_RDWR);
            close(*client_sd);
            free(client_sd);
            continue;
        }

        // Create a new thread to handle the command
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_command, client_sd) != 0) {
            perror("Error creating thread\n");
            shutdown(*client_sd, SHUT_RDWR);
            close(*client_sd);
            free(client_sd);
            continue;
        }

        // Lock the mutex to protect the copied client socket descriptor
        pthread_mutex_lock(&mutex_copy_client_sd);
        // Wait for the client socket descriptor to be copied
        while (!copied_client_sd_flag) {
            pthread_cond_wait(&cond_copied_client_sd, &mutex_copy_client_sd);
        }
        // Reset the copied client socket descriptor flag
        copied_client_sd_flag = 0;
        // Unlock the mutex
        pthread_mutex_unlock(&mutex_copy_client_sd);

    }

    return 0;
}