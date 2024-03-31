//
// File created on: 2024/03/28
// Author: Zizhou

#ifndef RR_KVRAFT_COMMAND_H
#define RR_KVRAFT_COMMAND_H

#include <string>
#include <fmt/format.h>
#include "RaftRegistry/rpc/serializer.h"

// 这个文件为kvraft模块定义了一些常量、枚举和结构体，用于表示键值存储的命令请求和响应

namespace RR::kvraft {
using namespace RR::rpc;

// 定义一个常量字符串，代表服务端处理命令的函数名，用于 RPC 调用
inline const std::string COMMAND = "KVServer::handleCommand";

// 下面这些常量主要用于定义和识别不同的键值存储事件和主题

inline const std::string KEYEVENTS_PUT = "put";
inline const std::string KEYEVENTS_DEL = "del";
inline const std::string KEYEVENTS_APPEND = "append";

inline const std::string TOPIC_KEYEVENT = "__keyevent__:";
inline const std::string TOPIC_KEYSPACE = "__keyspace__:";

// 所有 key 的事件
inline const std::string TOPIC_ALL_KEYEVENTS = "__keyevent__:*";
// 所有 key 的 put 事件
inline const std::string TOPIC_KEYEVENT_PUT = TOPIC_KEYEVENT + KEYEVENTS_PUT;
// 所有 key 的 del 事件
inline const std::string TOPIC_KEYEVENT_DEL = TOPIC_KEYEVENT + KEYEVENTS_DEL;
// 所有 key 的 append 事件
inline const std::string TOPIC_KEYEVENT_APPEND = TOPIC_KEYEVENT + KEYEVENTS_APPEND;

// 定义一个错误码枚举，包括操作成功、键不存在、错误的领导者、超时和关闭
enum Error {
    OK,
    NO_KEY,
    WRONG_LEADER,
    TIMEOUT,
    CLOSED
};

// 定义一个函数，将错误码转换为可读的字符串
inline std::string toString(Error err) const {
    switch (err){
        case OK:
            str = "OK";
            break;
        case NO_KEY:
            str = "NO KEY";
            break;
        case WRONG_LEADER:
            str = "WRONG LEADER";
            break;
        case TIMEOUT:
            str = "TIMEOUT";
            break;
        case CLOSED:
            str = "CLOSED";
            break;
        default:
            str = "unexpected error code";
            break;
    }
    return str;
}

// 定义一个操作类型枚举，包括获取、放置、追加、删除和清除
enum Operation {
    GET,
    PUT,
    APPEND,
    DELETE,
    CLEAR
};

inline std::string toString(Operation op) const {
    switch (op){
        case GET:
            str = "GET";
            break;
        case PUT:
            str = "PUT";
            break;
        case APPEND:
            str = "APPEND";
            break;
        case DELETE:
            str = "DELETE";
            break;
        case CLEAR:
            str = "CLEAR";
            break;
        default:
            str = "unexpected operation";
            break;
    }
    return str;
}

// 定义一个结构体，表示命令请求。包括操作类型、键、值、客户端 ID 和命令 ID。提供了一个 `toString` 方法用于生成该结构的字符串表示
struct CommandRequest {
    Operation op;
    std::string key;
    std::string value;
    int64_t clientId;
    int64_t commandId;
    std::string toString() const {
        std::string str = fmt.format("op: {} key: {} value: {} clientId: {} commandId: {}", toString(op), key, value, clientId, commandId);
        return "{" + str + "}";
    }
};

// 定义一个结构体，表示命令响应。包括错误码、值和领导者 ID。提供了一个 `toString` 方法用于生成该结构的字符串表示
struct CommandResponse {
    Error err = OK;
    std::string value;
    int64_t leaderId = -1;
    std::string toString() const {
        std::string str = fmt.format("error: {} value: {} leaderId: {}", toString(err), value, leaderId);
        return "{" + str + "}";
    }
};
}

#endif // RR_KVRAFT_COMMAND_H