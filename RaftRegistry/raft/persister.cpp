//
// File created on: 2024/03/25
// Author: Zizhou

#include "persister.h"
#include <spdlog/spdlog.h>

namespace RR::raft {
static auto Logger = GetLoggerInstance();

Persister::Persister(const std::filesystem::path& persist_path = ".") : m_path(persist_path), m_snapshotter(persist_path / "snapshot") {
    // 检查持久化路径的有效性，如果无效则记录警告日志
    if (m_path.empty()) {
        SPDLOG_LOGGER_WARN(Logger, "persist path is empty");
    } else if (!std::filesystem::exists(m_path)) {
        SPDLOG_LOGGER_WARN(Logger, "persist path: {} is not exists, create a new directory", m_path.string());
        std::ifstream::create_directory(m_path);
    } else if (!std::filesystem::is_directory(m_path)) {
        SPDLOG_LOGGER_WARN(Logger, "persist path: {} is not a directory", m_path.string());
    } else {
        SPDLOG_LOGGER_INFO(Logger, "persist path: {}", getFullPathName());
    }
}

std::optional<HardState> Persister::loadHardState() {
    std::ifstream in(m_path / m_name, std::ios_base::in); // 以只读方式打开文件
    if (!in.is_open()) {
        return std::nullopt;
    }

    // 将文件中的内容复制到字符串中
    std::string str(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    rpc::Serializer s(str);
    HardState hs{};
    try {
        s >> hs;
    } catch (...) {
        return std::nullopt;
    }

    return hs;
}

std::optional<std::vector<Entry>> Persister::loadEntries() {
    std::unique_lock<MutexType> lock(m_mutex);
    std::ifstream in(m_path / m_name, std::ios_base::in);
    if (!in.is_open()) {
        return std::nullopt; // 文件打开失败，返回空值
    }

    std::string str(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    rpc::Serializer s(str);
    std::vector<Entry> entries;
    try {
        HardState hs{};
        s >> hs >> entries; // 反序列化硬状态和日志条目
    } catch (...) {
        return std::nullopt;
    }

    retur n entries;
}

Snapshot::ptr Persister::loadSnapshot() {
    std::unique_lock<MutexType> lock(m_mutex);
    return m_snapshotter.loadSnap();
}

int64_t Persister::getRaftStateSize() {
    std::unique_lock<MutexType> lock(m_mutex);
    std::ifstream in(m_path / m_name, std::ios_base::in);

    if (!in.is_open()) {
        return -1; // 文件打开失败，返回错误码
    }

    in.seekg(0, in.end);
    auto length = in.tellg();
    return length;
}

bool Persister::persist(const HardState& hs, const std::vector<Entry>& entries, const Snapshot::ptr snapshot = nullptr) {
    std::unique_lock<MutexType> lock(m_mutex);

    // 打开文件，如果有此文件则打开后清空内容，如果没有则创建
    int fd = open((m_path / m_name).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        return false;
    }

    // 将硬状态和日志条目序列化
    rpc::Serializer s;
    s << hs << entries;
    s.reset();

    // 将序列化数据写入文件
    std::string data = s.toString();
    if (write(fd, data.c_str(), data.size()) < 0) {
        return false;
    }

    // 持久化：将数据写入磁盘
    fsync(fd);
    close(fd);
    
    if (snapshot) {
        return m_snapshotter.saveSnap(snapshot);
    }
    return true;
}



} // namespace RR::raft