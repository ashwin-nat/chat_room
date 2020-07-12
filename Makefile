HOSTPLATFORM:=WINDOWS

ifeq ($(HOSTPLATFORM),WINDOWS)
CC := arm-linux-gnueabihf-gcc
CLEAN:= del /q
else
CC := gcc
CLEAN:= rm -rf
endif
#CC:=arm-linux-gnueabihf-gcc

CFLAGS_RELEASE:=	-O2 \
					-DNDEBUG \
					-Wall \
					-Werror
CFLAGS_DEBUG:=		-O0 \
					-ggdb3 \
					-Wall

all: server_release client_release
debug: server_debug client_debug

server_release:
	$(CC) $(CFLAGS_RELEASE) utils.c server.c chat_room_server.c -o chat_room_server

client_release:
	$(CC) $(CFLAGS_RELEASE) utils.c client.c chat_room_client.c -o chat_room_client

clean:
	$(CLEAN) chat_room_server

server_debug:
	$(CC) $(CFLAGS_DEBUG) utils.c server.c chat_room_server.c -o chat_room_server
	
client_debug:
	$(CC) $(CFLAGS_DEBUG) utils.c client.c chat_room_client.c -o chat_room_client