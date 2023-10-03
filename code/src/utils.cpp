#include "utils.h"

#include <stdint.h>
#include <unistd.h>
#include <ctime>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <string>
#include <pwd.h>

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

std::string GetUser(uid_t uid) {
    struct passwd *pws;
    pws = getpwuid(uid);
    return pws->pw_name;
}

static void SetEuid(uid_t id) {
    if (seteuid(id) != 0) {
        perror("seteuid");
        exit(EXIT_FAILURE);
    }
}

static void SetGeuid(uid_t id) {
    if (setegid(id) != 0) {
        perror("setgid");
        exit(EXIT_FAILURE);
    }
}

// This is only called after we checked that geteuid is 0.
// Therefore, there are two options here. Either program
// was run using `sudo` or it was run directly from root
// superuser account.
void DropRoot() {

    uid_t ruid, euid, suid;
    getresuid(&ruid, &euid, &suid);
    Log("ruid=%d, euid=%d, suid=%d\n", euid, euid, suid);

    // Not effectively running as root, error.
    if (geteuid() != 0) {
        Log("Error: euid is not root (got %d(%s))", geteuid(), GetUser(geteuid()).c_str());
        exit(EXIT_FAILURE);
    }

    // If the program was run with set-user-id bit, the getuid will be non-root
    if (getuid() != 0) {
        SetEuid(getuid());
        SetGeuid(getgid());
        Log("Dropped set-user-id privileges to %d(%s)\n", getuid(), GetUser(getuid()).c_str());
        return;
    }

    // Detect if command was run with sudo, using env variables.
    const char *sudo_uid = secure_getenv("SUDO_UID");
    const char *sudo_gid = secure_getenv("SUDO_GID");
    if (sudo_uid == nullptr || sudo_gid == nullptr) {
        Log("Error: Cannot drop root (no sudo env variables)\n");
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

    SetGeuid(gid);
    SetEuid(uid);


    // check if we successfully dropped the root privileges
    if (setuid(0) == 0 || seteuid(0) == 0) {
        printf("Failed to drop root privileges!\n");
        exit(EXIT_FAILURE);
    }

    Log("Dropped sudo privileges to %d(%s)\n", getuid(), GetUser(getuid()).c_str());
}
