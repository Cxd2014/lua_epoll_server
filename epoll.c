#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>

#include "epoll.h"
#include "main.h"

/* 设置套接字为非阻塞模式 */
static void set_noblock(int sock)
{
	int opts;
	opts = fcntl(sock, F_GETFL);
	if(opts < 0) {
		log_perror("fcntl(sock, GETFL) error");
	}

	opts = opts | O_NONBLOCK;
	if(fcntl(sock, F_SETFL, opts) < 0) {
		log_perror("fcntl(sock, SETFL, opts) error");
	}
}

/* 初始化一个服务器端的监听套接字 */
static int init_server_socket(char *ip, char *port)
{
    log_debug("server ip: %s port: %s",ip,port);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) 
        log_die("server socket create error");
    
    int ret = 0;
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	inet_aton(ip, &(server_addr.sin_addr));
	server_addr.sin_port = htons(atoi(port));
    ret = bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0)
        log_die("server socket bind error");

    ret = listen(listenfd, BACK_LOG);
    if (ret < 0)
        log_die("server socket listen error");

    set_noblock(listenfd);

    return listenfd;
}

static void update_events(int efd, int lfd, int events, int op, struct http_request *request)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));

    ev.events = events; 
    
    if (request != NULL) {
        ev.data.ptr = request;
    } else {
        ev.data.fd = lfd;
    }
        
    if (epoll_ctl(efd, op, lfd, &ev) == -1) {
        log_perror("epoll_ctl error");
    }
}

void connect_close(int efd, int fd)
{
    struct epoll_event ev;
    ev.events = 0;
    ev.data.ptr = NULL;

    if (epoll_ctl(efd, EPOLL_CTL_DEL, fd, &ev) < 0) {
        log_perror("epoll_ctl error");
    }
    close(fd);
}

static void connect_accept(int efd, int fd) 
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int cfd = accept(fd, (struct sockaddr *)&client_addr, &addr_len);
    if (cfd < 0) {
        log_perror("accept error");
        connect_close(efd, fd);
    }

    log_debug("connect from %s fd = %d", inet_ntoa(client_addr.sin_addr), cfd);
    set_noblock(cfd);
    
    struct http_request *request = http_request_malloc(cfd, efd);
    update_events(efd, cfd, EPOLLIN|EPOLLOUT|EPOLLET, EPOLL_CTL_ADD, request);
}

int send_http_file(struct http_request *request)
{
    int size = 0;
    if ((size = sendfile(request->fd, request->file_fd, &request->offset, request->send_size)) < 0) {
        log_perror("sendfile() failed fd = %d", request->fd);  
        return -1; 
    }
    request->send_size = request->send_size - size;
    log_debug("size = %d, offset = %ld, send_size = %d", size, request->offset, request->send_size);
    return size;
}

void epoll_write(struct http_request *request)
{
    if (write(request->fd, request->replay_buf, request->replay_len) < 0) {
        log_perror("write error, fd = %d", request->fd);   
    }
}

static void connect_read(struct http_request *request)
{
    request->http_buf = (char *)zero_alloc(BUF_SIZE + 1);
    if((request->http_len = read(request->fd, request->http_buf, BUF_SIZE)) <= 0) {
        log_perror("read buf error fd = %d", request->fd); 
        connect_close(request->efd, request->fd); 
        http_request_free(request);
        return;
    }

    if (parse_http_request(request) < 0) {
        connect_close(request->efd, request->fd);
        http_request_free(request);
    }
        
}

static void connect_write(struct http_request *request)
{
    if (request->file_fd) {
        int ret = send_http_file(request);
        if (request->send_size == 0) {
            log_debug("sendfile finish!");
        } else if (ret < 0) {
            log_debug("sendfile fail!");
        } else { /* 文件还没有发送完成 等下次epoll写事件再接着发送 */
            return;
        }
        /* 文件发送完，或者发送失败都需要释放资源 */
        connect_close(request->efd, request->fd);     
        http_request_free(request);
    } else {
        log_info("No file to send ! efd = %d, fd = %d", request->efd, request->fd);
    }
}

void epoll_loop(char *ip, char *port)
{
    int listenfd = init_server_socket(ip, port);

    /* 创建一个 epoll 句柄，参见 man epoll_create 手册 */
    int epollfd = epoll_create(MAX_EVENTS);
    if (epollfd < 0) 
        log_die("epoll create error");
      
    update_events(epollfd, listenfd, EPOLLIN, EPOLL_CTL_ADD, NULL);
    
    struct epoll_event events[MAX_EVENTS];
    int n = 0, nfds = 0;
    while(1) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) 
            log_perror("epoll_wait");
           
        update_log_time();
        log_debug("epoll_loop nfds =%d", nfds);

        for (n = 0; n < nfds; n++) {
            int ev = events[n].events;
            struct http_request *request = events[n].data.ptr;
            int fd = events[n].data.fd;

            if (ev & EPOLLIN) {
                if (fd == listenfd){          
                    connect_accept(epollfd, fd);
                } else {         
                    connect_read(request);
                }
            } else if (ev & EPOLLOUT) {
                connect_write(request);
            } else {
                log_error("unknown event");
            }
        }
    }

    log_die("No possable go to here!");
}
