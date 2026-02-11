/* webserver_events.c */

//#include "webserver_events.h"
#include "webserver.h" // Enthält socket_info und handleer()

#if defined ( USE_EPOLL )


#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/epoll.h>

#define MAX_EVENTS 64

static int epoll_fd = -1;
static volatile int break_loop = 0;

void _set_main_thread( void ){}

void eventHandler(int fd, short flags, void *arg) {
    socket_info *sock = (socket_info *)arg;
    short event_type = 0;

    if (flags & EPOLLIN) event_type |= EVENT_READ;
    if (flags & EPOLLOUT) event_type |= EVENT_WRITE;
    if (flags & EPOLLERR || flags & EPOLLHUP) event_type |= EVENT_TIMEOUT;

    handleer(fd, event_type, arg);
}

void initEvents(void) {
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1 failed");
        exit(1);
    }
}

void freeEvents(void) {
    if (epoll_fd >= 0) close(epoll_fd);
    epoll_fd = -1;
}

void breakEvents(void) {
    break_loop = 1;
}

char waitEvents(void) {
    struct epoll_event events[MAX_EVENTS];
    while (!break_loop) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0 && errno != EINTR) {
            perror("epoll_wait failed");
            return -1;
        }
        for (int i = 0; i < n; ++i) {
            socket_info *sock = (socket_info *)events[i].data.ptr;
            eventHandler(sock->socket, events[i].events, sock);
        }
    }
    break_loop = 0;
    return 0;
}

static void modifyEpoll(socket_info *sock, uint32_t events, int persist) {
    struct epoll_event ev;
    ev.data.ptr = sock;
    ev.events = events | (persist ? 0 : EPOLLONESHOT);

    if (sock->registered) {
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock->socket, &ev);
    } else {
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock->socket, &ev);
        sock->registered = 1;
    }
}

void addEventSocketRead(socket_info *sock) {
    modifyEpoll(sock, EPOLLIN, 0);
}

void addEventSocketWrite(socket_info *sock) {
    modifyEpoll(sock, EPOLLOUT, 0);
}

void addEventSocketReadPersist(socket_info *sock) {
    modifyEpoll(sock, EPOLLIN, 1);
}

void addEventSocketWritePersist(socket_info *sock) {
    modifyEpoll(sock, EPOLLOUT, 1);
}

void addEventSocketReadWritePersist(socket_info *sock) {
    modifyEpoll(sock, EPOLLIN | EPOLLOUT, 1);
}

void delEventSocketReadPersist(socket_info *sock) {
    delEventSocketAll(sock);
}

void delEventSocketWritePersist(socket_info *sock) {
    delEventSocketAll(sock);
}

void delEventSocketAll(socket_info *sock) {
    if (sock->registered) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock->socket, NULL);
        sock->registered = 0;
    }
}

void deleteEvent(socket_info *sock) {
    delEventSocketAll(sock);
}

#ifdef WEBSERVER_USE_SSL
void commitSslEventFlags(socket_info *sock) {
    modifyEpoll(sock, sock->ssl_event_flags, 1);
}
#endif

#endif
