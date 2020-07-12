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
/***********************************************************************************************/
static int chat_room_poll (void);
static bool add_fd (int fd, struct sockaddr_in *cliaddr);
static bool remove_fd (int fd);
static void process_client (int client_fd);
static void process_fd (int fd);
static bool is_command (char *buffer, int bytes);
static void process_command (int client_fd, char *buffer, int bytes);
static int fd_to_client_list_index (int client_fd);
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
            int bytes = snprintf(str, sizeof(str), "Welcome to the Chat Server.\n\tCurr users = %d\n\tMax Users = %d\n\tYour UID = %d\n",
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
        char *str = "Server is at full capacity\n";
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
            //copy the last fd to this location
            pfds[i].fd = pfds[pfds_count-1].fd;
            //copy last item in client list to this location
            memcpy (&client_list[i-1], &client_list[pfds_count-2], sizeof(*client_list));

            //clear the last item
            printf("client %s disconnected\n", client_list[i-1].username);
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
                    //4 extra bytes: 1 for [, 1 for ], 1 for :, 1 for space
                    char *send_buffer = calloc (bytes + USER_NAME_MAX_LEN + 4 + 1, 1);
                    //add username to message
                    bytes = snprintf(send_buffer, (bytes + USER_NAME_MAX_LEN + 4 + 1),
                                        "[%s] %s", client_list[i].username, recv_buffer);
                    bytes = chat_room_insert_crlf(send_buffer, bytes);
                    for(i=0; i<(pfds_count-1); i++) {
                        if(client_list[i].fd != client_fd) {
                            chat_room_write (client_list[i].fd, send_buffer, bytes);
                        }
                    }
                    free(send_buffer);
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
    char *cmd = buffer + len;
    if(strncmp(cmd, CMD_SET_USERNAME, len) == 0) {
        char *username = cmd + strlen (CMD_SET_USERNAME);
        int i = fd_to_client_list_index (client_fd);
        len = strlen (username);
        len = chat_room_remove_crlf(username, len);
        //if found
        if(i != SOCK_ERR) {
            memset (client_list[i].username, 0, USER_NAME_MAX_LEN);
            strncpy (client_list[i].username, username, USER_NAME_MAX_LEN-1);
            printf("renamed user %d to %s\n", client_list[i].id, username);
        }
        else {
            ;
        }
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
