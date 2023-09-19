#pragma once

#include <string>

std::string GetCmdline(int pid);
uint64_t GetPSS(int pid);
void Declare(int pid, const std::string& cmdline);

