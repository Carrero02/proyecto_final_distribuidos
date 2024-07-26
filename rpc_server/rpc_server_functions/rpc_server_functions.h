// Functions for the RPC server

#include <pthread.h>

/**
 * @brief This function prints the operation logs to the standard output
 * <username> <operation> <time>
*/
void log_operation(const char *username, const char *operation, const char *time);
