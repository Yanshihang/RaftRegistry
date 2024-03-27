//
// File created on: 2024/03/25
// Author: Zizhou

#include "RaftRegistry/raft/snapshot.h"
#include "RaftRegistry/common/util.h"
#include <spdlog/spdlog.h>

namespace RR::raft {
static auto Logger = GetLoggerInstance();

Snapshotter::Snapshotter(const std::filesystem::path& dir, const std::string& suffix) : m_dir(dir), m_snap_suffix(suffix) { 
    if (!m_dir.empty()) {
        SPDLOG_LOGGER_WARN(Logger, "snapshot path is empty");
    } else if (!std::filesystem::exists(m_dir)) {
        // 目录不存在时创建目录，并记录警告日志
        SPDLOG_LOGGER_WARN(Logger, "snapshot path: {} is not exists, create directory", m_dir);
        std::filesystem::create_directory(m_dir);
    } else if (!std::filesystem::is_directory(m_dir)) {
        // 路径不是目录时记录警告日志
        SPDLOG_LOGGER_WARN(Logger, "snapshot path: {} is not a directory", m_dir);
    }
}

bool Snapshotter::saveSnap(const Snapshot::ptr& snapshot) {
    if (!snapshot || snapshot.empty()) {
        return false;
    }
    return save(*snapshot);
}

bool Snapshotter::saveSnap(const Snapshot& snapshot) {
    if (snapshot.empty()) { // 快照为空或无效时，不进行存储并返回失败
        return false;
    }
    return save(snapshot);
}

Snapshot::ptr Snapshotter::loadSnap() {
    std::vector<std::string> names = snapNames(); // 获取快照文件名列表
    if (names.empty()) {
        return nullptr;
    }
    // 遍历快照文件名列表，从最新的快照开始尝试加载
    for (auto& name : names) {
        auto snapshot = read(name); // 尝试读取快照
        if (snapshot) {
            return snapshot;
        }
    }
    return nullptr;
}

std::vector<std::string> Snapshotter::snapNames() {
    // 存储快照文件名的列表
    std::vector<std::string> names;
    // 如果快照目录不存在或不是一个目录，直接返回空列表
    if (!std::filesystem::exists(m_dir) || !std::filesystem::is_directory()) {
        return {};
    }

    std::filesystem::directory_iterator dirIter(m_dir);
    for (auto& iter : dirIter) {
        if (iter.status().type() == std::filesystem::file_type::regular) {
            // 将常规文件的文件名添加到快照列表
            name.push_back(iter.path().filename().string());
        }
    }

    // 通过检查文件名后缀，过滤出合法的快照文件
    names = checkSuffix(names);
    // 按文件名降序排序快照列表，使最新的快照排在前面
    std::sort(names.begin(),names.end(),std::greater<>());
    return names;

}

std::vector<std::string> Snapshotter::checkSuffix(const std::vector<std::string>& names) {
    std::vector<std::string> validNames;
    for (auto& name : names) {
        // 检查文件名是否以指定的快照文件后缀结尾
        if (name.compare(name.size() - m_snap_suffix.size(), m_snap_suffix.size(), m_snap_suffix) == 0) {
            validNames.push_back(name);
        } else {
            SPDLOG_LOGGER_WARN(Logger, "skip unexpected non snapshot file {}", name);
        }
    }
    return validNames;
}

bool Snapshotter::save(const Snapshot& snapshot) {
    // 创建快照文件名
    // 快照名格式 %016ld-%016ld%s
    std::unique_ptr<char[]> snapName = std::make_unique<char[]>(16+1+16+m_snap_suffix.size()+1);
    sprintf(&snapName[0],"%016ld-%016ld%s",snapshot.metadata.term, snapshot.metadata.index, m_snap_suffix.c_str());
    std::string filename = m_dir / snapName.get();

    int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        return false;
    }

    // 序列化快照数据
    Serializer s;
    s << snapshot;
    s.reset();

    std::string data = s.str();
    // 将数据写入文件
    if (write(fd, data.c_str(), data.size()) < 0) {
        return false;
    }

    // 数据写入磁盘
    fsync(fd);

    close(fd);
    return true;
}

std::unique_ptr<Snapshot> Snapshotter::read(const std::string& snapname) {
    // 以二进制读取模式打开文件
    std::ifstream file(m_dir / snapname, std::ios::binary);

    // 定位到文件末尾以确定文件大小
    file.seekg(0, file.end);
    size_t length = file.tellg();

    file.seekg(0, file.beg);

    if (!length) {
        return nullptr;
    }

    // 读取文件内容到字符串中
    std::string data;
    data.resize(length);
    file.read(&data[0], length);

    rpc::Serializer s(data);
    // 创建一个新的快照对象
    std::unique_ptr<Snapshot> snapshot = std::make_unique<Snapshot>();
    // 从序列化数据中反序列化快照对象
    ser >> *snapshot;

    return snapshot;

}

}