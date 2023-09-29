#include "utils.h"

#include <stdint.h>
#include <unistd.h>
#include <ctime>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <sys/prctl.h>

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

void DropRoot(uid_t uid) {
    if (setresuid(uid, uid, uid) != 0) {
        fprintf(stderr, "dropping privileges failed\n");
        exit(EXIT_FAILURE);
    }

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        fprintf(stderr, "PR_SET_NO_NEW_PRIVS failed");
        exit(EXIT_FAILURE);
    }
}
