//
// File created on: 2024/03/12
// Author: Zizhou

#include "config.h"
#include <spdlog/spdlog.h>

namespace RR {

// 创建一个静态的logger对象，用于记录日志，只允许在本文件中使用
static auto logger = GetLoggerInstance();

ConfigVarBase::ptr Config::LookUpBase(const std::string& name) {
    std::unique_lock<co_rmutex> lock(GetMutex().Reader());
    auto iter = GetDatas().find(name);
    if (iter == GetDatas().end()) {
        return nullptr;
    }
    return iter->second;
}

void Config::LoadFromFile(const std::string& path) {
    SPDLOG_LOGGER_INFO(logger, "LoadFromFile: {}", path);
    YAML::Node root = YAML::LoadFile(path);
    LoadFromYaml(root);
}

void Config::LoadFromYaml(const YAML::Node& node) {
    std::list<std::pair<std::string, YAML::Node>> allNodes;
    ListAllMembers("", node, allNodes);
    for (const auto& i : allNodes) {
        std::string key = i.first;
        if (key.empty()) {
            continue;
        }
        // 将key转换为小写
        std::transform(key.begin(),key.end(),key.begin(),::tolower);
        // 通过key查找对应的ConfigVarBase对象
        auto var = LookUpBase(key);
        if (var) {
            // 判断node是否是标量，如果是，则直接调用fromString进行配置
            // 如果不是，则将node转换为字符串，然后调用fromString进行配置
            if (i.second.IsScalar()) {
                var->fromString(i.second.Scalar());
            }else {
                std::stringstream ss;
                ss << i.second;
                var->fromString(ss.str());
            }
        }
    }
}

/**
 * @brief 列出某个节点下的所有子节点（包含自身）
 * 
 * @param prefix 前缀(名字)，如果为空则表示根节点，否则表示子节点；如果是子节点则前缀是父节点的名字加上一个点：parent.child
 * @param node 要检索的节点
 * @param result 所有节点的list
 */
static void ListAllMembers(const std::string& prefix, const YAML::Node& node, std::list<std::pair<std::string, YAML::Node>> result) {
    if (prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._") != std::string::npos) {
        SPDLOG_LOGGER_ERROR(logger, "Config invalid name: {}", prefix);
        return;
    }
    result.emplace_back(prefix, node);
    if (node.IsMap()) {
        for (const auto& i : node) {
            std::string name = prefix.empty() ? i.first.Scalar() : prefix + "." + i.first.Scalar();
            
            // 递归调用ListAllMembers，以遍历所有节点
            ListAllMembers(name,i.second, result);
        }
    }
}

void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb) {
    // 获取所有的ConfigVarMap，然后遍历执行回调函数
    auto& configMap = GetDatas();
    for (const auto& i : configMap) {
        cb(i.second);
    }
}


}