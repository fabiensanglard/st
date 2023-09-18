#pragma once

#include <cstdint>

#include <sys/time.h>
#include <sys/resource.h>

uint64_t GetTimeMs();
uint64_t toMs(struct timeval &val);
void log(const char *fmt, ...);
void DropRoot();