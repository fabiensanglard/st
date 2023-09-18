#include "netlink.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <cstring>

#include <linux/netlink.h>
#include <linux/cn_proc.h>
#include <linux/connector.h>
#include <sys/socket.h>
#include <unistd.h>

#include "track.h"
#include "utils.h"
#include "proc.h"

#define SEND_MESSAGE_LEN (NLMSG_LENGTH(sizeof(struct cn_msg) + sizeof(enum proc_cn_mcast_op)))
#define RECV_MESSAGE_LEN (NLMSG_LENGTH(sizeof(struct cn_msg) + sizeof(struct proc_event)))
#define SEND_MESSAGE_SIZE    (NLMSG_SPACE(SEND_MESSAGE_LEN))
#define RECV_MESSAGE_SIZE    (NLMSG_SPACE(RECV_MESSAGE_LEN))

static int BUFF_SIZE  = std::max((int)std::max(SEND_MESSAGE_SIZE, RECV_MESSAGE_SIZE), 1024);

/*     PARENT       CHILD
 *   TGID   PID   TGID   PID
 *
 *    X             B     B       X forked into B
 *    A             A     X       A created thread X
 *
 *    On fork, if TGID tracked, track child TGID
 *    On thread, if CHILD TGID tracked, count
 */
static void onFork(proc_event *ev) {
    if (ev->event_data.fork.child_pid !=
        ev->event_data.fork.child_tgid) {

        // This is a new thread
        if (tracked(ev->event_data.fork.child_tgid)) {
            IncThreads();
            log("%s:parent(pid,tgid)=%d,%d\tchild(pid,tgid)=%d,%d\n",
                "NEW_THREAD ",
                ev->event_data.fork.parent_pid,
                ev->event_data.fork.parent_tgid,
                ev->event_data.fork.child_pid,
                ev->event_data.fork.child_tgid);
        }
    } else {
        // This is a new process
        if (tracked(ev->event_data.fork.parent_tgid) ||
            tracked(ev->event_data.fork.child_tgid)) {
//            log("Event New Process from tracked tgid\n");
            IncThreads();
            IncProcesses();
            log("%s:parent(pid,tgid)=%d,%d\tchild(pid,tgid)=%d,%d\n",
                "NEW_PROCESS ",
                ev->event_data.fork.parent_pid,
                ev->event_data.fork.parent_tgid,
                ev->event_data.fork.child_pid,
                ev->event_data.fork.child_tgid);
            track(ev->event_data.fork.child_tgid);
        }
    }
}





static void onExec(proc_event *ev) {
    int pid = ev->event_data.exec.process_pid;
    std::string cmdline = getCmdline(pid);
//    printf("EXEC:pid=%d,tgid=%d\t[%s]\n",
//        ev->event_data.exec.process_pid,
//        ev->event_data.exec.process_tgid,
//        cmdline.c_str());
    printf("\033[0;31m");
    printf("EXEC");
    printf("\033[0m");
    printf(": [%s]\n",
           cmdline.c_str());
}

static void onExit(proc_event *ev) {
    log("EXIT:pid=%d, tgid=%d\texit code=%d\n",
        ev->event_data.exit.process_pid,
        ev->event_data.exit.process_tgid,
        ev->event_data.exit.exit_code);
    untrack(ev->event_data.exit.process_pid);
}

static void onUid(proc_event *ev) {
    log("UID:pid=%d,%d ruid=%d,euid=%d\n",
        ev->event_data.id.process_pid, ev->event_data.id.process_tgid,
        ev->event_data.id.r.ruid, ev->event_data.id.e.euid);
}

static void onGid(proc_event *ev) {
    log("gid change: pid=%d tgid=%d from %d to %d\n",
        ev->event_data.id.process_pid,
        ev->event_data.id.process_tgid,
        ev->event_data.id.r.rgid,
        ev->event_data.id.e.egid);
}

void HandleMsg(struct cn_msg *cn_hdr) {
    struct proc_event *ev = (struct proc_event *) cn_hdr->data;
    switch (ev->what) {
        case proc_event::PROC_EVENT_NONE:
            log("Listen request received\n");
            break;
        case proc_event::PROC_EVENT_FORK:
            onFork(ev);
            break;
        case proc_event::PROC_EVENT_EXEC:
            onExec(ev);
            break;
        case proc_event::PROC_EVENT_UID:
            onUid(ev);
            break;
        case proc_event::PROC_EVENT_GID:
            onGid(ev);
            break;
        case proc_event::PROC_EVENT_EXIT:
            onExit(ev);
            break;
        case proc_event::PROC_EVENT_SID:
        case proc_event::PROC_EVENT_PTRACE:
        case proc_event::PROC_EVENT_COMM:
        case proc_event::PROC_EVENT_COREDUMP:
            break;
        default:
            log("Unhandled message %d\n", ev->what);
            break;
    }
}


void readFromNetlink(int netlink_socket) {
    struct sockaddr_nl from_nla{};
    char b[BUFF_SIZE];

    struct sockaddr_nl kern_nla{};
    kern_nla.nl_family = AF_NETLINK;
    kern_nla.nl_groups = CN_IDX_PROC;
    kern_nla.nl_pid = 1;

    memset(b, 0, sizeof(b));
    nlmsghdr *netlinkMsgHeader = (nlmsghdr *) b;
    memcpy(&from_nla, &kern_nla, sizeof(from_nla));

    socklen_t from_nla_len = sizeof(from_nla);
    ssize_t bytesReceived = recvfrom(netlink_socket, b, BUFF_SIZE, 0, (struct sockaddr *) &from_nla, &from_nla_len);

    if (from_nla.nl_pid != 0) {
        log("nl_pid != 0");
        return;
    }

    if (bytesReceived < 1) {
        log("bytesReceived < 1");
        return;
    }

    log("Received %d bytes\n", bytesReceived);
    while (NLMSG_OK(netlinkMsgHeader, bytesReceived)) {
        cn_msg *cn_hdr = (cn_msg *) NLMSG_DATA(netlinkMsgHeader);
        if (netlinkMsgHeader->nlmsg_type == NLMSG_NOOP)
            continue;
        if ((netlinkMsgHeader->nlmsg_type == NLMSG_ERROR) ||
            (netlinkMsgHeader->nlmsg_type == NLMSG_OVERRUN))
            break;
        HandleMsg(cn_hdr);
        if (netlinkMsgHeader->nlmsg_type == NLMSG_DONE)
            break;
        netlinkMsgHeader = NLMSG_NEXT(netlinkMsgHeader, bytesReceived);
    }
}

static void sendListenToNetlink(int netlink_socket) {
    union {
        struct cn_msg listen_msg = {
                .id = {
                        .idx = CN_IDX_PROC,
                        .val = CN_VAL_PROC,
                },
                .seq = 0,
                .ack = 0,
                .len = sizeof(enum proc_cn_mcast_op)
        };
        char bytes[sizeof(struct cn_msg) + sizeof(enum proc_cn_mcast_op)];
    } buf;

    *((enum proc_cn_mcast_op *) buf.listen_msg.data) = PROC_CN_MCAST_LISTEN;

    if (send(netlink_socket, &buf, sizeof(buf), -2) != sizeof(buf)) {
        perror("failed to send proc connector mcast ctl op!\n");
        exit(EXIT_FAILURE);
    }
}

static void bindToNetlink(int netlink_socket) {
    struct sockaddr_nl my_nla{};
    my_nla.nl_family = AF_NETLINK;
    my_nla.nl_groups = CN_IDX_PROC;
    my_nla.nl_pid = getpid();

    struct sockaddr_nl kern_nla{};
    kern_nla.nl_family = AF_NETLINK;
    kern_nla.nl_groups = CN_IDX_PROC;
    kern_nla.nl_pid = 1;

    int err = bind(netlink_socket, (struct sockaddr *) &my_nla, sizeof(my_nla));
    if (err == -1) {
        perror("binding netlink_socket error");
        exit(EXIT_FAILURE);
    }
}

int InitNetlink() {
    // Le netlink socket
    int netlink_socket = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
    if (netlink_socket == -1) {
        perror("socket netlink_socket error");
        exit(EXIT_FAILURE);
    }

    bindToNetlink(netlink_socket);
    sendListenToNetlink(netlink_socket);
    return netlink_socket;
}