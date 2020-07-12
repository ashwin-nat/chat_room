#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SOCK_ERR            -1
#define READ_TIMED_OUT      -2
#define CONN_CLOSED         -3

#define CMD_ESC_SEQ         "-!+-"
#define CMD_SET_USERNAME    "setuname "

#define USER_NAME_MAX_LEN   40
#define IPv4_ADD_STR_LEN    20

int setup_server (uint16_t port, int backlog, bool print_info);
int accept_connections (int server_fd, struct sockaddr_in *cliaddr);
int chat_room_read (int fd, void *buff, size_t buff_size);
int chat_room_write (int fd, void *buff, size_t len);
int chat_room_connect (char *ipaddr, uint16_t port);
int chat_room_remove_crlf (char *buff, int len);
int chat_room_insert_crlf (char *buff, int len);

#endif // __UTILS_H__
