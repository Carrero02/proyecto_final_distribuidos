# Compiler
CC = gcc

# Paths
SERVER_FUNCTIONS_PATH = server_functions

# Flags
CFLAGS = -g -I/usr/include/tirpc -fPIC -lrt -lpthread
LDLIBS += -lnsl -lpthread -lrt -ltirpc -L. -lserverfunctions
RPCGENFLAGS = -N

# Sources and targets
SOURCES.x = rpc_server/rpc_server.x
TARGETS_SVC.c = rpc_server/rpc_server_svc.c rpc_server/rpc_server.c rpc_server/rpc_server_xdr.c rpc_server/rpc_server_functions/rpc_server_functions.c
TARGETS_CLNT.c = rpc_server/rpc_server_clnt.c server.c rpc_server/rpc_server_xdr.c 
TARGETS = rpc_server/rpc_server.h rpc_server/rpc_server_xdr.c rpc_server/rpc_server_clnt.c rpc_server/rpc_server_svc.c server.c rpc_server.c

# Objects
OBJECTS_CLNT = $(SOURCES_CLNT.c:%.c=%.o) $(TARGETS_CLNT.c:%.c=%.o)
OBJECTS_SVC = $(SOURCES_SVC.c:%.c=%.o) $(TARGETS_SVC.c:%.c=%.o)

# Binaries
CLIENT = server
SERVER = rpc_server_exe
BIN_FILES = server

all : libserverfunctions.so $(CLIENT) $(SERVER)

%.o: %.c
	@echo "Compiling... $<"
	$(CC) $(CFLAGS) -c $< -o $@

libserverfunctions.so: $(SERVER_FUNCTIONS_PATH)/server_functions.c
	$(CC) -fPIC -c -o $(SERVER_FUNCTIONS_PATH)/server_functions.o $<
	$(CC) -shared -fPIC -o $@ $(SERVER_FUNCTIONS_PATH)/server_functions.o

$(TARGETS) : $(SOURCES.x) 
	rpcgen $(RPCGENFLAGS) $(SOURCES.x)
	sed -i 's|#include "rpc_server/rpc_server.h"|#include "rpc_server.h"|' rpc_server/*.c
# Replace the include path in all .c in the rpc_server directory (it is incorrectly generated by rpcgen)

$(OBJECTS_CLNT) : $(SOURCES_CLNT.c) $(SOURCES_CLNT.h) $(TARGETS_CLNT.c) $(TARGETS)

$(OBJECTS_SVC) : $(SOURCES_SVC.c) $(SOURCES_SVC.h) $(TARGETS_SVC.c) $(TARGETS)

$(CLIENT) : $(OBJECTS_CLNT) 
	$(LINK.c) -o $(CLIENT) $(OBJECTS_CLNT) $(LDLIBS)

$(SERVER) : $(OBJECTS_SVC) 
	$(LINK.c) -o $(SERVER) $(OBJECTS_SVC) $(LDLIBS)

clean:
	rm -f $(BIN_FILES) *.out *.o *.so $(SERVER_FUNCTIONS_PATH)/*.o $(SERVER) users.txt connected_users.txt published_files.txt rpc_server/rpc_server.h rpc_server/rpc_server_xdr.c rpc_server/rpc_server_clnt.c rpc_server/rpc_server_svc.c rpc_server/rpc_server_client.c rpc_server/rpc_server_server.c rpc_server/*.o rpc_server/rpc_server_functions/*.o

re:	clean all

.PHONY: all libserverfunctions.so server clean re