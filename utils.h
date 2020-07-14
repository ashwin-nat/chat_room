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

#define CMD_ESC_SEQ         "/"
#define CMD_SET_USERNAME    "uname "
#define CMD_SET_ROOM_NAME   "roomname "
#define CMD_QUIT            "q"
#define CMD_SHUTDOWN        "shutdown"
#define CMD_HELP            "help"
#define CMD_WHISPER         "w "
#define CMD_USER_LIST       "list"
#define CMD_MY_NAME         "myname"

#define USER_NAME_MAX_LEN   40
#define IPv4_ADD_STR_LEN    20
#define ROOM_NAME_MAX_LEN   30
#define ROOM_NAME_DEFAULT   "The Chat Room"

#define CR                  "\r"
#define LF                  "\n"
#define CLEAR_LINE          "\33[2K\r"
#define CRLF                CR LF

#define PROMPT_TEXT         CR"--> "

#define KNRM                "\x1B[0m"
#define KRED                "\x1B[31m"
#define KGRN                "\x1B[32m"
#define KYEL                "\x1B[33m"
#define KBLU                "\x1B[34m"
#define KMAG                "\x1B[35m"
#define KCYN                "\x1B[36m"
#define KWHT                "\x1B[37m"

int setup_server (uint16_t port, int backlog, bool print_info);
int accept_connections (int server_fd, struct sockaddr_in *cliaddr);
int chat_room_read (int fd, void *buff, size_t buff_size);
int chat_room_write (int fd, void *buff, size_t len);
int chat_room_connect (char *ipaddr, uint16_t port);
int chat_room_remove_crlf (char *buff, int len);
int chat_room_insert_crlf (char *buff, int len);

#endif // __UTILS_H__
