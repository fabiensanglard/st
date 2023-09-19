#include "utils.h"

#include <cstdint>
#include <ctime>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <csignal>

uint64_t toMs(struct timeval &val) {
    return val.tv_sec * 1000 + val.tv_usec / 1000;
}

// Return time in milliseconds.
uint64_t GetTimeMs() {
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return spec.tv_nsec / 1000000 + spec.tv_sec * 1000;
}

static bool kLogEnable = false;
void Log(const char *fmt, ...) {
    if (!kLogEnable) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
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