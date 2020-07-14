#include "chat_room_client.h"
#include "utils.h"
#include <poll.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
/***********************************************************************************************/
#define POLL_PERIOD     1500
#define BUFFER_SIZE     512
/***********************************************************************************************/
int chat_room_client_loop(int server_fd, char *username)
{
    char buffer[BUFFER_SIZE];
    //poll on stdin and server
    struct pollfd pfds[2] = {
        {.fd = STDIN_FILENO,    .events = POLLIN},
        {.fd = server_fd,       .events = POLLIN},
    };
    int rc,bytes;
    //bool prompt = false;
    //the client loop
    while(1) {
#if 0
        if(prompt == true) {
            chat_room_write(STDOUT_FILENO, PROMPT_TEXT, strlen(PROMPT_TEXT));
            prompt = false;
        }
#endif
        //poll on the 2 fd's
        rc = poll (pfds, 2, POLL_PERIOD);
        if(rc == SOCK_ERR) {
            perror("poll");
            exit(1);
        }
        //no fd's ready
        else if(rc == 0) {
            continue;
        }
        else {
            memset (buffer, 0, BUFFER_SIZE);
            //stdin is ready for reading
            if(pfds[0].revents & POLLIN) {
                bytes = chat_room_read(pfds[0].fd, buffer, BUFFER_SIZE-1);
                //we check if bytes > 1 because it includes the line break
                if(bytes > 1) {
                    //send it to the server
                    bytes = chat_room_insert_crlf(buffer, bytes);
                    bytes = chat_room_write(pfds[1].fd, buffer, bytes);
                }
                else {
                    chat_room_write(STDOUT_FILENO, PROMPT_TEXT, strlen(PROMPT_TEXT));
                }
                //prompt = true;
            }
            //if server fd is ready for reading
            if(pfds[1].revents & POLLIN) {
                bytes = chat_room_read(pfds[1].fd, buffer, BUFFER_SIZE-1);
                if(bytes > 0) {
                    //write it to stdout
                    //bytes = chat_room_insert_crlf(buffer, bytes);
                    //chat_room_write(STDOUT_FILENO, CLEAR_LINE, strlen(CLEAR_LINE));
                    chat_room_write(STDOUT_FILENO, buffer, bytes);
                    //chat_room_write(STDOUT_FILENO, CR, strlen(CR));
                    //prompt = true;
                }
                else {
                    printf("connection closed\n");
                    break;
                }
            }
        }
    }

    return 0;
}
