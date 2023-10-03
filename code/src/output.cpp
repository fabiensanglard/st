#include "output.h"
#include "track.h"
#include "utils.h"

#include <locale.h>
#include <cstdio>
#include <unordered_map>
#include <algorithm>

#include <sys/wait.h>

static void GenerateASCII(FILE* out, uint64_t totalDurationMs) {
    static uint64_t cwidth = 85;
    static uint64_t cheight = 15;

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
    float bracketWidth = (float)totalDurationMs / (float)cwidth;
    uint64_t maxPss = GetMaxCombinedPss();

    for (const auto pss: psss) {
      uint64_t timestamp = pss.first - minTimestamp;
      uint64_t bracket = (uint64_t)(timestamp / bracketWidth);
      bracket = std::min(cwidth-1, bracket);
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
    } else {
        fprintf(out, "0XB");
    }
    fprintf(out, "┗");
    for (int i = 0 ; i < cwidth ; i++ ) {
        const char* v =  i == cwidth/2 ? "┳" : "━";
        fprintf(out, "%s", v);
    }
    fprintf(out, "┛\n");

    // Draw bottom text
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


void GenerateOutputs(int pid, uint64_t startTimeMs) {
    // It's output time!
    printf("Num threads = %lu\n", NumThreads());
    printf("Num process = %lu\n", NumProcs());
    setlocale(LC_NUMERIC, "");
    printf("Max PSS: %'zu bytes\n", GetMaxCombinedPss());

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
    printf("Walltime: %'zums - user-space: %'zums - kernel-space: %'zums\n",
           durationMs,
           toMs(cmdStats.ru_utime),// + toMs(childStats.ru_utime),
           toMs(cmdStats.ru_stime)// + toMs(childStats.ru_stime))
    );



    if (!events.empty()) {
        GenerateASCII(stdout, durationMs);
    }
}

void InitOutput() {
    // No buffering
    setvbuf(stdout, nullptr, _IONBF, 0);
//    setvbuf(stderr, NULL, _IONBF, 0);
}
