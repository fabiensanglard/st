#include "proc.h"

#include "utils.h"

#include <string>
#include <unordered_map>
#include <cstring>
#include <stdlib.h>

#include <unistd.h>
#include <fcntl.h>

// Local cache for pid -> cmdline
std::unordered_map<int, std::string> pidCmdlines;


void Declare(int pid, const std::string& cmdline) {
  pidCmdlines[pid] = cmdline;
}

std::string GetCmdline(int pid) {
    if (pidCmdlines.contains(pid)) {
        return pidCmdlines[pid];
    }

    // Get cmdLine
    ssize_t CMDLINE_BUFF_SIZE = 1024;
    char cmdline[CMDLINE_BUFF_SIZE];
    memset(&cmdline, 0, CMDLINE_BUFF_SIZE);

    char cmdlinePath[1024];
    snprintf(cmdlinePath, sizeof(cmdlinePath), "/proc/%d/cmdline", pid);
    Log("Opening '%s'\n", cmdlinePath);
    int fd = open(cmdlinePath, O_RDONLY);
    if (fd <= 0) {
        Log("Unable to open '%s'\n", cmdlinePath);
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
