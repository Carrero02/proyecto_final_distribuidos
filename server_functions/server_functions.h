#ifndef SERVER_FUNCTIONS_H
#define SERVER_FUNCTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// RPC server IP
#define RPC_SERVER_IP "localhost"


/**
 * @retval -1 if error
 * @retval 0 if user does not exist
 * @retval 1 if user exists
*/
int user_exists(const char *username);


/**
 * @retval -1 if error
 * @retval 0 if user is not connected
 * @retval 1 if user is connected
*/
int is_user_connected(const char *username);


/**
 * @brief This function deletes the connected_users.txt file
 * 
 * @return int error code
 * @retval 0 No error
 * @retval 1 Error
 */
int init_server();

/**
 * @brief This function saves the user's username in the server's users file
 * 
 * @return int error code
 * @retval 0 No error
 * @retval 1 Username already exists
 * @retval 2 Other error
 */
int register_user(char *username);

/**
 * @brief This function removes the user's username from the server's users file
 * 
 * @return int error code
 * @retval 0 No error
 * @retval 1 Username already exists
 * @retval 2 Other error
 */
int unregister_user(char *username);

/**
 * @brief This function connects a user by saving the user's
 * username, IP and port in the server's connected_users file
 * 
 * @return int error code
 * @retval 0 No error
 * @retval 1 User does not exist
 * @retval 2 User already connected
 * @retval 3 Other error
 */
int connect_user(char *username, char *ip, char *port);


/**
 * @brief This function disconnects a user by removing the user's
 * username, IP and port from the server's connected_users file 
 * 
 * @return int error code
 * @retval 0 No error
 * @retval 1 User does not exist
 * @retval 2 User not connected
 * @retval 3 Other error
 */
int disconnect_user(char *username);


/**
 * @brief This function receives a user's username, a filename and a description
 * and saves the file in the server's published_files file
 * 
 * @return int error code
 * @retval 0 No error
 * @retval 1 User does not exist
 * @retval 2 User not connected
 * @retval 3 File already published
 * @retval 4 Other error
 */
int publish(char *username, char *filename, char *description);


/**
 * @brief This function receives a user's username and a filename
 * and removes the file from the server's published_files file
 * 
 * @return int error code
 * @retval 0 No error
 * @retval 1 User does not exist
 * @retval 2 User not connected
 * @retval 3 File not published
 * @retval 4 Other error
 */
int delete(char *username, char *filename);

#endif