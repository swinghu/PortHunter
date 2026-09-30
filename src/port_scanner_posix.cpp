#include "port_scanner.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __APPLE__
#include <libproc.h>
#endif

namespace {

enum class Socket { Tcp, Udp };

std::string Trim(const std::string &text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::vector<std::string> SplitWhitespace(const std::string &line, size_t maxTokens) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    while (pos < line.size() && tokens.size() < maxTokens) {
        while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) ++pos;
        const auto start = pos;
        while (pos < line.size() && !std::isspace(static_cast<unsigned char>(line[pos]))) ++pos;
        if (pos > start) tokens.push_back(line.substr(start, pos - start));
    }
    return tokens;
}

std::string BaseName(const std::string &path) {
    const auto slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string ExePathOf(int pid) {
#ifdef __APPLE__
    std::array<char, PROC_PIDPATHINFO_MAXSIZE> buffer{};
    const auto length = proc_pidpath(pid, buffer.data(), static_cast<uint32_t>(buffer.size()));
    return length > 0 ? std::string(buffer.data(), static_cast<size_t>(length)) : std::string();
#else
    std::array<char, 4096> buffer{};
    const auto link = "/proc/" + std::to_string(pid) + "/exe";
    const auto length = readlink(link.c_str(), buffer.data(), buffer.size() - 1);
    return length > 0 ? std::string(buffer.data(), static_cast<size_t>(length)) : std::string();
#endif
}

std::string UserOfPid(int pid) {
    struct stat status {};
    if (stat(("/proc/" + std::to_string(pid)).c_str(), &status) != 0) return "";
    const auto *entry = getpwuid(status.st_uid);
    return entry && entry->pw_name ? entry->pw_name : std::to_string(status.st_uid);
}

std::vector<std::string> RunCommand(const std::string &command) {
    std::vector<std::string> lines;
    std::unique_ptr<FILE, int (*)(FILE *)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) return lines;
    std::array<char, 1024> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get())) {
        auto line = Trim(std::string(buffer.data()));
        if (!line.empty()) lines.push_back(std::move(line));
    }
    return lines;
}

bool CommandExists(const std::string &name) {
    return !RunCommand("command -v " + name + " 2>/dev/null").empty();
}

void AddProcess(std::vector<ProcessInfo> &out, uint16_t port, ProcessInfo info) {
    if (info.pid <= 0) return;
    const auto duplicate = std::any_of(out.begin(), out.end(), [&](const ProcessInfo &existing) {
        return existing.pid == info.pid && existing.proto == info.proto && existing.address == info.address;
    });
    if (duplicate) return;
    if (info.name.empty()) info.name = BaseName(ExePathOf(info.pid));
    if (info.user.empty()) info.user = UserOfPid(info.pid);
    if (info.address.empty()) info.address = ":" + std::to_string(port);
    info.exePath = ExePathOf(info.pid);
    info.killCommand = "kill -9 " + std::to_string(info.pid);
    out.push_back(std::move(info));
}

// lsof columns: COMMAND PID USER FD TYPE DEVICE SIZE/OFF NODE NAME [ (LISTEN) ]
void ScanWithLsof(std::vector<ProcessInfo> &out, uint16_t port, Socket socket) {
    const auto portText = std::to_string(port);
    const auto command = socket == Socket::Udp ? "lsof -nP -iUDP:" + portText + " 2>/dev/null"
                                               : "lsof -nP -iTCP:" + portText + " -sTCP:LISTEN 2>/dev/null";
    for (const auto &line : RunCommand(command)) {
        if (line.rfind("COMMAND", 0) == 0) continue;
        const auto tokens = SplitWhitespace(line, 12);
        if (tokens.size() < 9) continue;
        ProcessInfo info;
        info.name = tokens[0];
        info.pid = std::atoi(tokens[1].c_str());
        info.user = tokens[2];
        info.proto = tokens[7];
        info.address = tokens[8];
        AddProcess(out, port, std::move(info));
    }
}

// ss output: LISTEN 0 128 127.0.0.1:8080 0.0.0.0:* users:(("node",pid=1234,fd=18))
void ScanWithSs(std::vector<ProcessInfo> &out, uint16_t port, const std::string &protoFlag) {
    const auto command = "ss -Hln" + protoFlag + " '( sport = :" + std::to_string(port) + " )' 2>/dev/null";
    for (const auto &line : RunCommand(command)) {
        const auto tokens = SplitWhitespace(line, 5);
        if (tokens.size() < 4) continue;
        ProcessInfo info;
        info.proto = protoFlag == "t" ? "TCP" : "UDP";
        info.address = tokens[3];
        const auto pidMarker = line.find("pid=");
        if (pidMarker == std::string::npos) {
            AddProcess(out, port, std::move(info));
            continue;
        }
        info.pid = std::atoi(line.c_str() + pidMarker + 4);
        const auto quote = line.rfind('"', pidMarker);
        const auto open = line.rfind("\"", quote == std::string::npos ? pidMarker : quote - 1);
        if (quote != std::string::npos && open != std::string::npos && quote > open) {
            info.name = line.substr(open + 1, quote - open - 1);
        }
        AddProcess(out, port, std::move(info));
    }
}

}  // namespace

ScanResult ScanPort(uint16_t port) {
    ScanResult result;
    result.port = port;

#ifdef __APPLE__
    if (!CommandExists("lsof")) {
        result.error = "系统缺少 lsof，无法查询端口占用";
        return result;
    }
    ScanWithLsof(result.processes, port, Socket::Tcp);
    ScanWithLsof(result.processes, port, Socket::Udp);
#else
    if (CommandExists("ss")) {
        ScanWithSs(result.processes, port, "t");
        ScanWithSs(result.processes, port, "u");
    } else if (CommandExists("lsof")) {
        ScanWithLsof(result.processes, port, Socket::Tcp);
        ScanWithLsof(result.processes, port, Socket::Udp);
    } else {
        result.error = "系统缺少 ss 与 lsof，无法查询端口占用";
        return result;
    }
#endif

    result.ok = true;
    return result;
}

bool TerminateProcess(const ProcessInfo &info, std::string &errorMessage) {
    if (info.pid <= 0) {
        errorMessage = "无效的 PID";
        return false;
    }
    const std::string command = "kill -9 " + std::to_string(info.pid);
    const int status = std::system(command.c_str());
    if (status != 0) {
        errorMessage = "kill -9 " + std::to_string(info.pid) + " 执行失败，可能需要更高权限";
        return false;
    }
    return true;
}

std::string PlatformName() {
#ifdef __APPLE__
    return "macOS";
#else
    return "Linux";
#endif
}
