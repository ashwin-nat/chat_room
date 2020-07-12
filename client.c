#include "utils.h"
#include "chat_room_client.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static int print_usage(char *filename);

int main(int argc, char *argv[])
{
    //4 args are required, executable filename, ipaddr, port, username
	if(argc != 4) {
        return print_usage(argv[0]);
	}
	//parse args
	char *ipaddr = argv[1];
	uint16_t port = (uint16_t) strtoul (argv[2], NULL, 0);
	char *username = argv[3];
	if(strlen(username) > USER_NAME_MAX_LEN-1) {
        printf("User name too long. Limit is %d characters\n", USER_NAME_MAX_LEN-1);
        return 1;
	}
    //connect to the server
	int server_fd = chat_room_connect (ipaddr, port);
	if(server_fd == SOCK_ERR) {
        return 1;
	}
	//register username with the server
    char command[100] = {0,};
    int bytes = snprintf(command, sizeof(command)-1, CMD_ESC_SEQ CMD_SET_USERNAME "%s", username);
    chat_room_write(server_fd, command, bytes);
	//perform the client loop
    chat_room_client_loop (server_fd, username);
	return 0;
}

static int print_usage(char *filename)
{
    printf("usage: %s <ip_addr> <port> <username>\n", filename);
    return 1;
}
