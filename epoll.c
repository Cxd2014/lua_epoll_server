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

static void update_events(int efd, int lfd, int events, int op)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));

    ev.events = events; 
    ev.data.fd = lfd;
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

    log_debug("connect from %s", inet_ntoa(client_addr.sin_addr));
    //set_noblock(cfd);
    update_events(efd, cfd, EPOLLIN|EPOLLOUT|EPOLLET, EPOLL_CTL_ADD);
}

int send_http_file(struct http_request *request, char *file_path)
{
    /* 打开要发送的文件 */
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        log_perror("open error %s", file_path);
        return fd;
    }
    
    /* 在发送请求文件
     * 注意：当设置为非阻塞模式的时候，发送大文件的时候会出现
     * 缓冲区被写满，导致发送文件失败
     */
    char buf[2048] = {0};
    int size = 0;
    while ((size = read(fd, buf, 2048)) > 0) {
        if (write(request->fd, buf, size) < 0) {
            log_perror("write error");
            return -1;
        }
        memset(buf, 0, 2048);
    }
    return 0;
}

void epoll_write(struct http_request *request)
{
    if (write(request->fd, request->replay_buf, request->replay_len) < 0) {
        log_perror("write error, fd = %d", request->fd);   
    }
}

static void connect_read(int efd, int fd)
{
    int size = 0;
    char buf[BUF_SIZE] = {0};

    if((size = read(fd, buf, BUF_SIZE)) < 0) {
        log_perror("read buf error"); 
        connect_close(efd, fd); 
        return;
    }

    struct http_request *request = http_request_malloc(buf, size, fd, efd);
    if (parse_http_request(request) < 0) {
        connect_close(efd, fd);
        http_request_free(request);
    }
        
}

static void connect_write(int efd, int fd)
{
    log_info("connect_write");
}

void epoll_loop(char *ip, char *port)
{
    int listenfd = init_server_socket(ip, port);

    /* 创建一个 epoll 句柄，参见 man epoll_create 手册 */
    int epollfd = epoll_create(MAX_EVENTS);
    if (epollfd < 0) 
        log_die("epoll create error");
      
    update_events(epollfd, listenfd, EPOLLIN, EPOLL_CTL_ADD);
    
    struct epoll_event events[MAX_EVENTS];
    int n = 0, nfds = 0;
    while(1) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) 
            log_perror("epoll_wait");
           
        update_log_time();
        log_debug("epoll_loop nfds =%d", nfds);

        for (n = 0; n < nfds; n++) {
            int fd = events[n].data.fd;
            int ev = events[n].events;

            if (ev & EPOLLIN) {
                if (fd == listenfd){
                    connect_accept(epollfd, fd);
                } else {
                    connect_read(epollfd, fd);
                }
            } else if (ev & EPOLLOUT) {
                connect_write(epollfd, fd);
            } else {
                log_error("unknown event");
            }
        }
    }

    log_die("No possable go to here!");
}
