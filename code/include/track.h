#pragma once

#include <cstdint>
#include <vector>

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

extern std::vector<Event> events;


void Track(int pid);
void Untrack(int pid);
bool Tracked(int pid);


long GetMaxCombinedPss();

void IncThreads();
void IncProcesses();
uint64_t NumThreads();
uint64_t NumProcs();
void SnapshotPss();