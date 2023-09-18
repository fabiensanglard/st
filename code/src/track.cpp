#include "track.h"

#include <unordered_set>
#include <unordered_map>

#include "utils.h"
#include "proc.h"

std::vector<Event> events;

int numThread = 0;
int numProcesses = 0;

std::unordered_set<int> trackedPids;

static void dumpTrack(const char *prefix) {
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

long GetMaxCombinedPss() {
    std::unordered_map<uint64_t , uint64_t> psss;
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

void IncThreads() {
    numThread++;
}

void IncProcesses() {
    numProcesses++;
}

uint64_t NumThreads() {
    return numThread;
}

uint64_t NumProcs() {
    return numProcesses;
}


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