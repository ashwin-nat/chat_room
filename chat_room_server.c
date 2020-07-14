#include "chat_room_server.h"
#include "utils.h"
#include <stdlib.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
/***********************************************************************************************/
#define POLL_PERIOD     1500
#define DFL_USERNAME    "user##"
/***********************************************************************************************/
//private structure
struct chat_room_list {
    int id;
    int fd;
    char username[USER_NAME_MAX_LEN];
    char ipaddr[IPv4_ADD_STR_LEN];
    bool first;
};
/***********************************************************************************************/
//static variable definitions
static int pfds_count = 0;
struct pollfd *pfds = NULL;
static int server_fd=0;
static int max_fds = 0;
static int buffer_size = 0;
static int curr_uid=0;
static struct chat_room_list *client_list = NULL;
static char chat_room_name[ROOM_NAME_MAX_LEN] = ROOM_NAME_DEFAULT;
/***********************************************************************************************/
static int chat_room_poll (void);
static bool add_fd (int fd, struct sockaddr_in *cliaddr);
static bool remove_fd (int fd);
static void process_client (int client_fd);
static void process_fd (int fd);
static bool is_command (char *buffer, int bytes);
static void process_command (int client_fd, char *buffer, int bytes);
static int fd_to_client_list_index (int client_fd);
static int username_to_client_list_index (char *username);
static void send_to_other_clients (int client_fd, char *send_buffer, int bytes);
static void send_to_all_clients (char *send_buffer, int bytes);
static void _cmd_setuname (int client_fd, char *username);
static void _cmd_set_room_name (int client_fd, char *room_name);
static void _cmd_user_list (int client_fd);
static void _cmd_send_whisper (int client_fd, char *buffer, int len);
static char* _cmd_find_next_arg (char *buffer, int len);
/***********************************************************************************************/
//allocates memory for the pollfd structure
//sets the max clients limit
int init_chat_room_server (int max_clients, int buff_size)
{
    //1 extra fd for the server fd
    max_fds = max_clients + 1;
    pfds = malloc (max_fds * sizeof(*pfds));
    if(pfds == NULL) {
        perror("malloc");
        return -1;
    }
    buffer_size = buff_size;
    client_list = calloc (max_clients, sizeof(*client_list));
    printf("Max Clients = %d\n", max_clients);
    return 0;
}
//sets the fd for the server
//runs the inifite loop for the server
void chat_room_server_loop (int server_fd_arg)
{
    //set the global server_fd variable
    server_fd = server_fd_arg;
    //add the server_fd to the list of fd's to poll on
    add_fd (server_fd, NULL);
    int i,count,rc;
    while(1) {
        rc = chat_room_poll();
        if(rc > 0) {
            count=0;
            //some fd is ready
            for(i=0; i<pfds_count; i++) {
                //check if this is the fd ready for reading
                if(pfds[i].revents & POLLIN) {
                    process_fd(pfds[i].fd);
                    count++;
                    if(count == rc) {
                        break;
                    }
                }//end of if(pfds[i].revents & POLLIN)
            }//end of for loop
        }//end of if(rc > 0)
    }//end of while(1)
}
/***********************************************************************************************/
static int chat_room_poll (void)
{
    if(pfds_count == 0) {
        return 0;
    }
    int ret = poll (pfds, pfds_count, POLL_PERIOD);
    if(ret == SOCK_ERR) {
        perror("poll");
        exit(1);
    }
    return ret;
}
/*<============================================================>*/
//add an fd to the pfds array
static bool add_fd (int fd, struct sockaddr_in *cliaddr)
{
    //check if there is room for the new client
    if(pfds_count < (max_fds)) {
        //register fd for polling
        pfds[pfds_count].fd = fd;
        pfds[pfds_count].events = POLLIN;
        //the first call to add_fd will be for server_fd, we will not register in client_list
        if(pfds_count > 0) {
            //register in client list - we use pfds_count-1 because
            //pfds[0] is the server_fd and there will not be an entry for this fd in the client_list
            memset (&client_list[pfds_count-1].id, 0, sizeof(*client_list));
            client_list[pfds_count-1].id = curr_uid;
            client_list[pfds_count-1].fd = fd;
            snprintf(client_list[pfds_count-1].username, USER_NAME_MAX_LEN-1, DFL_USERNAME"%d", curr_uid);
            //parse the ip addr
            if(inet_ntop(AF_INET, &(cliaddr->sin_addr), client_list[pfds_count-1].ipaddr, IPv4_ADD_STR_LEN-1 ) == NULL) {
                perror("inet_ntop");
            }

            //send a welcome message
            char str[100] = {0,};
            int bytes = snprintf(str, sizeof(str), "Welcome to %s.\n\tCurr users = %d\n\tMax Users = %d\n"
                                        "\tYour UID = %d\n" PROMPT_TEXT, chat_room_name,
                                    (pfds_count), (max_fds-1), (curr_uid) );
            chat_room_write(fd, str, bytes);

            //increment counters
            curr_uid++;
            printf("client %s connected\n", client_list[pfds_count-1].ipaddr);
        }
        pfds_count++;
        return true;
    }
    //the server is full, reject the client
    else {
        printf("Currently at max clients, rejecting this client\n");
        char *str = KRED "[server]Chat room is full\n" KNRM;
        chat_room_write(fd, str, strlen(str));
        close (fd);
        return false;
    }
}
/*<============================================================>*/
//remove an fd from the pfds array
static bool remove_fd (int fd)
{
    int i;
    //linearly search for the fd in the array
    for(i=0; i<pfds_count; i++) {
        //found the fd
        if(pfds[i].fd == fd) {
            break;
        }
    }

    //if fd is found
    if(i < pfds_count) {
        //do not allow removal of server_fd
        if(pfds[i].fd == server_fd) {
            return false;
        }
        else {
            //inform the other users that this one has left
            printf("client %s disconnected\n", client_list[i-1].username);
            size_t size = 70;
            char *str = calloc (size, 1);
            int bytes = snprintf(str, size-1, CLEAR_LINE KGRN "[server] %s has left the chat" KNRM CRLF PROMPT_TEXT,
                                    client_list[i-1].username);
            send_to_other_clients(fd, str, bytes);
            free(str);

            //copy the last fd to this location
            pfds[i].fd = pfds[pfds_count-1].fd;
            //copy last item in client list to this location
            memcpy (&client_list[i-1], &client_list[pfds_count-2], sizeof(*client_list));

            //clear the last item
            memset (&pfds[pfds_count-1], 0, sizeof(*pfds));
            memset (&client_list[pfds_count-2], 0, sizeof(*client_list));
            //decrement counter
            pfds_count--;
            return true;
        }
    }
    else {
        return false;
    }
}
/*<============================================================>*/
//just echo for now
static void process_client (int client_fd)
{
    char *recv_buffer = calloc (buffer_size, 1);
    int bytes = chat_room_read(client_fd, recv_buffer, buffer_size);
    //printf("fd = %d bytes: %d\n", client_fd, bytes);
    if(bytes > 1) {
        bytes = chat_room_remove_crlf(recv_buffer, bytes);
        //the message was just an enter keystroke, no need to send it to the clients
        if(bytes == 0) {
                ;
        }
        else {
            //check if it is a command
            if(is_command (recv_buffer, bytes)) {
                process_command (client_fd, recv_buffer, bytes);
            }
            else {
                //send the message to all clients other than this one
                int i = fd_to_client_list_index(client_fd);
                if(i != SOCK_ERR) {
                    //size_t size = bytes + USER_NAME_MAX_LEN + 4 + 1;
                    size_t size = bytes + 70;
                    bytes = 0;
                    //4 extra bytes: 1 for [, 1 for ], 1 for :, 1 for space
                    char *send_buffer = calloc (size, 1);
                    if(client_list[i].first == false) {
                        //send new join message
                        bytes += snprintf(send_buffer+bytes, (size-1-bytes),
                                    CR KGRN "[server]: user %s has joined\n" KNRM, client_list[i].username);
                    }
                    //add username to message
                    bytes += snprintf(send_buffer+bytes, (size-1-bytes),
                                        CLEAR_LINE "[%s] %s" CRLF PROMPT_TEXT, client_list[i].username, recv_buffer);
                    //bytes = chat_room_insert_crlf(send_buffer, bytes);
                    send_to_other_clients (client_fd, send_buffer, bytes);
                    //send prompt text to that client
                    chat_room_write(client_fd, CR PROMPT_TEXT, strlen (CR) + strlen(PROMPT_TEXT));
                    free(send_buffer);
                    client_list[i].first = true;
                }
            }
        }
    }
    //we treat failed reads as disconnected
    else {
        remove_fd (client_fd);
    }
    free(recv_buffer);
}
/*<============================================================>*/
//process the fd that is ready for reading
static void process_fd (int fd)
{
    //if the fd is the server_fd, then accept the connection
    if(fd == server_fd) {
        struct sockaddr_in cliaddr = {0,};
        int client_fd = accept_connections(fd, &cliaddr);
        if(client_fd != SOCK_ERR) {
            add_fd (client_fd, &cliaddr);
        }
    }
    //else, some client has sent some data
    else {
        process_client (fd);
    }
}
/*<============================================================>*/
//check if this recvd message is a command
static bool is_command (char *buffer, int bytes)
{
    int len = strlen (CMD_ESC_SEQ);
    if(bytes <= strlen (CMD_ESC_SEQ)) {
        return false;
    }
    else {
        if(strncmp(buffer, CMD_ESC_SEQ, len) == 0) {
            return true;
        }
    }
    return false;
}
/*<============================================================>*/
//process the cmd
static void process_command (int client_fd, char *buffer, int bytes)
{
    int len = strlen (CMD_ESC_SEQ);
    //offset the buffer by the length of the escape sequence
    char *cmd = buffer + len;
    if(strncmp(cmd, CMD_SET_USERNAME, len) == 0) {
        //char *username = cmd + strlen (CMD_SET_USERNAME);
        char *username = _cmd_find_next_arg (cmd, (bytes-len));
        if(username) {_cmd_setuname (client_fd, username);}
    }
    else if(strncmp(cmd, CMD_QUIT, strlen(CMD_QUIT)) == 0) {
        remove_fd(client_fd);
        close(client_fd);
    }
    else if(strncmp(cmd, CMD_SET_ROOM_NAME, strlen(CMD_SET_ROOM_NAME)) == 0) {
        //char *room_name = cmd + strlen (CMD_SET_ROOM_NAME);
        char *room_name = _cmd_find_next_arg(cmd, (bytes-len));
        if(room_name) {_cmd_set_room_name (client_fd, room_name);}
    }
    else if(strncmp(cmd, CMD_SHUTDOWN, strlen(CMD_SHUTDOWN)) == 0) {
        char *str = KRED "[server] The server is shutting down now\n" KNRM;
        send_to_all_clients (str, strlen(str));
        exit (0);
    }
    else if(strncmp(cmd, CMD_USER_LIST, strlen(CMD_USER_LIST)) == 0) {
        _cmd_user_list(client_fd);
    }
    else if(strncmp(cmd, CMD_WHISPER, strlen(CMD_WHISPER)) == 0) {
        char *str = _cmd_find_next_arg (cmd, (bytes-len));
        _cmd_send_whisper (client_fd, str, (bytes-len-(str-cmd)));
        chat_room_write(client_fd, PROMPT_TEXT, strlen(PROMPT_TEXT));
    }
    else if(strncmp(cmd, CMD_MY_NAME, strlen(CMD_MY_NAME)) == 0) {
        int i = fd_to_client_list_index (client_fd);
        size_t size = USER_NAME_MAX_LEN+15;
        char *send_buffer = calloc (size, 1);
        int bytes = snprintf(send_buffer, size-1, KGRN "[server]: %s\n" KNRM PROMPT_TEXT, client_list[i].username);

        chat_room_write(client_fd, send_buffer, bytes);
        free(send_buffer);
    }
    else if(strncmp(cmd, CMD_HELP, strlen(CMD_HELP)) == 0) {
        size_t size = 512;
        char *send_buffer = calloc (size, 1);
        int bytes = 0;
        bytes += snprintf(send_buffer+bytes, size-bytes, CR "The following commands are available:\n");
        bytes += snprintf(send_buffer+bytes, size-bytes, "\t" CMD_QUIT " - disconnect from the chat room\n");
        bytes += snprintf(send_buffer+bytes, size-bytes, "\t" CMD_SET_USERNAME " <new_username> - change your username\n");
        bytes += snprintf(send_buffer+bytes, size-bytes, "\t" CMD_SET_ROOM_NAME " <new_roomname> - change the room name\n");
        bytes += snprintf(send_buffer+bytes, size-bytes, "\t" CMD_SHUTDOWN " - shutdown the chat server\n");
        bytes += snprintf(send_buffer+bytes, size-bytes, "\t" CMD_WHISPER " <username> <message> - "
                                            "send a private whisper to the specified user\n");
        bytes += snprintf(send_buffer+bytes, size-bytes, "\t" CMD_USER_LIST " - returns a list of current users\n");
        bytes += snprintf(send_buffer+bytes, size-bytes, "\t" CMD_MY_NAME " - returns your specified username\n");


        bytes += snprintf(send_buffer+bytes, size-bytes, PROMPT_TEXT);
        send_to_all_clients (send_buffer, bytes);
        free(send_buffer);
    }
    else {
        chat_room_write(client_fd, PROMPT_TEXT, strlen(PROMPT_TEXT));
    }
}
/*<============================================================>*/
//search through the client list for the fd
static int fd_to_client_list_index (int client_fd)
{
    int ret = SOCK_ERR;
    int i;
    //we use pfds_count-1 because the client list will always have 1 element less than pfds
    for(i=0; i<(pfds_count-1); i++) {
        if(client_list[i].fd == client_fd) {
            break;
        }
    }
    //if found, set return value
    if(i < (pfds_count-1)) { ret = i; }
    return ret;
}
/*<============================================================>*/
//search through the client list for the username
static int username_to_client_list_index (char *username)
{
    int ret = SOCK_ERR;
    int i;
    //we use pfds_count-1 because the client list will always have 1 element less than pfds
    for(i=0; i<(pfds_count-1); i++) {
        if(strncmp(client_list[i].username, username, USER_NAME_MAX_LEN) == 0) {
            break;
        }
    }
    //if found, set return value
    if(i < (pfds_count-1)) { ret = i; }
    return ret;
}
/*<============================================================>*/
//send the given message to all clients except the one specified
static void send_to_other_clients (int client_fd, char *send_buffer, int bytes)
{
    int i;
    for(i=0; i<(pfds_count-1); i++) {
        if(client_list[i].fd != client_fd) {
            chat_room_write (client_list[i].fd, send_buffer, bytes);
        }
    }
}
/*<============================================================>*/
//send the given message to all clients
static void send_to_all_clients (char *send_buffer, int bytes)
{
    int i;
    for(i=0; i<(pfds_count-1); i++) {
        chat_room_write (client_list[i].fd, send_buffer, bytes);
    }
}
/*<============================================================>*/
//update the username of the specified client
static void _cmd_setuname (int client_fd, char *username)
{
    int i = fd_to_client_list_index (client_fd);
    int len = strlen (username);
    len = chat_room_remove_crlf(username, len);
    int j;
    //if found
    if(i != SOCK_ERR) {
        //check if old username is same as current
        if(strncmp(client_list[i].username, username, USER_NAME_MAX_LEN) == 0) {
            //send prompt symbol to this client
            chat_room_write(client_fd, PROMPT_TEXT, strlen(PROMPT_TEXT));
            return;
        }
        //check if it is unique
        for(j=0; j<(pfds_count-1); j++) {
            if(strncmp(client_list[j].username, username, USER_NAME_MAX_LEN) == 0) {
                char *str = CR KRED "[server]: This username is already in use\n" KNRM PROMPT_TEXT;
                chat_room_write(client_fd, str, strlen(str));
                return;
            }
        }
        //notify other clients about username change
        size_t buff_size = 70;
        int bytes = 0;
        char *send_buffer = calloc (buff_size, 1);
        if(client_list[i].first == false) {
            bytes = snprintf(send_buffer, buff_size-1 ,
                        CLEAR_LINE KGRN "[server] %s has joined the chat" KNRM CRLF PROMPT_TEXT,
                        username);
        }
        else {
            bytes = snprintf(send_buffer, buff_size-1 ,
                        CLEAR_LINE KGRN "[server] %s renamed to %s" KNRM CRLF PROMPT_TEXT,
                        client_list[i].username, username);
        }
        send_to_other_clients(client_fd, send_buffer, bytes);
        free(send_buffer);
        //update username in local list
        memset (client_list[i].username, 0, USER_NAME_MAX_LEN);
        strncpy (client_list[i].username, username, USER_NAME_MAX_LEN-1);
        printf("renamed user %d to %s\n", client_list[i].id, username);
        //send prompt symbol to this client
        chat_room_write(client_fd, PROMPT_TEXT, strlen(PROMPT_TEXT));
    }
}
/*<============================================================>*/
//set the chat room name - notify all clients about the change
static void _cmd_set_room_name (int client_fd, char *room_name)
{
    //notify all clients
    size_t size = 2*ROOM_NAME_MAX_LEN + 20;
    char *buff =  calloc (size, 1);
    int bytes = snprintf(buff, size-1, CLEAR_LINE KGRN "[server] Chat room renamed from %s to %s" KNRM CRLF PROMPT_TEXT,
                                    chat_room_name, room_name);
    send_to_all_clients (buff, bytes);
    free(buff);

    //update chat room name
    memset (chat_room_name, 0, ROOM_NAME_MAX_LEN);
    strncpy (chat_room_name, room_name, ROOM_NAME_MAX_LEN);
}
/*<============================================================>*/
//send user list to the client that requested it
static void _cmd_user_list (int client_fd)
{
    int client_count = (pfds_count-1);
    size_t size = (client_count*USER_NAME_MAX_LEN) + (2*(client_count-1)) + USER_NAME_MAX_LEN;
    char *send_buffer = calloc (size, 1);
    int bytes = 0;
    int i;

    bytes += snprintf(send_buffer+bytes, (size-bytes-1), CR KRED "[server]: ");
    for(i=0; i<(client_count-1); i++) {
        bytes += snprintf(send_buffer+bytes, (size-bytes-1), "%s, ", client_list[i].username);
    }
    bytes += snprintf(send_buffer+bytes, (size-bytes-1), "%s\n" KNRM PROMPT_TEXT, client_list[i].username);

    chat_room_write(client_fd, send_buffer, bytes);
    free(send_buffer);
}
/*<============================================================>*/
//send a private message to the specified user
static void _cmd_send_whisper (int client_fd, char *buffer, int len)
{
    char *invalid = KRED CR "[server]: Invalid syntax\n" KNRM PROMPT_TEXT;
    char *notfound = KRED CR "[server]: User not found\n" KNRM PROMPT_TEXT;
    if(buffer==NULL) {
        chat_room_write(client_fd, invalid, strlen(invalid));
        return;
    }
    int i, username_len;
    for(i=0; i<len; i++) {
        if(buffer[i]==' ') {
            break;
        }
    }
    username_len = i;

    //if end of username was not found
    if(i==len || i>USER_NAME_MAX_LEN) {
        chat_room_write(client_fd, invalid, strlen(invalid));
        return;
    }
    char username[USER_NAME_MAX_LEN] = {0,};
    strncpy (username, buffer, username_len);
    int index = username_to_client_list_index (username);
    if(index == SOCK_ERR) {
        chat_room_write(client_fd, notfound, strlen(notfound));
    }
    else {
        size_t size = (USER_NAME_MAX_LEN*2) + len;
        char *msg = _cmd_find_next_arg(buffer, len);
        char *send_buffer = calloc (size, 1);
        int myindex = fd_to_client_list_index(client_fd);
        int bytes = snprintf(send_buffer, size-1, KCYN CR "[%s]: %s\n" KNRM PROMPT_TEXT,
                                    client_list[myindex].username, msg);

        chat_room_write(client_list[index].fd, send_buffer, bytes);
        free(send_buffer);
    }
    return;
}
/*<============================================================>*/
//traverse through the array and find the start of the next arg
//the start of the next arg is defined as the character that is after a space
//or a series of spaces
static char* _cmd_find_next_arg (char *buffer, int len)
{
    char *ret = NULL;
    int i;
    //find the space character
    for(i=0; i<len; i++) {
        if(buffer[i] == ' ') {break;}
    }
    //if no space is found, return NULL
    if(i==len) { return ret; }

    //now that the space character is found, find the next non-space character
    for(; i<len; i++) {
        if(buffer[i] != ' ') {break;}
    }
    //if found, update the ret ptr
    if(i < len) {ret = buffer+i; }
    return ret;
}
