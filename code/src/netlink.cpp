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

static int BUFF_SIZE = std::max((int) std::max(SEND_MESSAGE_SIZE, RECV_MESSAGE_SIZE), 1024);

/*     PARENT       CHILD
 *   TGID   PID   TGID   PID
 *
 *    X             B     B       X forked into B
 *    A             A     X       A created thread X
 *
 *    On fork, if TGID tracked, Track child TGID
 *    On thread, if CHILD TGID Tracked, count
 */
static void OnFork(proc_event *ev) {
    if (ev->event_data.fork.child_pid !=
        ev->event_data.fork.child_tgid) {

        // This is a new thread
        if (Tracked(ev->event_data.fork.child_tgid)) {
            IncThreads();
            Log("%s:parent(pid,tgid)=%d,%d\tchild(pid,tgid)=%d,%d\n",
                "NEW_THREAD ",
                ev->event_data.fork.parent_pid,
                ev->event_data.fork.parent_tgid,
                ev->event_data.fork.child_pid,
                ev->event_data.fork.child_tgid);
        }
    } else {
        // This is a new process
        if (Tracked(ev->event_data.fork.parent_tgid) ||
            Tracked(ev->event_data.fork.child_tgid)) {
            IncThreads();
            IncProcesses();
            Log("%s:parent(pid,tgid)=%d,%d\tchild(pid,tgid)=%d,%d\n",
                "NEW_PROCESS ",
                ev->event_data.fork.parent_pid,
                ev->event_data.fork.parent_tgid,
                ev->event_data.fork.child_pid,
                ev->event_data.fork.child_tgid);
            Track(ev->event_data.fork.child_tgid);
        }
    }
}


static void OnExec(proc_event *ev) {
    int pid = ev->event_data.exec.process_pid;
    std::string cmdline = GetCmdline(pid);
//    printf("EXEC:pid=%d,tgid=%d\t[%s]\n",
//        ev->event_data.exec.process_pid,
//        ev->event_data.exec.process_tgid,
//        cmdline.c_str());
    if (Tracked(pid)) {  
      printf("\033[0;31m"); // Draw it in red
      printf("EXEC");
      printf("\033[0m");
      printf(": [%s]\n", cmdline.c_str());
    }
}

static void OnExit(proc_event *ev) {
    Log("EXIT:pid=%d, tgid=%d\texit code=%d\n",
        ev->event_data.exit.process_pid,
        ev->event_data.exit.process_tgid,
        ev->event_data.exit.exit_code);
    Untrack(ev->event_data.exit.process_pid);
}

static void OnUid(proc_event *ev) {
    Log("UID:pid=%d,%d ruid=%d,euid=%d\n",
        ev->event_data.id.process_pid, ev->event_data.id.process_tgid,
        ev->event_data.id.r.ruid, ev->event_data.id.e.euid);
}

static void OnGid(proc_event *ev) {
    Log("gid change: pid=%d tgid=%d from %d to %d\n",
        ev->event_data.id.process_pid,
        ev->event_data.id.process_tgid,
        ev->event_data.id.r.rgid,
        ev->event_data.id.e.egid);
}

void HandleMsg(struct cn_msg *cn_hdr) {
    struct proc_event *ev = (struct proc_event *) cn_hdr->data;
    switch (ev->what) {
        case proc_event::PROC_EVENT_NONE:
            Log("Listen request received\n");
            break;
        case proc_event::PROC_EVENT_FORK:
            OnFork(ev);
            break;
        case proc_event::PROC_EVENT_EXEC:
            OnExec(ev);
            break;
        case proc_event::PROC_EVENT_UID:
            OnUid(ev);
            break;
        case proc_event::PROC_EVENT_GID:
            OnGid(ev);
            break;
        case proc_event::PROC_EVENT_EXIT:
            OnExit(ev);
            break;
        case proc_event::PROC_EVENT_SID:
        case proc_event::PROC_EVENT_PTRACE:
        case proc_event::PROC_EVENT_COMM:
        case proc_event::PROC_EVENT_COREDUMP:
            break;
        default:
            Log("Unhandled message %d\n", ev->what);
            break;
    }
}


void ReadFromNetlink(int netlink_socket) {
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
        Log("nl_pid != 0");
        return;
    }

    if (bytesReceived < 1) {
        Log("bytesReceived < 1");
        return;
    }

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

// This does not work :(
//static void SendMCastListen(int netlink_socket) {
//    union {
//        struct cn_msg listen_msg = {
//                .id = {
//                        .idx = CN_IDX_PROC,
//                        .val = CN_VAL_PROC,
//                },
//                .seq = 0,
//                .ack = 0,
//                .len = sizeof(enum proc_cn_mcast_op)
//        };
//        char bytes[sizeof(struct cn_msg) + sizeof(enum proc_cn_mcast_op)];
//    } buf;
//
//    *((enum proc_cn_mcast_op *) buf.listen_msg.data) = PROC_CN_MCAST_LISTEN;
//
//    if (send(netlink_socket, &buf, sizeof(buf), -2) != sizeof(buf)) {
//        perror("failed to send proc connector mcast ctl op!\n");
//        exit(EXIT_FAILURE);
//    }
//}

static void SendMCastListen(int netlink_socket) {
    char buff[BUFF_SIZE];
    struct nlmsghdr* message = (struct nlmsghdr *)buff;
    struct cn_msg* payload = (struct cn_msg *)NLMSG_DATA(message);

    memset(buff, 0, sizeof(buff));

    enum proc_cn_mcast_op* op = (enum proc_cn_mcast_op*)&payload->data[0];
    *op = PROC_CN_MCAST_LISTEN;

    message->nlmsg_len = SEND_MESSAGE_LEN;
    message->nlmsg_type = NLMSG_DONE;
    message->nlmsg_flags = 0;
    message->nlmsg_seq = 0;
    message->nlmsg_pid = getpid();

    payload->id.idx = CN_IDX_PROC;
    payload->id.val = CN_VAL_PROC;
    payload->seq = 0;
    payload->ack = 0;
    payload->len = sizeof(enum proc_cn_mcast_op);
    if (send(netlink_socket, message, message->nlmsg_len, 0) != message->nlmsg_len) {
        perror("failed to send proc connector mcast ctl op!\n");
        exit(EXIT_FAILURE);
    }
}

static void BindToNetlink(int netlink_socket) {
    struct sockaddr_nl my_nla{};
    my_nla.nl_family = AF_NETLINK;
    my_nla.nl_groups = CN_IDX_PROC;
    my_nla.nl_pid = getpid();

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

    BindToNetlink(netlink_socket);
    SendMCastListen(netlink_socket);
    return netlink_socket;
}
