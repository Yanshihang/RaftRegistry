//
// File created on: 2024/03/22
// Author: Zizhou

#ifndef RAFTREGISTRY_RPC_PUBSUB_H
#define RAFTREGISTRY_RPC_PUBSUB_H

#include <string>
#include <memory>

namespace RR::rpc {
// 发布订阅消息类型的枚举，定义了各种发布订阅操作的消息类型
enum class PubsubMsgType {
    Publish,               // 发布消息
    Message,               // 消息通知
    Subscribe,             // 订阅操作
    Unsubscribe,           // 取消订阅操作
    PatternMessage,        // 带模式的消息通知
    PatternSubscribe,      // 带模式的订阅操作
    PatternUnsubscribe,    // 带模式的取消订阅操作
};

// pattern是指一个用于匹配多个频道的模式字符串
// 通过使用pattern，订阅者可以订阅匹配特定模式的一组频道。
// 例如，如果一个订阅者订阅了模式news.*，那么它可以接收到所有以news. 开头的频道的消息，如news.sports、news.weather等。

// 发布订阅请求的结构体，封装了请求的类型、频道、消息和模式
struct PubsubRequest {
    PubsubMsgType type; // 消息类型
    std::string channel; // 频道名称
    std::string message; // 消息内容
    std::string pattern; // 模式字符串，用于模式匹配的订阅
};

// 发布订阅响应的结构体，封装了响应的类型、频道和模式
struct PubsubResponse {
    PubsubMsgType type;
    std::string channel;
    std::string pattern;
};

// 发布订阅监听器
class PubsubListener {
public:
    using ptr = std::shared_ptr<PubsubListener>;

    virtual ~PubsubListener() {}

    /**
     * @brief 处理接收到的消息
     * 
     * @param channel 频道
     * @param message 消息
     */
    virtual void onMessage(const std::string& channel, const std::string& message) {}

    /**
     * @brief 处理订阅成功事件
     * 
     * @param channel 
     */
    virtual void onSubscribe(const std::string& channel) {}

    /**
     * @brief 处理取消订阅的事件
     * 
     * @param channel 
     */
    virtual void onUnsubscribe(const std::string& channel) {}

    /**
     * @brief 处理带模式的消息接收事件
     * @param pattern 模式
     * @param channel 频道
     * @param message 消息
     */
    virtual void onPatternMessage(const std::string& patter, const std::string& channel, const std::string& message) {}

    virtual void onPatternSubscribe(const std::string& pattern) {}

    virtual void onPatternUnSubscribe(const std::string& pattern) {}
};


}


#endif //RAFTREGISTRY_RPC_PUBSUB_H