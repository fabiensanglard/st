#pragma once

#include <stdint.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>

uint64_t GetTimeMs();
uint64_t toMs(struct timeval &val);
void Log(const char *fmt, ...);
void DropRoot(uid_t uid);
