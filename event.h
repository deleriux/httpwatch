#ifndef _EVENT_H_
#define _EVENT_H_

#define EVENT_MAXFDS 1048576
#include <sys/epoll.h>

void event_init(void);
/* Returns number of events handled or -1 on error */
int event_loop(int max, int timeout);
void event_del_fd(int fd);
int event_stop_fd(int fd);
int event_start_fd(int fd);
int event_mod_event(int fd, int event);
void event_mod_cb(int fd, int (*callback)(int fd, int event, void *data));
int event_add_fd(
                int fd,
                int (*callback)(int fd, int event, void *data),
                void (*destroy)(int fd, void *data),
                void *data,
                int event);
#endif
