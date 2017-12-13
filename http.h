#ifndef http_h
#define http_h

#include "hash.h"

#define MAX_REPLAY_SIZE 2048

struct http_request {
    int fd;
    int efd;

    int http_len;
    char *http_buf;

    int replay_len;
    char *replay_buf;

    char *method;
    char *uri; 
    char *version;
    char *http_body;
    hash_st *param;

    int file_fd;
    long int send_size;
    long int offset;
};

struct http_request *http_request_malloc(int fd, int efd);
void http_request_free(struct http_request *request);

int parse_http_request(struct http_request *request);
void combie_http_lua_replay(struct http_request *request, const char *code, const char *type, const char *location, const char *body);

#endif