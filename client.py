from enum import Enum
import argparse
import socket
import threading
import requests
import errno
import os


def read_line(socket):
    """
    Read characters from the socket until a null character is found.
    
    """
    line = b""
    while True:
        c = socket.recv(1)
        if c == b'\0':
            break
        line += c
    # print("Line: ", line.decode())
    return line.decode()

class client :

    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum) :
        OK = 0
        ERROR = 1
        USER_ERROR = 2
        USER_ERROR_2 = 3
        USER_ERROR_3 = 4

    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1
    _connected_user = None
    _listening = False
    _server_thread = None
    _server_socket = None
    _users_list = []
    _ws_url = "http://localhost:5000/get_time"
    _force_disconnect = False

    # ******************** METHODS *******************


    @staticmethod
    def  register(user) :
        try:
            # Check if the size of the username exceeds 256 bytes
            if len(user.encode()) > 255:    # + 1 for the null character = 256
                return client.RC.USER_ERROR

            # Create a socket object
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)   # AF_INET: Address Family - Internet, SOCK_STREAM: TCP

            # Connect to the server
            s.connect((client._server, client._port))
            
            # Get the time from the web service
            current_time, success = client.get_time()
            if not success:
                print(current_time)
                return client.RC.USER_ERROR
            
            # Send "REGISTER" command to the server
            s.sendall(b"REGISTER\0")

            # Send the current time to the server
            s.sendall((current_time + '\0').encode())

            # Send username to the server
            s.sendall((user + '\0').encode())

            # Receive response from the server
            response = int(read_line(s))

            # Close the connection
            s.shutdown(socket.SHUT_RDWR)
            s.close()

            # Return the appropriate client.RC value
            if response == 0:
                return client.RC.OK
            elif response == 1:
                return client.RC.ERROR
            else:
                return client.RC.USER_ERROR

        except Exception as e:
            print("Exception: " + str(e))
            # Check if the socket 's' was created
            if 's' in locals():
                try:
                    # Try to send an empty data to check if the socket is connected
                    s.send(b'')
                except socket.error:
                    # If the socket is not connected, don't try to shut it down or close it
                    pass
                else:
                    # If the socket is connected, shut it down and close it
                    s.shutdown(socket.SHUT_RDWR)
                    s.close()
            return client.RC.USER_ERROR

   
    @staticmethod
    def  unregister(user) :
        try:
            # Check if the size of the username exceeds 256 bytes
            if len(user.encode()) > 255:
                return client.RC.USER_ERROR

            # Create a socket object
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)


            # Connect to the server
            s.connect((client._server, client._port))

            # Get the time from the web service
            current_time, success = client.get_time()
            if not success:
                print(current_time)
                return client.RC.USER_ERROR

            # Send "UNREGISTER" command to the server
            s.sendall(b"UNREGISTER\0")

            # Send the current time to the server
            s.sendall((current_time + '\0').encode())

            # Send username to the server
            s.sendall((user + '\0').encode())

            # Receive response from the server
            response = int(read_line(s))

            # Close the connection
            s.shutdown(socket.SHUT_RDWR)
            s.close()

            # Return the appropriate client.RC value
            if response == 0:
                return client.RC.OK
            elif response == 1:
                return client.RC.ERROR
            else:
                return client.RC.USER_ERROR

        except Exception as e:
            print("Exception: " + str(e))
            # Check if the socket 's' was created
            if 's' in locals():
                try:
                    # Try to send an empty data to check if the socket is connected
                    s.send(b'')
                except socket.error:
                    # If the socket is not connected, don't try to shut it down or close it
                    pass
                else:
                    # If the socket is connected, shut it down and close it
                    s.shutdown(socket.SHUT_RDWR)
                    s.close()
            return client.RC.USER_ERROR


    @staticmethod
    def  connect(user) :
        try:
            # Check if the size of the username exceeds 256 bytes
            if len(user.encode()) > 255:
                return client.RC.USER_ERROR_2
            
            # Check if a different user is already connected, disconnect it and connect the new user
            if client._connected_user is not None and client._connected_user != user:
                client.disconnect(client._connected_user)

            # print("Creating server socket")
            # Create a socket object to listen for messages from the server
            client._server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            client._server_socket.bind(('localhost', 0)) # Bind to a random port            

            # print("Starting server thread")
            # Create a thread to receive messages from the server
            client._server_thread = threading.Thread(target=client.serve_files)
            client._server_thread.start()

            # Create a socket object
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

            # Connect to the server
            s.connect((client._server, client._port))

            # Get the time from the web service
            current_time, success = client.get_time()
            if not success:
                print(current_time)
                # Wait for the server thread to start
                while client._listening == False:
                    pass

                # print("Stopping server thread")
                # Stop the server thread
                client._listening = False
                client._server_thread.join()
                client._server_thread = None
                # print("Server thread stopped")

                # Shutdown and close the server socket
                client._server_socket.shutdown(socket.SHUT_RDWR)
                client._server_socket.close()
                client._server_socket = None
                # print("Server socket closed")
                return client.RC.USER_ERROR_2

            # Send "CONNECT" command to the server
            s.sendall(b"CONNECT\0")

            # Send the current time to the server
            s.sendall((current_time + '\0').encode())

            # Send username to the server
            s.sendall((user + '\0').encode())

            # Send the port at which the client will listen for messages
            # getsockname() returns (ip_address, port)
            s.sendall((str(client._server_socket.getsockname()[1]) + '\0').encode())

            # Receive response from the server
            response = int(read_line(s))

            # Close the connection
            s.shutdown(socket.SHUT_RDWR)
            s.close()

            # Return the appropriate client.RC value
            if response == 0:
                client._connected_user = user
                return client.RC.OK
            else:
                # If the connection was not successful, stop the server thread
                # and close the socket

                # print(client._listening)
                # Wait for the server thread to start
                while client._listening == False:
                    pass

                # print("Stopping server thread")
                # Stop the server thread
                client._listening = False
                client._server_thread.join()
                client._server_thread = None
                # print("Server thread stopped")

                # Shutdown and close the server socket
                client._server_socket.shutdown(socket.SHUT_RDWR)
                client._server_socket.close()
                client._server_socket = None
                # print("Server socket closed")

                if response == 1:
                    return client.RC.ERROR
                elif response == 2:
                    # That user is already connected in the server file
                    # We need to make sure it is also connected in the client side
                    client._connected_user = user
                    return client.RC.USER_ERROR
                else:
                    return client.RC.USER_ERROR_2

        except Exception as e:
            print("Exception: " + str(e))
            # Check if the socket 's' was created
            if 's' in locals():
                try:
                    # Try to send an empty data to check if the socket is connected
                    s.send(b'')
                except socket.error:
                    # If the socket is not connected, don't try to shut it down or close it
                    pass
                else:
                    # If the socket is connected, shut it down and close it
                    s.shutdown(socket.SHUT_RDWR)
                    s.close()

            # Close the server thread
            client._listening = False
            if client._server_thread is not None:
                client._server_thread.join()
                client._server_thread = None
            # Close the server socket
            if client._server_socket is not None:
                client._server_socket.shutdown(socket.SHUT_RDWR)
                client._server_socket.close()
                client._server_socket = None
            # Reset the connected user
            client._connected_user = None

            return client.RC.USER_ERROR_2


    @staticmethod
    def  disconnect(user) :
        try:
            # Check if the size of the username exceeds 256 bytes
            if len(user.encode()) > 255:
                return client.RC.USER_ERROR_2
            
            # Check if the user is not connected
            if client._connected_user != user:
                return client.RC.USER_ERROR

            # Create a socket object
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

            # Connect to the server
            s.connect((client._server, client._port))

            # Get the time from the web service
            current_time, success = client.get_time()
            if not success:
                print(current_time)
                if not client._force_disconnect:
                    return client.RC.USER_ERROR_2
                else:
                    current_time = "01/01/1970 00:00:00"

            # Send "DISCONNECT" command to the server
            s.sendall(b"DISCONNECT\0")

            # Send the current time to the server
            s.sendall((current_time + '\0').encode())

            # Send username to the server
            s.sendall((user + '\0').encode())

            # Receive response from the server
            response = int(read_line(s))

            # Close the connection
            s.shutdown(socket.SHUT_RDWR)
            s.close()

            # Return the appropriate client.RC value
            if response == 0 or client._force_disconnect:
                # Close the server thread
                client._listening = False
                if client._server_thread is not None:
                    client._server_thread.join()
                    client._server_thread = None
                # Close the server socket
                if client._server_socket is not None:
                    client._server_socket.shutdown(socket.SHUT_RDWR)
                    client._server_socket.close()
                    client._server_socket = None
                # Reset the connected user
                client._connected_user = None
                return client.RC.OK
            elif response == 1:
                return client.RC.ERROR
            elif response == 2:
                # In this case, the _connected_user is not connected in the server file, 
                # so we reset it to None in case it was not reset
                # and also stop the server thread and close the server socket
                # This ensures the consistency between the server and the client

                # Close the server thread
                client._listening = False
                if client._server_thread is not None:
                    client._server_thread.join()
                    client._server_thread = None
                # Close the server socket
                if client._server_socket is not None:
                    client._server_socket.shutdown(socket.SHUT_RDWR)
                    client._server_socket.close()
                    client._server_socket = None
                # Reset the connected user
                client._connected_user = None

                return client.RC.USER_ERROR
            else:
                return client.RC.USER_ERROR_2

        except Exception as e:
            print("Exception: " + str(e))
            # Check if the socket 's' was created
            if 's' in locals():
                try:
                    # Try to send an empty data to check if the socket is connected
                    s.send(b'')
                except socket.error:
                    # If the socket is not connected, don't try to shut it down or close it
                    pass
                else:
                    # If the socket is connected, shut it down and close it
                    s.shutdown(socket.SHUT_RDWR)
                    s.close()

            # If the server is down
            if e.errno == errno.ECONNREFUSED:
                # In this case, we stop the server thread and close the server socket
                # because the server is not running, so the user is not connected
                # Note that each time the server is launched, the connected users are reset
                client._listening = False
                if client._server_thread is not None:
                    client._server_thread.join()
                    client._server_thread = None
                # Close the server socket
                if client._server_socket is not None:
                    client._server_socket.shutdown(socket.SHUT_RDWR)
                    client._server_socket.close()
                    client._server_socket = None
                # Reset the connected user
                client._connected_user = None
                return client.RC.OK
            return client.RC.USER_ERROR_2
        

    @staticmethod
    def  publish(fileName,  description) :
        try:
            # Check if there is someone connected
            if client._connected_user == None:
                return client.RC.USER_ERROR
            
            # Check if the size of the file name exceeds 256 bytes
            if len(fileName.encode()) > 255:
                return client.RC.USER_ERROR_3

            # Check if the size of the description exceeds 256 bytes
            if len(description.encode()) > 255:
                return client.RC.USER_ERROR_3

            # Create a socket object
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

            # Connect to the server
            s.connect((client._server, client._port))

            # Get the time from the web service
            current_time, success = client.get_time()
            if not success:
                print(current_time)
                return client.RC.USER_ERROR_3

            # Send "PUBLISH" command to the server
            s.sendall(b"PUBLISH\0")

            # Send the current time to the server
            s.sendall((current_time + '\0').encode())

            # Send username to the server
            s.sendall((client._connected_user + '\0').encode())

            # Send filename to the server
            s.sendall((fileName + '\0').encode())

            # Send description to the server
            s.sendall((description + '\0').encode())

            # Receive response from the server
            response = int(read_line(s))

            # Close the connection
            s.shutdown(socket.SHUT_RDWR)
            s.close()

            # Return the appropriate client.RC value
            if response == 0:
                return client.RC.OK
            elif response == 1:
                return client.RC.ERROR
            elif response == 2:
                # In this case, the _connected_user is not connected in the server file, 
                # so we reset it to None in case it was not reset
                # and also stop the server thread and close the server socket
                # This ensures the consistency between the server and the client

                # Close the server thread
                client._listening = False
                if client._server_thread is not None:
                    client._server_thread.join()
                    client._server_thread = None
                # Close the server socket
                if client._server_socket is not None:
                    client._server_socket.shutdown(socket.SHUT_RDWR)
                    client._server_socket.close()
                    client._server_socket = None
                # Reset the connected user
                client._connected_user = None
                
                return client.RC.USER_ERROR
            elif response == 3:
                return client.RC.USER_ERROR_2
            else:
                return client.RC.USER_ERROR_3

        except Exception as e:
            print("Exception: " + str(e))
            # Check if the socket 's' was created
            if 's' in locals():
                try:
                    # Try to send an empty data to check if the socket is connected
                    s.send(b'')
                except socket.error:
                    # If the socket is not connected, don't try to shut it down or close it
                    pass
                else:
                    # If the socket is connected, shut it down and close it
                    s.shutdown(socket.SHUT_RDWR)
                    s.close()
            return client.RC.USER_ERROR_3

    @staticmethod
    def  delete(fileName) :
        try:
            # Check if there is someone connected
            if client._connected_user == None:
                return client.RC.USER_ERROR
                        
            # Check if the size of the file name exceeds 256 bytes
            if len(fileName.encode()) > 255:
                return client.RC.USER_ERROR_3
            
            # Create a socket object
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

            # Connect to the server
            s.connect((client._server, client._port))

            # Get the time from the web service
            current_time, success = client.get_time()
            if not success:
                print(current_time)
                return client.RC.USER_ERROR_3

            # Send "DELETE" command to the server
            s.sendall(b"DELETE\0")

            # Send the current time to the server
            s.sendall((current_time + '\0').encode())

            # Send username to the server
            s.sendall((client._connected_user + '\0').encode())

            # Send filename to the server
            s.sendall((fileName + '\0').encode())

            # Receive response from the server
            response = int(read_line(s))

            # Close the connection
            s.shutdown(socket.SHUT_RDWR)
            s.close()

            # Return the appropriate client.RC value
            if response == 0:
                return client.RC.OK
            elif response == 1:
                return client.RC.ERROR
            elif response == 2:
                # In this case, the _connected_user is not connected in the server file,
                # so we reset it to None in case it was not reset
                # and also stop the server thread and close the server socket
                # This ensures the consistency between the server and the client

                # Close the server thread
                client._listening = False
                if client._server_thread is not None:
                    client._server_thread.join()
                    client._server_thread = None
                # Close the server socket
                if client._server_socket is not None:
                    client._server_socket.shutdown(socket.SHUT_RDWR)
                    client._server_socket.close()
                    client._server_socket = None
                # Reset the connected user
                client._connected_user = None

                return client.RC.USER_ERROR
            elif response == 3:
                return client.RC.USER_ERROR_2
            else:
                return client.RC.USER_ERROR_3
            
        except Exception as e:
            print("Exception: " + str(e))
            # Check if the socket 's' was created
            if 's' in locals():
                try:
                    # Try to send an empty data to check if the socket is connected
                    s.send(b'')
                except socket.error:
                    # If the socket is not connected, don't try to shut it down or close it
                    pass
                else:
                    # If the socket is connected, shut it down and close it
                    s.shutdown(socket.SHUT_RDWR)
                    s.close()
            return client.RC.USER_ERROR_3    


    @staticmethod
    def  listusers() :
        try:
            # Check if there is someone connected
            if client._connected_user == None:
                return client.RC.USER_ERROR
            
            # Create a socket object
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

            # Connect to the server
            s.connect((client._server, client._port))

            # Get the time from the web service
            current_time, success = client.get_time()
            if not success:
                print(current_time)
                return client.RC.USER_ERROR_2

            # Send "LIST_USERS" command to the server
            s.sendall(b"LIST_USERS\0")

            # Send the current time to the server
            s.sendall((current_time + '\0').encode())

            # Send username to the server
            s.sendall((client._connected_user + '\0').encode())

            # Receive response from the server
            response = int(read_line(s))
            if response == 0:
                print("c> LIST_USERS OK")
                # Reset the users list
                client._users_list = []
                # The second response is the number of users that the server will send
                num_users = int(read_line(s))

                # Receive <num_users> times 3 responses: username, IP and port
                for i in range(num_users):
                    # Receive username
                    username = read_line(s)
                    # Receive IP
                    ip = read_line(s)
                    # Receive port
                    port = read_line(s)
                    print("\t%s %s %s" % (username, ip, port))

                    # Add the user to the list
                    client._users_list.append((username, ip, port))
                
                # Close the connection
                s.shutdown(socket.SHUT_RDWR)
                s.close()
                return client.RC.OK

            elif response == 1:
                return client.RC.ERROR
            elif response == 2:
                # In this case, the _connected_user is not connected in the server file,
                # so we reset it to None in case it was not reset
                # and also stop the server thread and close the server socket
                # This ensures the consistency between the server and the client

                # Close the server thread
                client._listening = False
                if client._server_thread is not None:
                    client._server_thread.join()
                    client._server_thread = None
                # Close the server socket
                if client._server_socket is not None:
                    client._server_socket.shutdown(socket.SHUT_RDWR)
                    client._server_socket.close()
                    client._server_socket = None
                # Reset the connected user
                client._connected_user = None

                return client.RC.USER_ERROR
            else:
                return client.RC.USER_ERROR_2
    
        except Exception as e:
            print("Exception: " + str(e))
            # Check if the socket 's' was created
            if 's' in locals():
                try:
                    # Try to send an empty data to check if the socket is connected
                    s.send(b'')
                except socket.error:
                    # If the socket is not connected, don't try to shut it down or close it
                    pass
                else:
                    # If the socket is connected, shut it down and close it
                    s.shutdown(socket.SHUT_RDWR)
                    s.close()
            return client.RC.USER_ERROR_2

    @staticmethod
    def  listcontent(user) :
        try:
            # Check if there is someone connected
            if client._connected_user == None:
                return client.RC.USER_ERROR
            
            # Check if the size of the username exceeds 256 bytes
            if len(user.encode()) > 255:
                return client.RC.USER_ERROR_2
            
            # Create a socket object
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

            # Connect to the server
            s.connect((client._server, client._port))

            # Get the time from the web service
            current_time, success = client.get_time()
            if not success:
                print(current_time)
                return client.RC.USER_ERROR_2

            # Send "LIST_CONTENT" command to the server
            s.sendall(b"LIST_CONTENT\0")

            # Send the current time to the server
            s.sendall((current_time + '\0').encode())

            # Send the username of the client performing the request
            s.sendall((client._connected_user + '\0').encode())

            # Send the username of the client whose content we want to list
            s.sendall((user + '\0').encode())

            # Receive response from the server
            response = int(read_line(s))
            if response == 0:
                print("c> LIST_CONTENT OK")
                # The second response is the number of files that the server will send
                num_files = int(read_line(s))

                # Receive <num_files> times 2 responses: filename and description
                for i in range(num_files):
                    # Receive filename
                    filename = read_line(s)
                    # Receive description
                    description = read_line(s)
                    print("\t%s %s" % (filename, description))
                
                # Close the connection
                s.shutdown(socket.SHUT_RDWR)
                s.close()
                return client.RC.OK

            elif response == 1:
                return client.RC.ERROR
            elif response == 2:
                # In this case, the _connected_user is not connected in the server file,
                # so we reset it to None in case it was not reset
                # and also stop the server thread and close the server socket
                # This ensures the consistency between the server and the client

                # Close the server thread
                client._listening = False
                if client._server_thread is not None:
                    client._server_thread.join()
                    client._server_thread = None
                # Close the server socket
                if client._server_socket is not None:
                    client._server_socket.shutdown(socket.SHUT_RDWR)
                    client._server_socket.close()
                    client._server_socket = None
                # Reset the connected user
                client._connected_user = None

                return client.RC.USER_ERROR
            else:
                return client.RC.USER_ERROR_2

        except Exception as e:
            print("Exception: " + str(e))
            # Check if the socket 's' was created
            if 's' in locals():
                try:
                    # Try to send an empty data to check if the socket is connected
                    s.send(b'')
                except socket.error:
                    # If the socket is not connected, don't try to shut it down or close it
                    pass
                else:
                    # If the socket is connected, shut it down and close it
                    s.shutdown(socket.SHUT_RDWR)
                    s.close()
            return client.RC.USER_ERROR_3

    @staticmethod
    def  getfile(user,  remote_FileName,  local_FileName) :
        try:
            # Check if there is someone connected
            if client._connected_user == None:
                return client.RC.USER_ERROR
            
            # Check if the size of the username exceeds 256 bytes
            if len(user.encode()) > 255:
                return client.RC.USER_ERROR
            
            # Check if the size of the remote file name exceeds 256 bytes
            if len(remote_FileName.encode()) > 255:
                return client.RC.USER_ERROR
            
            # Check if the size of the local file name exceeds 256 bytes
            if len(local_FileName.encode()) > 255:
                return client.RC.USER_ERROR
            
            # Create a socket object to connect to the other client
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

            # Set a timeout of 5 seconds for the connection
            s.settimeout(5.0)

            # Get the IP and port of the other client
            found = False
            for user_info in client._users_list:
                if user_info[0] == user:
                    found = True
                    ip = user_info[1]
                    port = int(user_info[2])
                    break
            if not found:
                return client.RC.USER_ERROR

            # Connect to the other client
            s.connect((ip, port))

            # Send "GET_FILE" command to the other client
            s.sendall(b"GET_FILE\0")

            # Send the remote file name to the other client
            s.sendall((remote_FileName + '\0').encode())

            # Receive response from the other client
            # The first response is the oparation error code
            response = int(read_line(s))
            # print("Response: ", response)
            if response == 0:
                # The following responses are the file bytes
                with open(local_FileName, 'wb') as f:
                    try:
                        while True:
                            chunk = s.recv(1024)
                            # print("Received chunk: ", chunk.decode())
                            # Empty chunk when the other client closes the connection
                            if chunk == b'':
                                break
                            f.write(chunk)
                    except Exception as e:
                        f.close()
                        # Remove the file if an error occurs
                        os.remove(local_FileName)
                        # Re-raise the exception to be caught by the outer except block
                        raise e

                # Close the connection
                s.shutdown(socket.SHUT_RDWR)
                s.close()
                return client.RC.OK
            elif response == 1:
                return client.RC.ERROR
            else:
                return client.RC.USER_ERROR

        except Exception as e:
            print("Exception: " + str(e))
            # Check if the socket 's' was created
            if 's' in locals():
                try:
                    # Try to send an empty data to check if the socket is connected
                    s.send(b'')
                except socket.error:
                    # If the socket is not connected, don't try to shut it down or close it
                    pass
                else:
                    # If the socket is connected, shut it down and close it
                    s.shutdown(socket.SHUT_RDWR)
                    s.close()
            return client.RC.USER_ERROR
    
    @staticmethod
    def serve_files():
        """
        Function that will run in a thread to listen for incoming connections from other clients
        """
        # print("Server thread started")
        client._listening = True

        # Set a timeout for the accept() function
        client._server_socket.settimeout(1.0)  # 1 second timeout
        
        # print("Server listening...")
        while client._listening:
            try:
                # Listen for incoming connections
                client._server_socket.listen(10)  # maximum 10 connections in the queue
                # Accept the connection
                conn, addr = client._server_socket.accept()
                # print("Connection accepted")
                # Receive the command from the client
                command = read_line(conn)
                # print("Command: ", command)
                if command == "GET_FILE":
                    # Receive the file name
                    fileName = read_line(conn)
                    # print("File name: ", fileName)
                    try:
                        # Open the file
                        with open(fileName, 'rb') as f:
                            # Send a 0 error code to the client
                            conn.sendall(b"0\0")

                            # Send the file bytes to the client
                            while True:
                                chunk = f.read(1024)
                                # print("Sending chunk: ", chunk.decode())
                                if chunk == b'':    # EOF
                                    break
                                conn.sendall(chunk)
                    except FileNotFoundError:
                        # If the file does not exist, send an error code
                        # print("File not found")
                        conn.sendall(b"1\0")
                # Close the connection
                conn.shutdown(socket.SHUT_RDWR)
                conn.close()
            except socket.timeout:
                # No client connected during the timeout,
                # so run again the loop
                # print("Timeout")
                pass
            except Exception as e:
                print("Exception: " + str(e))
                try:
                    conn.sendall(b"2\0")
                    conn.shutdown(socket.SHUT_RDWR)
                    conn.close()
                except socket.error:
                    print("Failed to send error code: connection was already closed")
                    pass
                break
        # print("Server thread stopped")

    @staticmethod
    def  get_time() :
        # Call the web service to get the current time
        try:
            response = requests.get(client._ws_url)
            if response.status_code == 200:
                return response.text, True
            else:
                return "Error " + str(response.status_code) + ": " + response.text, False
        except Exception as e:
            return "Error: " + str(e), False

    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():

        while (True) :
            try :
                command = input("c> ")
                line = command.split(" ")
                if (len(line) > 0):

                    line[0] = line[0].upper()

                    if (line[0]=="REGISTER") :
                        if (len(line) == 2) :
                            result = client.register(line[1])
                            if result == client.RC.OK:
                                print("c> REGISTER OK")
                            elif result == client.RC.ERROR:
                                print("c> USERNAME IN USE")
                            else:
                                print("c> REGISTER FAIL")
                        else :
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif(line[0]=="UNREGISTER") :
                        if (len(line) == 2) :
                            result = client.unregister(line[1])
                            if result == client.RC.OK:
                                print("c> UNREGISTER OK")
                            elif result == client.RC.ERROR:
                                print("c> USER DOES NOT EXIST")
                            else:
                                print("c> UNREGISTER FAIL")
                        else :
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif(line[0]=="CONNECT") :
                        if (len(line) == 2) :
                            result = client.connect(line[1])
                            if result == client.RC.OK:
                                print("c> CONNECT OK")
                            elif result == client.RC.ERROR:
                                print("c> CONNECT FAIL, USER DOES NOT EXIST")
                            elif result == client.RC.USER_ERROR:
                                print("c> USER ALREADY CONNECTED")
                            else:
                                print("c> CONNECT FAIL")
                        else :
                            print("Syntax error. Usage: CONNECT <userName>")
                    
                    elif(line[0]=="PUBLISH") :
                        if (len(line) >= 3) :
                            #  Remove first two words
                            description = ' '.join(line[2:])
                            result = client.publish(line[1], description)
                            if result == client.RC.OK:
                                print("c> PUBLISH OK")
                            elif result == client.RC.ERROR:
                                print("c> PUBLISH FAIL, USER DOES NOT EXIST")
                            elif result == client.RC.USER_ERROR:
                                print("c> PUBLISH FAIL, USER NOT CONNECTED")
                            elif result == client.RC.USER_ERROR_2:
                                print("c> PUBLISH FAIL, CONTENT ALREADY PUBLISHED")
                            else:
                                print("c> PUBLISH FAIL")
                        else :
                            print("Syntax error. Usage: PUBLISH <fileName> <description>")

                    elif(line[0]=="DELETE") :
                        if (len(line) == 2) :
                            result = client.delete(line[1])
                            if result == client.RC.OK:
                                print("c> DELETE OK")
                            elif result == client.RC.ERROR:
                                print("c> DELETE FAIL, USER DOES NOT EXIST")
                            elif result == client.RC.USER_ERROR:
                                print("c> DELETE FAIL, USER NOT CONNECTED")
                            elif result == client.RC.USER_ERROR_2:
                                print("c> DELETE FAIL, CONTENT NOT PUBLISHED")
                            else:
                                print("c> DELETE FAIL")
                        else :
                            print("Syntax error. Usage: DELETE <fileName>")

                    elif(line[0]=="LIST_USERS") :
                        if (len(line) == 1) :
                            result = client.listusers()
                            if result == client.RC.ERROR:
                                print("c> LIST_USERS FAIL, USER DOES NOT EXIST")
                            elif result == client.RC.USER_ERROR:
                                print("c> LIST_USERS FAIL, USER NOT CONNECTED")
                            elif result == client.RC.USER_ERROR_2:
                                print("c> LIST_USERS FAIL")

                        else :
                            print("Syntax error. Use: LIST_USERS")

                    elif(line[0]=="LIST_CONTENT") :
                        if (len(line) == 2) :
                            result = client.listcontent(line[1])
                            if result == client.RC.ERROR:
                                print("c> LIST_CONTENT FAIL, USER DOES NOT EXIST")
                            elif result == client.RC.USER_ERROR:
                                print("c> LIST_CONTENT FAIL, USER NOT CONNECTED")
                            elif result == client.RC.USER_ERROR_2:
                                print("c> LIST_CONTENT FAIL, REMOTE USER DOES NOT EXIST")
                            elif result == client.RC.USER_ERROR_3:
                                print("c> LIST_CONTENT FAIL")
                        else :
                            print("Syntax error. Usage: LIST_CONTENT <userName>")

                    elif(line[0]=="DISCONNECT") :
                        if (len(line) == 2) :
                            result = client.disconnect(line[1])
                            if result == client.RC.OK:
                                print("c> DISCONNECT OK")
                            elif result == client.RC.ERROR:
                                print("c> DISCONNECT FAIL / USER DOES NOT EXIST")
                            elif result == client.RC.USER_ERROR:
                                print("c> DISCONNECT FAIL / USER NOT CONNECTED")
                            else:
                                print("c> DISCONNECT FAIL")
                        else :
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif(line[0]=="GET_FILE") :
                        if (len(line) == 4) :
                            result = client.getfile(line[1], line[2], line[3])
                            if result == client.RC.OK:
                                print("c> GET_FILE OK")
                            elif result == client.RC.ERROR:
                                print("c> GET_FILE FAIL / FILE NOT EXIST")
                            else:
                                print("c> GET_FILE FAIL")
                        else :
                            print("Syntax error. Usage: GET_FILE <userName> <remote_fileName> <local_fileName>")

                    elif(line[0]=="QUIT") :
                        if (len(line) == 1) :
                            # Disconnect the user if it is connected
                            if client._connected_user is not None:
                                client._force_disconnect = True
                                client.disconnect(client._connected_user)
                            break
                        else :
                            print("Syntax error. Use: QUIT")
                    else :
                        print("Error: command " + line[0] + " not valid.")
            except Exception as e:
                print("Exception: " + str(e))

    # *
    # * @brief Prints program usage
    @staticmethod
    def usage() :
        print("Usage: python3 client.py -s <server> -p <port>")


    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def  parseArguments(argv) :
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if (args.s is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535")
            return False
        
        client._server = args.s
        client._port = args.p

        return True


    # ******************** MAIN *********************
    @staticmethod
    def main(argv) :
        try:
            if (not client.parseArguments(argv)) :
                client.usage()
                return

            #  Write code here
            client.shell()
            print("+++ FINISHED +++")
        except KeyboardInterrupt:
            if client._connected_user is not None:
                client._force_disconnect = True
                client.disconnect(client._connected_user)

            print("\n+++ FINISHED +++")
    

if __name__=="__main__":
    client.main([])