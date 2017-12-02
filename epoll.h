#ifndef epoll_h
#define epoll_h

#include "http.h"

#define MAX_EVENTS 100
#define BACK_LOG 10
#define BUF_SIZE 2048

int send_http_file(struct http_request *request, char *file_path);
void epoll_write(struct http_request *request);
void epoll_loop(char *ip, char *port);
void connect_close(int efd, int fd);

#endif
