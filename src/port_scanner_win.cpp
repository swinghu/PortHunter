#include "port_scanner.h"

#include <algorithm>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN  // keep windows.h from pulling winsock.h, which clashes with winsock2.h
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <sddl.h>

namespace {

std::string BaseName(const std::string &path) {
    const auto slash = path.find_last_of("\\/");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string ProcessPath(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!process) return "";
    std::vector<char> buffer(MAX_PATH * 4);
    DWORD size = static_cast<DWORD>(buffer.size());
    std::string path;
    if (QueryFullProcessImageNameA(process, 0, buffer.data(), &size)) path.assign(buffer.data(), size);
    CloseHandle(process);
    return path;
}

std::string ProcessUser(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return "";
    std::string user;
    HANDLE token = nullptr;
    if (OpenProcessToken(process, TOKEN_QUERY, &token)) {
        DWORD needed = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
        std::vector<unsigned char> buffer(needed);
        auto *sid = reinterpret_cast<PTOKEN_USER>(buffer.data());
        if (GetTokenInformation(token, TokenUser, buffer.data(), needed, &needed)) {
            LPSTR text = nullptr;
            if (ConvertSidToStringSidA(sid->User.Sid, &text)) {
                DWORD nameSize = 0, domainSize = 0;
                SID_NAME_USE type;
                LookupAccountSidA("", text, nullptr, &nameSize, nullptr, &domainSize, &type);
                std::vector<char> name(nameSize), domain(domainSize);
                if (LookupAccountSidA("", text, name.data(), &nameSize, domain.data(), &domainSize, &type)) {
                    user = std::string(domain.data()) + "\\" + name.data();
                }
                LocalFree(text);
            }
        }
        CloseHandle(token);
    }
    CloseHandle(process);
    return user;
}

std::string FormatEndpoint(ULONG address, ULONG port) {
    char text[INET_ADDRSTRLEN] = {0};
    ULONG networkOrder = htonl(address);
    inet_ntop(AF_INET, &networkOrder, text, sizeof(text));
    return std::string(text) + ":" + std::to_string(ntohs(static_cast<unsigned short>(port)));
}

void CollectTcp(std::vector<ProcessInfo> &out, uint16_t port, bool ipv6) {
    const ULONG family = ipv6 ? AF_INET6 : AF_INET;
    const ULONG tableClass = TCP_TABLE_OWNER_PID_ALL;
    ULONG size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, family, tableClass, 0);
    if (size == 0) return;
    std::vector<unsigned char> buffer(size);

    if (!ipv6) {
        auto *table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID *>(buffer.data());
        if (GetExtendedTcpTable(table, &size, FALSE, family, tableClass, 0) != NO_ERROR) return;
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            const auto &row = table->table[i];
            if (ntohs(static_cast<unsigned short>(row.dwLocalPort)) != port) continue;
            ProcessInfo info;
            info.pid = static_cast<int>(row.dwOwningPid);
            info.proto = "TCP";
            info.address = FormatEndpoint(row.dwLocalAddr, row.dwLocalPort);
            out.push_back(std::move(info));
        }
    } else {
        auto *table = reinterpret_cast<MIB_TCP6TABLE_OWNER_PID *>(buffer.data());
        if (GetExtendedTcpTable(table, &size, FALSE, family, tableClass, 0) != NO_ERROR) return;
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            const auto &row = table->table[i];
            if (ntohs(static_cast<unsigned short>(row.ulLocalPort)) != port) continue;
            ProcessInfo info;
            info.pid = static_cast<int>(row.dwOwningPid);
            info.proto = "TCP6";
            info.address = "[::]:" + std::to_string(port);
            out.push_back(std::move(info));
        }
    }
}

void CollectUdp(std::vector<ProcessInfo> &out, uint16_t port, bool ipv6) {
    const ULONG family = ipv6 ? AF_INET6 : AF_INET;
    ULONG size = 0;
    GetExtendedUdpTable(nullptr, &size, FALSE, family, UDP_TABLE_OWNER_PID, 0);
    if (size == 0) return;
    std::vector<unsigned char> buffer(size);

    if (!ipv6) {
        auto *table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID *>(buffer.data());
        if (GetExtendedUdpTable(table, &size, FALSE, family, UDP_TABLE_OWNER_PID, 0) != NO_ERROR) return;
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            const auto &row = table->table[i];
            if (ntohs(static_cast<unsigned short>(row.dwLocalPort)) != port) continue;
            ProcessInfo info;
            info.pid = static_cast<int>(row.dwOwningPid);
            info.proto = "UDP";
            info.address = FormatEndpoint(row.dwLocalAddr, row.dwLocalPort);
            out.push_back(std::move(info));
        }
    } else {
        auto *table = reinterpret_cast<MIB_UDP6TABLE_OWNER_PID *>(buffer.data());
        if (GetExtendedUdpTable(table, &size, FALSE, family, UDP_TABLE_OWNER_PID, 0) != NO_ERROR) return;
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            const auto &row = table->table[i];
            if (ntohs(static_cast<unsigned short>(row.ulLocalPort)) != port) continue;
            ProcessInfo info;
            info.pid = static_cast<int>(row.dwOwningPid);
            info.proto = "UDP6";
            info.address = "[::]:" + std::to_string(port);
            out.push_back(std::move(info));
        }
    }
}

}  // namespace

ScanResult ScanPort(uint16_t port) {
    ScanResult result;
    result.port = port;

    std::vector<ProcessInfo> raw;
    CollectTcp(raw, port, false);
    CollectTcp(raw, port, true);
    CollectUdp(raw, port, false);
    CollectUdp(raw, port, true);

    // One process can hold several sockets on the same port; keep one row per (pid, proto, address).
    for (auto &entry : raw) {
        const auto duplicate = std::any_of(result.processes.begin(), result.processes.end(),
                                           [&](const ProcessInfo &existing) {
                                               return existing.pid == entry.pid && existing.proto == entry.proto &&
                                                      existing.address == entry.address;
                                           });
        if (duplicate) continue;
        entry.exePath = ProcessPath(static_cast<DWORD>(entry.pid));
        if (entry.name.empty()) entry.name = BaseName(entry.exePath);
        if (entry.name.empty()) entry.name = "(未知，可能无权限查看)";
        entry.user = ProcessUser(static_cast<DWORD>(entry.pid));
        if (entry.user.empty()) entry.user = "-";
        entry.killCommand = "taskkill /PID " + std::to_string(entry.pid) + " /F";
        result.processes.push_back(std::move(entry));
    }

    result.ok = true;
    return result;
}

bool TerminateProcess(const ProcessInfo &info, std::string &errorMessage) {
    if (info.pid <= 0) {
        errorMessage = "无效的 PID";
        return false;
    }
    std::string command = "taskkill /PID " + std::to_string(info.pid) + " /F";
    std::vector<char> writable(command.begin(), command.end());
    writable.push_back('\0');

    STARTUPINFOA startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION procInfo {};
    if (!CreateProcessA(nullptr, writable.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                        &procInfo)) {
        errorMessage = "无法启动 taskkill，错误码 " + std::to_string(GetLastError());
        return false;
    }
    WaitForSingleObject(procInfo.hProcess, 15000);
    DWORD exitCode = 0;
    GetExitCodeProcess(procInfo.hProcess, &exitCode);
    CloseHandle(procInfo.hThread);
    CloseHandle(procInfo.hProcess);

    if (exitCode != 0) {
        errorMessage = "taskkill 退出码 " + std::to_string(exitCode) + "，可能需要管理员权限";
        return false;
    }
    return true;
}

std::string PlatformName() { return "Windows"; }
