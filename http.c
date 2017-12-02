#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "http.h"
#include "epoll.h"
#include "main.h"
#include "lua_lib.h"

#define HTML_PATH "html"

#define NOT_FOUND "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nUser-Agent: cxd-lua-server\r\n\r\n<h1>Error Not Found This Page</h1>"
#define NO_REPALY "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nUser-Agent: cxd-lua-server\r\n\r\n<h1>Opps This Request No Replay</h1>"

#define TEMPLATE "HTTP/1.1 %s\r\nContent-Type: %s\r\nLocation: %s\r\nContent-Length: %d\r\nCache-Control: no-cache\r\nConnection: close\r\nUser-Agent: cxd-lua-server\r\n\r\n%s"
void combie_http_lua_replay(struct http_request *request, const char *code, const char *type, const char *location, const char *body)
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

#define HTML_TEMPLATE "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nCache-Control: no-cache\r\nConnection: keep-alive\r\nUser-Agent: cxd-lua-server\r\n\r\n"
static int combie_http_file_replay(struct http_request *request, char *file_path)
{
    struct stat stat_info;  
    if(stat(file_path, &stat_info) < 0) {  
        log_perror("stat error %s", file_path);
        return -1;  
    }
    
    char file_type[32] = {0};
    char *type = strrchr(request->uri, '.');
    if (type == NULL)
        strcpy(file_type, "Unknow/Type");
    else if (strcmp(type, ".html") == 0)
        strcpy(file_type, "text/html");
    else if (strcmp(type, ".jpg") == 0)
        strcpy(file_type, "image/jpg");
    else if (strcmp(type, ".ico") == 0)
        strcpy(file_type, "image/x-icon");

    request->replay_buf = (char *)zero_alloc(MAX_REPLAY_SIZE);
    snprintf(request->replay_buf, MAX_REPLAY_SIZE, HTML_TEMPLATE, "200 OK", file_type, (int)stat_info.st_size);
    request->replay_len = strlen(request->replay_buf);

    return 0;
}

static void do_request(char *method, struct http_request *request)
{
    char task_id[MAX_ID_LEN] = {0};
    strcpy(task_id, method);
    strncat(task_id, request->uri, MAX_ID_LEN);
    log_debug("task_id = %s", task_id);

    if (task_run_executor(task_id, request) < 0) {  /* Lua中没有注册此url，查找HTML目录下面有没有此文件 */
       
        char file_path[128] = HTML_PATH;
        strncat(file_path, request->uri, 128);

        if(combie_http_file_replay(request, file_path) < 0) { /* HTML目录下面没有此文件，返回404 */
            request->replay_len = strlen(NOT_FOUND);
            request->replay_buf = (char *)zero_alloc(request->replay_len + 1);
            strncpy(request->replay_buf, NOT_FOUND, request->replay_len);
            epoll_write(request);
        } else {
            epoll_write(request); /* 先发送http头部信息 */
            send_http_file(request, file_path);
        }
    } else {
        /* 如果Lua没有返回，就返回一个默认页面 */
        if(request->replay_buf == NULL) {
            request->replay_len = strlen(NO_REPALY);
            request->replay_buf = (char *)zero_alloc(request->replay_len + 1);
            strncpy(request->replay_buf, NO_REPALY, request->replay_len);
        }
        epoll_write(request);
    }
    
    connect_close(request->efd, request->fd);
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
