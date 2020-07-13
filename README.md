# chat_room
This code is designed for a linux environment.
This is a simple TCP based chat room. Both server and client codes are defined here.
The max clients that the server can support can be defined by modifying MAX_CLIENTS in server.c
The server can support multiple clients at once and uses poll to multiplex between all clients in a single thread.

To build, use GNU Make
```
make
```

Run the server as follows.
```
./chat_room_server <port>
```

To connect to the server, use the client as follows
```
/chat_room_client <server_ip_addr> <server_ip_port> <username>
```
