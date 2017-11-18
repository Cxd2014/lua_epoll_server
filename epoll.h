#ifndef epoll_h
#define epoll_h

#include "http.h"

#define MAX_EVENTS 100
#define BACK_LOG 10
#define BUF_SIZE 2048


void epoll_write_and_close(struct http_request *request);
void epoll_loop(char *ip, char *port);

#endif
