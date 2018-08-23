#include "task.h"
#include "thread_pool.h"
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>
#define MAX_EVENTS 10
#define PORT 8989

using namespace std;
//设置socket连接为非阻塞模式
void setnonblocking(int sockfd) {
    int opts;

    opts = fcntl(sockfd, F_GETFL);
    if(opts < 0) {
        perror("fcntl(F_GETFL)\n");
        exit(1);
    }
    opts = (opts | O_NONBLOCK);
    if(fcntl(sockfd, F_SETFL, opts) < 0) {
        perror("fcntl(F_SETFL)\n");
        exit(1);
    }
}

 void setreuseaddr(int sockfd)
 {
    int on = 1;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(ret == -1)
    {
         perror("setsockopt: SO_REUSEADDR");
         exit(-1);
    }
 }

int main(){
    struct epoll_event ev, events[MAX_EVENTS];
    socklen_t  addrlen;
    int listenfd, conn_sock, nfds, epfd, fd, i, nread, sockfd;
    struct sockaddr_in local, remote;
    char buf[BUFSIZ];
    threadpool<task> pool(20);
    pool.start();
    //创建listen socket
    if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("sockfd\n");
        exit(1);
    }
    setnonblocking(listenfd);
    setreuseaddr(listenfd);
    bzero(&local, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);;
    local.sin_port = htons(PORT);
    if( bind(listenfd, (struct sockaddr *) &local, sizeof(local)) < 0) {
        perror("bind\n");
        exit(1);
    }
    listen(listenfd, 20);

    epfd = epoll_create(MAX_EVENTS);
    if (epfd == -1) {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_pwait");
            exit(EXIT_FAILURE);
        }
        cout<<"hello"<<endl;
        for (i = 0; i < nfds; ++i) {
            fd = events[i].data.fd;
            if (fd == listenfd) {
                while ((conn_sock = accept(listenfd,(struct sockaddr *) &remote,
                                &addrlen)) > 0) {
                    setnonblocking(conn_sock);
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = conn_sock;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, conn_sock,
                                &ev) == -1) {
                        perror("epoll_ctl: add");
                        exit(EXIT_FAILURE);
                    }

                }
                if (conn_sock == -1) {
                    if (errno != EAGAIN && errno != ECONNABORTED
                            && errno != EPROTO && errno != EINTR)
                        perror("accept");
                }
                continue;
            }
             else if(events[i].events&EPOLLOUT || events[i].events&EPOLLIN)
            {
                 if ( (sockfd= events[i].data.fd) < 0)
                      continue;
                 ev.data.fd = sockfd;
                 epoll_ctl(epfd,EPOLL_CTL_DEL,sockfd,&ev);

                 task *ta = new task(sockfd);
                    //向线程池添加任务
                 pool.append_task(ta);


            }

     
