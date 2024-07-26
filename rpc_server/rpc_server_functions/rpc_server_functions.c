#include "../rpc_server.h"


void log_operation(const char *username, const char *operation, const char *time) {
    printf("%s %s %s\n", username, operation, time);
}
