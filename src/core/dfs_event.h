#ifndef DFS_EVENT_H
#define DFS_EVENT_H

#include "dfs_types.h"
#include "dfs_queue.h"
#include "dfs_rbtree.h"

#define EVENT_HAVE_EPOLL       1 // use epoll
#define EVENT_HAVE_CLEAR_EVENT 1

#define EVENT_TIMER_INFINITE   (rb_msec_t) -1
#define EVENT_TIMER_LAZY_DELAY 300

typedef void (*event_handler_pt)(event_t *ev);

struct event_s 
{
    void            *data;
    uint32_t         write:1;
    uint32_t         accepted:1; 
    uint32_t         instance:1;
    uint32_t         last_instance:1;
    uint32_t         active:1;
    uint32_t         ready:1;
    uint32_t         timedout:1;
    uint32_t         timer_set:1;
    uint32_t         timer_event:1;
    uint32_t         delayed:1;
    event_handler_pt handler;
    rbtree_node_t    timer;
    queue_t          post_queue;
    int              available; 
};

/*
 * The event filter requires to read/write the whole data:
 * select, poll, /dev/poll, kqueue, epoll.
 */
#define EVENT_USE_LEVEL_EVENT      0x00000001

/*
 * The event filter is deleted after a notification without an additional
 * syscall: kqueue, epoll.
 */
#define EVENT_USE_ONESHOT_EVENT    0x00000002

/*
 * The event filter notifies only the changes and an initial level:
 * kqueue, epoll.
 */
#define EVENT_USE_CLEAR_EVENT      0x00000004

/*
 * The event filter has kqueue features: the eof flag, errno,
 * available data, etc.
 */
#define EVENT_USE_KQUEUE_EVENT     0x00000008

/*
 * The event filter supports low water mark: kqueue's NOTE_LOWAT.
 * kqueue in FreeBSD 4.1-4.2 has no NOTE_LOWAT so we need a separate flag.
 */
#define EVENT_USE_LOWAT_EVENT      0x00000010

/*
 * The event filter requires to do i/o operation until EAGAIN: epoll, rtsig.
 */
#define EVENT_USE_GREEDY_EVENT    0x00000020

/*
 * The event filter is epoll.
 */
#define EVENT_USE_EPOLL_EVENT     0x00000040

/*
 * No need to add or delete the event filters: rtsig.
 */
#define EVENT_USE_RTSIG_EVENT      0x00000080

/*
 * No need to add or delete the event filters: overlapped, aio_read,
 * aioread, io_submit.
 */
#define EVENT_USE_AIO_EVENT        0x00000100

/*
 * Need to add socket or handle only once: i/o completion port.
 * It also requires DFS_HAVE_AIO and EVENT_USE_AIO_EVENT to be set.
 */
#define EVENT_USE_IOCP_EVENT       0x00000200

/*
 * The event filter has no opaque data and requires file descriptors table:
 * poll, /dev/poll, rtsig.
 */
#define EVENT_USE_FD_EVENT         0x00000400

/*
 * The event module handles periodic or absolute timer event by itself:
 * kqueue in FreeBSD 4.4, NetBSD 2.0, and MacOSX 10.4, Solaris 10's event ports.
 */
#define EVENT_USE_TIMER_EVENT      0x00000800

/*
 * All event filters on file descriptor are deleted after a notification:
 * Solaris 10's event ports.
 */
#define EVENT_USE_EVENTPORT_EVENT  0x00001000

/*
 * The event filter support vnode notifications: kqueue.
 */
#define EVENT_USE_VNODE_EVENT      0x00002000


/*
 * The event filter is deleted just before the closing file.
 * Has no meaning for select and poll.
 * kqueue, epoll, rtsig, eventport:  allows to avoid explicit delete,
 *                                   because filter automatically is deleted
 *                                   on file close,
 *
 * /dev/poll:                        we need to flush POLLREMOVE event
 *                                   before closing file.
 */
#define EVENT_CLOSE_EVENT    1

/*
 * disable temporarily event filter, this may avoid locks
 * in kernel malloc()/free(): kqueue.
 */
#define EVENT_DISABLE_EVENT  2

/*
 * event must be passed to kernel right now, do not wait until batch processing.
 */
#define EVENT_FLUSH_EVENT    4


/* these flags have a meaning only for kqueue */
#define EVENT_LOWAT_EVENT    0
#define EVENT_VNODE_EVENT    0

#if (EVENT_HAVE_EPOLL)
#define EVENT_READ_EVENT     EPOLLIN
#define EVENT_WRITE_EVENT    EPOLLOUT

#define EVENT_LEVEL_EVENT    0
#define EVENT_CLEAR_EVENT    0
#define EVENT_ONESHOT_EVENT  0x70000000

#elif (DFS_HAVE_POLL)
#define EVENT_READ_EVENT     POLLIN
#define EVENT_WRITE_EVENT    POLLOUT

#define EVENT_LEVEL_EVENT    0
#define EVENT_ONESHOT_EVENT  1
#else  /* use select */
#define EVENT_READ_EVENT     0
#define EVENT_WRITE_EVENT    1

#define EVENT_LEVEL_EVENT    0
#define EVENT_ONESHOT_EVENT  1
#endif

#ifndef EVENT_CLEAR_EVENT
#define EVENT_CLEAR_EVENT    0    /* dummy declaration */
#endif

#define event_init            epoll_init
#define event_process_events  epoll_process_events
#define event_add             epoll_add_event
#define event_delete          epoll_del_event
#define event_add_conn        epoll_add_connection
#define event_del_conn        epoll_del_connection

#define event_fd(p)           (((conn_t *) (p))->fd)

enum 
{
    EVENT_UPDATE_TIME = 0x01,
    EVENT_POST_EVENTS = 0x02
};

int event_handle_read(event_base_t *base, event_t *rev, uint32_t flags);
int event_del_read(event_base_t *base, event_t *rev);
int event_handle_write(event_base_t *base, event_t *wev, size_t lowat);
int event_del_write(event_base_t *base, event_t *wev);
void event_process_posted(volatile queue_t *posted, log_t *log);

#endif

