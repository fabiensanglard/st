#pragma once

#include <string>

std::string getCmdline(int pid);
uint64_t GetPSS(int pid);

void declare(int pid, const std::string& cmdline);

