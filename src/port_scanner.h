#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ProcessInfo {
    int pid = 0;
    std::string name;
    std::string user;
    std::string proto;      // TCP / UDP
    std::string address;    // 127.0.0.1:8080
    std::string exePath;
    std::string killCommand;
};

struct ScanResult {
    bool ok = false;
    uint16_t port = 0;
    std::vector<ProcessInfo> processes;
    std::string error;
};

ScanResult ScanPort(uint16_t port);
bool TerminateProcess(const ProcessInfo &info, std::string &errorMessage);
std::string PlatformName();
