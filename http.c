#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "epoll.h"
#include "main.h"
#include "lua_lib.h"


#define NOT_FOUND "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nUser-Agent: epoll-lua-server\r\n\r\n<h1>Not Found</h1>"
#define NO_REPALY "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nUser-Agent: epoll-lua-server\r\n\r\n<h1>No Replay</h1>"

#define TEMPLATE "HTTP/1.1 %s\r\nContent-Type: %s\r\nLocation: %s\r\nContent-Length: %d\r\nCache-Control: no-cache\r\nConnection: close\r\nUser-Agent: epoll-lua-server\r\n\r\n%s"
void combie_http_replay(struct http_request *request, const char *code, const char *type, const char *location, const char *body)
{
    if(request->replay_buf == NULL) {
        request->replay_buf = (char *)zero_alloc(MAX_REPLAY_SIZE);
        int len = strlen(body);
        snprintf(request->replay_buf, MAX_REPLAY_SIZE, TEMPLATE, code, type, location, len, body);
        request->replay_len = strlen(request->replay_buf);
    } else {
        log_error("The %s request replay twice!", request->uri);
    }
}

static void do_request(char *method, struct http_request *request)
{
    char task_id[MAX_ID_LEN] = {0};
    strcpy(task_id, method);
    strncat(task_id, request->uri, MAX_ID_LEN);
    log_debug("task_id = %s", task_id);

    if (task_run_executor(task_id, request) < 0) {
        request->replay_len = strlen(NOT_FOUND);
        request->replay_buf = (char *)zero_alloc(request->replay_len + 1);
        strncpy(request->replay_buf, NOT_FOUND, request->replay_len);
    } else {
        if(request->replay_buf == NULL) {
            request->replay_len = strlen(NO_REPALY);
            request->replay_buf = (char *)zero_alloc(request->replay_len + 1);
            strncpy(request->replay_buf, NO_REPALY, request->replay_len);
        }
    }
    
    epoll_write_and_close(request);
    http_request_free(request);
}

static void parse_request_line(char *head,struct http_request *request)
{
    char *param = NULL;
    char *name = NULL; 
    char *value = NULL;
    char *lines = NULL;

    request->method = strtok_r(head, " ", &head);
    request->uri = strtok_r(head, " ", &head);
    request->version = strtok_r(head, " ", &head);
    
    request->uri = strtok_r(request->uri, "?", &param);
    if(param == NULL)
        return;

    while (*param != '\0') {
        lines = strtok_r(param, "&", &param);
        name = strtok_r(lines, "=", &value);
        xhash_insert(request->param, name, strlen(name) + 1, (void *)value);
    }

}

struct http_request *http_request_malloc(char *buf, int len, int fd, int efd)
{
    struct http_request *request = (struct http_request *)zero_alloc(sizeof(struct http_request));

    request->http_len = len;
    request->fd = fd;
    request->efd = efd;

    request->http_buf = (char *)zero_alloc(len + 2);
    strncpy(request->http_buf, buf, len);

    request->param = xhash_create(NULL, NULL);
    return request;
}

void http_request_free(struct http_request *request)
{
    xhash_destroy(request->param);
    free(request->http_buf);

    if (request->replay_buf) {
        free(request->replay_buf);
    }

    free(request);
    request = NULL;
}

int parse_http_request(struct http_request *request)
{
    char *http_head = NULL, *pos = NULL;
    char *name = NULL; 
    char *value = NULL;
    char *lines = NULL;
    
    //log_debug("%s",request->http_buf);

    pos = strstr(request->http_buf, "\r\n\r\n");
    if (pos == NULL){
        log_error("parse http request url error\nhttp_buf = %s",request->http_buf);
        return -1;
    }

    request->http_buf[pos - request->http_buf] = '\0';
    request->http_body = pos + sizeof("\r\n\r\n") + 1;
    http_head = request->http_buf;

    /* 解析请求行 */
    lines = strtok_r(http_head, "\r\n", &http_head);
    parse_request_line(lines,request);

    while(*http_head != '\0'){
        lines = strtok_r(http_head, "\r\n", &http_head);
        name = strtok_r(lines, ": ", &value);
        xhash_insert(request->param, name, strlen(name) + 1, (void *)value);
    }

    if(strcmp(request->method, "GET") == 0){
        do_request("HTTPGET:", request);
    } else if (strcmp(request->method, "POST") == 0){
        do_request("HTTPPOST:", request);
    }

    return 0;
}
