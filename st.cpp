#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <linux/connector.h>
#include <linux/netlink.h>
#include <linux/cn_proc.h>


#define SEND_MESSAGE_LEN (NLMSG_LENGTH(sizeof(struct cn_msg) + sizeof(enum proc_cn_mcast_op)))
#define RECV_MESSAGE_LEN (NLMSG_LENGTH(sizeof(struct cn_msg) + sizeof(struct proc_event)))
#define SEND_MESSAGE_SIZE    (NLMSG_SPACE(SEND_MESSAGE_LEN))
#define RECV_MESSAGE_SIZE    (NLMSG_SPACE(RECV_MESSAGE_LEN))
#define max(x, y) ((y)<(x)?(x):(y))
#define min(x, y) ((y)>(x)?(x):(y))
#define BUFF_SIZE (max(max(SEND_MESSAGE_SIZE, RECV_MESSAGE_SIZE), 1024))

int numThread = 0;
int numProcesses = 0;
bool kLogEnable = false;

void log(const char *fmt, ...) {
    if (!kLogEnable) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

// Return time in milliseconds.
uint64_t GetTimeMs() {
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return spec.tv_nsec / 1000000 + spec.tv_sec * 1000;
}

std::unordered_set<int> trackedPids;

constexpr int kSnapshotEveryMs = 1;

void dumpTrack(const char *prefix) {
    log("%s Tracking pids = {", prefix);
    int i = 0;
    for (const int &pid: trackedPids) {
        log("%d", pid);
        if (i != trackedPids.size() - 1) {
            log(", ");
        }
        i++;
    }
    log("} %d process %d threads\n", numProcesses, numThread);
}

void track(int pid) {
    trackedPids.insert(pid);
    dumpTrack("Add -> ");
}

void untrack(int pid) {
    trackedPids.erase(pid);
    dumpTrack("Rmv -> ");
}

bool tracked(int pid) {
    return trackedPids.contains(pid);
}

// We sum all of /proc/%d/smaps
uint64_t GetPSS(int pid) {
    char procStatusPath[512];
    snprintf(procStatusPath, sizeof(procStatusPath), "/proc/%d/smaps", pid);
    FILE *f = fopen(procStatusPath, "r");
    if (!f) {
        return 0;
    }
    char line[1024];
    long pss = 0;
    while (fgets(line, sizeof(line), f) != nullptr) {
        if (strncmp("Pss:", line, 4) == 0) {
            char *pch = strtok(line, " ");
            pch = strtok(nullptr, " ");
            pss += atoi(pch);
        }
    }
    fclose(f);
    return pss * 1024;
}

enum EventType {
    PSS
};

struct Pss {
    int pid;
    uint64_t value;
};

struct Event {
    uint64_t timestamp;
    enum EventType type;
    union {
        Pss pss;
    };
};

std::vector<Event> events;

void SnapshotPss() {
    uint64_t now = GetTimeMs();
    for (int pid: trackedPids) {
        uint64_t pss = GetPSS(pid);
        events.push_back({.timestamp = now,
                                 .type = PSS,
                                 .pss = {pid, pss}}
        );
    }
}


/*     PARENT       CHILD
 *   TGID   PID   TGID   PID
 *
 *    X             B     B       X forked into B
 *    A             A     X       A created thread X
 *
 *    On fork, if TGID tracked, track child TGID
 *    On thread, if CHILD TGID tracked, count
 */
void onFork(proc_event *ev) {
    if (ev->event_data.fork.child_pid !=
        ev->event_data.fork.child_tgid) {

        // This is a new thread
        if (tracked(ev->event_data.fork.child_tgid)) {
            numThread++;
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
            numThread++;
            numProcesses++;
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

std::unordered_map<int, std::string> pidCmdlines;

std::string getCmdline(int pid) {
    if (pidCmdlines.contains(pid)) {
        return pidCmdlines[pid];
    }

    // Get cmdLine
    ssize_t CMDLINE_BUFF_SIZE = 1024;
    char cmdline[CMDLINE_BUFF_SIZE];
    memset(&cmdline, 0, CMDLINE_BUFF_SIZE);

    char cmdlinePath[1024];
    snprintf(cmdlinePath, sizeof(cmdlinePath), "/proc/%d/cmdline", pid);
    log("Opening '%s'\n", cmdlinePath);
    int fd = open(cmdlinePath, O_RDONLY);
    if (fd <= 0) {
        log("Unable to open '%s'\n", cmdlinePath);
        pidCmdlines[pid] = "";
        return "";
    }

    ssize_t r = read(fd, cmdline, CMDLINE_BUFF_SIZE - 1);
    close(fd);

    // Replace null char with space (except the last one)
    for (int i = 0; r > 0 && i < r; ++i) {
        if (cmdline[i] == 0)
            cmdline[i] = ' ';
    }

    std::string name = std::string{cmdline};
    pidCmdlines[pid] = name;
    return name;
}

void onExec(proc_event *ev) {
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

void onExit(proc_event *ev) {
    log("EXIT:pid=%d, tgid=%d\texit code=%d\n",
        ev->event_data.exit.process_pid,
        ev->event_data.exit.process_tgid,
        ev->event_data.exit.exit_code);
    untrack(ev->event_data.exit.process_pid);
}

void onUid(proc_event *ev) {
    log("UID:pid=%d,%d ruid=%d,euid=%d\n",
        ev->event_data.id.process_pid, ev->event_data.id.process_tgid,
        ev->event_data.id.r.ruid, ev->event_data.id.e.euid);
}

void onGid(proc_event *ev) {
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

void sendListenToNetlink(int netlink_socket) {
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

void bindToNetlink(int netlink_socket) {
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

void DropRoot() {
    const char *sudo_uid = secure_getenv("SUDO_UID");
    const char *sudo_gid = secure_getenv("SUDO_GID");
    if (sudo_uid == nullptr || sudo_gid == nullptr) {
        // Not running with sudo, user is root and nothing we can drop here.
        return;
    }

    // no need to "drop" the privileges we don't have in the first place!
    if (getuid() != 0) {
        return;
    }

    // when invoked with sudo, getuid() will return 0 and we won't be able to drop your privileges
    uid_t uid;
    if ((uid = getuid()) == 0) {
        uid = (uid_t) strtoll(sudo_uid, nullptr, 10);
//        if (errno != 0) {
//            perror("under-/over-flow in converting `SUDO_UID` to integer");
//            exit(EXIT_FAILURE);
//        }
    }

    // again, in case we were invoked using sudo
    gid_t gid;
    if ((gid = getgid()) == 0) {
        gid = (gid_t) strtoll(sudo_gid, nullptr, 10);
//        if (errno != 0) {
//            perror("under-/over-flow in converting `SUDO_GID` to integer");
//            exit(EXIT_FAILURE);
//        }
    }

    if (setgid(gid) != 0) {
        perror("setgid");
        exit(EXIT_FAILURE);
    }
    if (setuid(uid) != 0) {
        perror("setgid");
        exit(EXIT_FAILURE);
    }

    // check if we successfully dropped the root privileges
    if (setuid(0) == 0 || seteuid(0) == 0) {
        printf("could not drop root privileges!\n");
        exit(EXIT_FAILURE);
    }
}

int forkAndExec(char *cmd, char **parameters, int numParameters) {
    log("forkAndExec %s", cmd);
    std::string cmdline = std::string();
    for (int i = 0; i < numParameters; i++) {
        log(" %s", parameters[i]);
        cmdline += " ";
        cmdline += parameters[i];
    }

    int pid = fork();
    if (pid == 0) { // This is the new process
        // Drop superuser privileges
        DropRoot();

        // We cannot reuse parameters as it, since it is not null terminated
        char *argv[numParameters + 1];
        for (int i = 0; i < numParameters; i++) {
            argv[i] = parameters[i];
            log("execv argv[%d]=%s\n", i, argv[i]);
        }
        argv[numParameters] = nullptr;

        log("execv argv[%d]=%s\n", numParameters, "NULL");
        int ret = execvp(cmd, argv);
        if (ret == -1) {
            log("Could not execv '%s'", cmd);
            perror("Unable to execv");
            exit(EXIT_FAILURE);
        }
    } else {
        track(pid);
        // For short-lived process, we may not be quick enough to poll /proc/PID/cmdline.
        // We cheat and pre-populate the cache here.
        pidCmdlines[pid] = cmdline;
    }
    return pid;
}

long GetMaxCombinedPss() {
    std::unordered_map<long, long> psss;
    for (const auto &event: events) {
        switch (event.type) {
            case PSS:
               if (!psss.contains(event.timestamp)) {
                  psss[event.timestamp] = 0;
               }
                  psss[event.timestamp] = psss[event.timestamp] +  event.pss.value;
               break;
            default:
                break;
        }

    }

    long maxPss = 0;
    for(auto pair: psss) {
        if (maxPss < pair.second) {
            maxPss = pair.second;
        }
    }
    return maxPss;
}

void GenerateASCII(FILE* out, uint64_t totalDurationMs) {
    static int64_t cwidth = 85;
    static int64_t cheight = 15;

    // We need to generate the PSS values for [0,cwidth-1]
    uint64_t psses[cwidth];
    std::fill_n(psses, cwidth, 0);

    // Transform events into combined pss for convenience
    // We use timestamp as the key and pss as the value
    std::unordered_map<long, long> psss;
    for (const auto &event: events) {
        switch (event.type) {
            case PSS:
                if (!psss.contains(event.timestamp)) {
                    psss[event.timestamp] = 0;
                }
                psss[event.timestamp] = psss[event.timestamp] +  event.pss.value;
                break;
            default:
                break;
        }

    }


    struct PssCal {
        uint64_t total = 0;
        uint64_t n = 0;
    };

    PssCal pssCalcs[cwidth];
    std::fill_n(pssCalcs, cwidth, PssCal{0, 0});

    uint64_t minTimestamp = events[0].timestamp;
    uint64_t maxTimestamp = events[events.size() - 1].timestamp;
    float bracketWidth = (float)totalDurationMs / (float)cwidth;
    uint64_t maxPss = GetMaxCombinedPss();

    for (const auto pss: psss) {
      uint64_t timestamp = pss.first - minTimestamp;
      uint64_t bracket = (uint64_t)(timestamp / bracketWidth);
      bracket = min(cwidth-1, bracket);
      pssCalcs[bracket].n++;
      pssCalcs[bracket].total += pss.second;
    }

    // Now calc average
    uint64_t lastAverage;
    for (int i = 0; i < cwidth; i++) {
        if (pssCalcs[i].total == 0) {
            psses[i] = lastAverage;
        } else {
            psses[i] = pssCalcs[i].total / pssCalcs[i].n;
            psses[i] = (psses[i] /(float)maxPss) * cheight;
        }
        lastAverage = psses[i];
    }




   //Draw top line
    uint64_t displayMaxPss = maxPss;
    while (displayMaxPss >= 1000) {
        displayMaxPss /= 1000;
    }
    fprintf(out, "%3lu", displayMaxPss);
    fprintf(out, "┏");
    for (int i = 0; i < cwidth; i++) {
        fprintf(out, "━");
    }
    fprintf(out, "┓\n");

    // Draw upper half
    for (int i = cheight - 1; i > cheight / 2; i--) {
        fprintf(out, "   ┃");
        for (int j = 0; j < cwidth; j++) {
            const char *v = psses[j] >= i ? "█" : " ";
            fprintf(out, "%s", v);
        }
        fprintf(out, "┃\n");
    }

    // Draw middle line
    fprintf(out, "   ┫");
    for (int j = 0; j < cwidth; j++) {
        const char *v = psses[j] >= (cheight / 2) ? "█" : " ";
        fprintf(out, "%s", v);
    }
    fprintf(out, "┃\n");

    // Draw bottom half
    for (int i = cheight / 2 - 1; i >= 0; i--) {
        fprintf(out, "   ┃");
        for (int j = 0; j < cwidth; j++) {
            const char *v = psses[j] >= i ? "█" : " ";
            fprintf(out, "%s", v);
        }
        fprintf(out, "┃\n");
    }

    // Draw bottom line
    if (maxPss < 1000) {
      fprintf(out, "0B ");
    } else if (maxPss < 1000000) {
        fprintf(out, "0KB");
    } else if (maxPss < 1000000000) {
        fprintf(out, "0MB");
    } else if (maxPss < 1000000000000) {
        fprintf(out, "0GB");
    } else if (maxPss < 1000000000000000) {
        fprintf(out, "0TB");
    } else {
        fprintf(out, "0XB");
    }
    fprintf(out, "┗");
    for (int i = 0 ; i < cwidth ; i++ ) {
        const char* v =  i == cwidth/2 ? "┳" : "━";
        fprintf(out, "%s", v);
    }
    fprintf(out, "┛\n");

    // Draw botton text
    fprintf(out, "   ");
    if (totalDurationMs < 1000) {
        fprintf(out, "0ms");
    } else if (totalDurationMs < 1000000){
        fprintf(out, "0s ");
    } else if (totalDurationMs < 1000000 * 60) {
        fprintf(out, "0mn");
    } else if (totalDurationMs < 1000000 * 60 * 24) {
       fprintf(out, "0hr");
     }

    for (int i = 0 ; i < cwidth -4; i++ ) {
        fprintf(out, " ");
    }
    while(totalDurationMs > 1000) {
        totalDurationMs /= 1000;
    }
    fprintf(out, "%3lu\n", totalDurationMs);
}

uint64_t toMs(struct timeval &val) {
    return val.tv_sec * 1000 + val.tv_usec / 1000;
}

int main(int argc, char **argv) {
    if (getuid() != 0) {
        log("Only root can startTimeMs/stop the fork connector\n");
        return 0;
    }

    for (int i = 0; i < argc; i++) {
        log("argv[%02d]:%s\n", i, argv[i]);
    }

    if (argc < 2) {
        log("No command to trace\n");
        return 0;
    }

    // No buffer for stdout
    setvbuf(stdout, NULL, _IONBF, 0);

    // Le netlink socket
    int netlink_socket = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
    if (netlink_socket == -1) {
        perror("socket netlink_socket error");
        exit(EXIT_FAILURE);
    }

    bindToNetlink(netlink_socket);
    sendListenToNetlink(netlink_socket);


    int epfd = epoll_create(1);
    if (epfd == -1) {
        perror("Cannot create epoll");
        exit(EXIT_FAILURE);
    }

    // Prepare epoll
    struct epoll_event ev;
    ev.data.fd = netlink_socket;
    ev.events = EPOLLIN; // Register for read availability
    int ctl = epoll_ctl(epfd, EPOLL_CTL_ADD, netlink_socket, &ev);
    if (ctl) {
        perror("epoll_ctl error");
        exit(EXIT_FAILURE);
    }

    // From here, we are receiving netlink events. We can create the process we want to observe.
    // We add the forked process to the list of tracked processes.
    long startTimeMs = GetTimeMs();
    int pid = forkAndExec(argv[1], &argv[1], argc - 1);

    long nextMemorySnapshot = GetTimeMs() - kSnapshotEveryMs;

    // Let's roll until all processes have run!
    while (!trackedPids.empty()) {
        const int kMaxEvents = 1;

        // Calculate timeout so we snapshot PSS at regular intervals.
        long timeOut = nextMemorySnapshot - GetTimeMs();
        if (timeOut < 0) timeOut = 0;

        struct epoll_event evlist[kMaxEvents];
        int ready = epoll_wait(epfd, evlist, kMaxEvents, timeOut);
        switch (ready) {
            case -1 : { // Error?
                if (errno == EINTR) {
                    continue;
                }

                log("epoll error");
                exit(EXIT_FAILURE);
                break;
            }
            case 0 : { // Timeout, this is time to snapshop PSS for all processes.
                SnapshotPss();
                nextMemorySnapshot = GetTimeMs() + kSnapshotEveryMs;
                break;
            }
            default: {
                for (int j = 0; j < ready; j++) {
                    if (evlist[j].events & EPOLLIN) {
                        readFromNetlink(evlist[j].data.fd);
                    } else if (evlist[j].events & (EPOLLHUP | EPOLLERR)) {
                        perror("Netlink hangup?\n");
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
    }
    close(netlink_socket);

    // Let's RIP the cmd process and gather some cmdStats
    struct rusage cmdStats{};
    int status;
    int waited = wait4(pid, &status, 0, &cmdStats);
    if (waited < 0) {
        perror("Could not wait4");
        exit(EXIT_FAILURE);
    }
    uint64_t endTimeMs = GetTimeMs();
    uint64_t durationMs = endTimeMs - startTimeMs;

//    struct rusage childStats{};
//    int usaged = getrusage(RUSAGE_CHILDREN, &childStats);


    // It's output time!
    printf("Num threads = %d\n", numThread);
    printf("Num process = %d\n", numProcesses);
    setlocale(LC_NUMERIC, "");
    printf("Max PSS: %'zu bytes\n", GetMaxCombinedPss());
    printf("Walltime: %'zums - user-space: %'zums - kernel-space: %'zums\n",
           durationMs,
           toMs(cmdStats.ru_utime),// + toMs(childStats.ru_utime),
           toMs(cmdStats.ru_stime)// + toMs(childStats.ru_stime))
    );

    DropRoot();

    if (!events.empty()) {
        GenerateASCII(stdout, durationMs);
    }

    return EXIT_SUCCESS;
}
