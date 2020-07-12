#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <poll.h>
#include <stdbool.h>
#include "utils.h"
#include "chat_room_server.h"
/***********************************************************************************************/
#define CHAT_ROOM_BACKLOG       5
#define BUFFER_SIZE             512
#define MAX_CLIENTS             5
#define POLLFD_STRUCT_SIZE      MAX_CLIENTS+1
/***********************************************************************************************/
//static function declarations
static int print_usage (char *filename);
/***********************************************************************************************/
int main(int argc, char *argv[])
{
    if(argc != 2) {
        return print_usage (argv[0]);
    }
    uint16_t port = (uint16_t) strtoul (argv[1], NULL, 0);

    int server_fd = setup_server (port, CHAT_ROOM_BACKLOG, true);
    if(init_chat_room_server (MAX_CLIENTS, BUFFER_SIZE) == SOCK_ERR) {
        return 1;
    }
    chat_room_server_loop(server_fd);
    return 0;
}
/***********************************************************************************************/
static int print_usage (char *filename)
{
    printf("usage: %s <port>\n", filename);
    return 1;
}
/*<============================================================>*/
