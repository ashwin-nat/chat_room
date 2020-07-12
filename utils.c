#include "utils.h"
#include <stdlib.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
/***********************************************************************************************/
//static function declarations
static int _create_tcp_server (uint16_t port, int backlog);
static void print_host_name (void);
static void print_ip_addr (void);
/***********************************************************************************************/
//public function definitions
//sets up the tcp server and returns the fd of the server
//returns -1 on failure
int setup_server (uint16_t port, int backlog, bool print_info)
{
    int server_fd = _create_tcp_server (port, backlog);
    if(server_fd == SOCK_ERR) {
        //we exit because the main purpose of this app is to be a tcp server
        exit(1);
    }

    //print the info
    if(print_info) {
        printf("Server Port %"PRIu16"\n", port);
        print_host_name();
        print_ip_addr();
    }

    return server_fd;
}
/*<============================================================>*/
//wait and accept incoming connections
//returns the client_fd on success, -1 on failure
int accept_connections (int server_fd, struct sockaddr_in *cliaddr)
{
    socklen_t len = sizeof(*cliaddr);
    int client_fd = accept (server_fd, (struct sockaddr*)cliaddr, &len);
    if(client_fd == SOCK_ERR) {
        perror("accept");
    }
    return client_fd;
}
/*<============================================================>*/
//read data from the socket or stdin
int chat_room_read (int fd, void *buff, size_t buff_size)
{
    if(buff==NULL || buff_size == 0) {
       printf("%s: empty buffer passed\n", __func__);
       return SOCK_ERR;
    }
    int ret;
    if(fd == STDIN_FILENO) {
        ret = read (fd, buff, buff_size);
    }
    //it is a socket
    else {
        ret = recv (fd, buff, buff_size, 0);
    }
    return ret;
}
/*<============================================================>*/
//write data to the socket or stdout or stderr
int chat_room_write (int fd, void *buff, size_t len)
{
    if(buff==NULL || len == 0) {
       printf("%s: empty buffer passed\n", __func__);
       return SOCK_ERR;
    }
    //use write for stdout or stderr
    if(fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        return write (fd, buff, len);
    }
    //use send for socket
    else {
        return send (fd, buff, len, MSG_NOSIGNAL);
    }
}
/*<============================================================>*/
//write data to the socket or stdout or stderr
int chat_room_connect (char *ipaddr, uint16_t port)
{
    //create the socket
    int fd = socket (AF_INET, SOCK_STREAM, 0);
    if(fd == SOCK_ERR) {
        perror ("socket");
        exit (1);
    }

    //parse server address
    struct sockaddr_in server = {0,};
    server.sin_family       = AF_INET;
    server.sin_port         = htons (port);
    if(inet_pton(AF_INET, ipaddr, &server.sin_addr.s_addr) != 1) {
        perror("inet_pton");
        exit(1);
    }

    //connect to server
    if(connect (fd, (struct sockaddr*) &server, sizeof(server)) == SOCK_ERR) {
        perror("connect");
        exit(1);
    }

    return fd;
}
/*<============================================================>*/
//removes the trailing \n if present, returns the updated msg len
int chat_room_remove_crlf (char *buff, int len)
{
    int ret = len;
    int i;
    for(i=len-2; i<len; i++) {
        if(buff[i] == 0x0A || buff[i] == 0x0D) {
            buff[i] = 0;
            ret--;
        }
    }
    return ret;
}
/*<============================================================>*/
//adds the trailing \n if not present, returns the updated msg len
int chat_room_insert_crlf (char *buff, int len)
{
    int ret = len;
    //check if crlf already exists
    if(buff[len-2] == 0x0D && buff[len-1] == 0x0A) {
        //do nothing
        ;
    }
    //check if only lf exists
    else if (buff[len-1] == 0x0A) {
        buff[len-1] = 0x0D;
        buff[len+0] = 0x0A;
        ret++;
    }
    //neither of those exists
    else {
        buff[len+0] = 0x0D;
        buff[len+1] = 0x0A;
        ret+=2;
    }
    return ret;
}
/***********************************************************************************************/
//static function definitions
//create socket, set options, bind it to the given port and setup listening
static int _create_tcp_server (uint16_t port, int backlog)
{
    //create the socket
    int server_fd = socket (AF_INET, SOCK_STREAM, 0);
    if(server_fd == SOCK_ERR) {
        perror("socket");
        return SOCK_ERR;
    }

    //set options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == SOCK_ERR) {
        perror ("socket");
        close (server_fd);
        return SOCK_ERR;
    }

    //bind to port
    struct sockaddr_in server = {0,};
    server.sin_family       = AF_INET;
    server.sin_addr.s_addr  = htonl (INADDR_ANY);
    server.sin_port         = htons (port);
    if( bind(server_fd, (struct sockaddr*)&server, sizeof(server)) == SOCK_ERR) {
        perror("bind");
        close (server_fd);
        return SOCK_ERR;
    }

    //set the wait queue length
    if( listen(server_fd, backlog) == SOCK_ERR) {
        perror("listen");
        close (server_fd);
        return SOCK_ERR;
    }

    return server_fd;
}
/*<============================================================>*/
//print the hostname of the system
static void print_host_name (void)
{
        FILE *fp = fopen ("/etc/hostname", "r");
        char buffer[100] = {0,};
        fgets (buffer, sizeof(buffer)-1, fp);
        printf("hostname: %s", buffer);
        fclose(fp);
}
/*<============================================================>*/
//print the hostname of the system
static void print_ip_addr (void)
{
        //get list of interfaces
        struct ifaddrs *addrs, *iap;
        struct sockaddr_in *sa;
        int rc = getifaddrs (&addrs);
        char *ip_addr = NULL;
        if(rc == -1) return;

        printf("ip addresses:\n");
        //loop through the resulting linked list
        for(iap = addrs; iap; iap = iap->ifa_next) {
                //accept only interfaces that are up
                if (iap->ifa_addr && (iap->ifa_flags & IFF_UP) && iap->ifa_addr->sa_family == AF_INET) {
                        sa = (struct sockaddr_in*) iap->ifa_addr;
                        ip_addr =  inet_ntoa(sa->sin_addr);
                        printf("\t%s\n", ip_addr);
                }
        }
        //cleanup and return
        freeifaddrs(addrs);
}
