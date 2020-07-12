#ifndef __CHAT_ROOM_SERVER_H__
#define __CHAT_ROOM_SERVER_H__

int init_chat_room_server (int max_clients, int buff_size);
void chat_room_server_loop (int server_fd_arg);

#endif // __CHAT_ROOM_SERVER_H__
