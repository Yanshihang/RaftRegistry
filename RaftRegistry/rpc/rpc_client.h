//
// File created on: 2024/03/24
// Author: Zizhou

#ifndef RR_RPC_CLIENT_H
#define RR_RPC_CLIENT_H

#include <memory>
#include <functional>
#include <libgo/libgo.h>
#include <atomic>
#include <string>
#include <map>
#include "RaftRegistry/common/util.h"
#include "RaftRegistry/rpc/rpc_session.h"
#include "RaftRegistry/rpc/protocol.h"
#include "RaftRegistry/rpc/serializer.h"
#include "RaftRegistry/rpc/pubsub.h"
#include "RaftRegistry/net/address.h"
#include "RaftRegistry/rpc/rpc.h"
#include "RaftRegistry/common/traits.h"

namespace RR::rpc {

/**
 * @brief RPC客户端
 * @details
 * 开启一个 send 协程，通过 Channel 接收调用请求，并转发 request 请求给服务器。
 * 开启一个 recv 协程，负责接收服务器发送的 response 响应并通过序列号获取对应调用者的 Channel，
 * 将 response 放入 Channel 唤醒调用者。
 * 所有调用者的 call 请求将通过 Channel 发送给 send 协程， 然后开启一个 Channel 用于接收 response 响应，
 * 并将请求序列号与自己的 Channel 关联起来。
 */
class RpcClient : public std::enable_shared_from_this<RpcClient> {
public:
    using ptr = std::shared_ptr<RpcClient>;
    using MutexType = co::co_mutex;

    RpcClient();
    ~RpcClient();

    // 关闭 RPC 客户端，断开与服务器的连接，清理资源
    void close();

    /**
     * @brief 设置是否开启自动心跳包
     * 
     * @param isAuto 
     */
    void setHeartbeat (bool isAuto) {
        m_autoHeartbeat = isAuto;
    }

    /**
     * @brief 连接一个RPC服务器
     * @param[in] address 服务器地址
     */
    bool connect(Address::ptr address);

    /**
     * @brief 设置RPC调用超时时间
     * @param[in] timeoutMs 超时时间，单位毫秒
     */
    void setTimeout(uint64_t timeoutMs);

    /**
     * @brief 有参数的rpc调用函数
     * @param[in] name 函数名
     * @param[in] ps 可变参数
     * @return 返回调用结果
     */
    template <typename R, typename... Params>
    Result<R> call (const std::string& name, Params... params) {
        using argsType = std::tuple<typename std::decay<Params>::type...>;
        argsType args = std::make_tuple(params...);
        Serializer s;
        ss << name << args;
        s.reset();
        return call<R>(s);
    }

    /**
     * @brief 无参数的rpc调用函数
     * @param[in] name 函数名
     * @return 返回调用结果
     */
    template <typename R>
    Result<R> call (const std::string& name) {
        Serializer s;
        s << name;
        s.reset();
        return call<R>(s);
    }

    template <typename... Params>
    void callback(const std::string& name, Params&&... params) {
        // 确保至少有一个参数，这个参数应该是回调函数
        static_assert(sizeof...(ps), "without a callback function");
        // 使用tuple存储所有参数
        auto tp = std::make_tuple(params...);
        // 获取元组大小，即参数个数
        constexpr auto size = std::tuple_size<typename std::decay<decltype(tp)>::type>value;
        // 获取最后一个元素，即回调函数
        auto cb = std::get<size-1>(tp);
        // 检查回调函数的形参数量是否为1，不是则编译失败
        static_assert(function_traits<decltype(cb)>{}.arity == 1, "callback type not support");
        // 获取cb的第一个参数类型
        using res = typename function_traits<decltype(cb)>::template args<0>::type;
        // 获取
        using rt = typename res::rowType;
        // 静态断言，检查 cb 是否可以被调用，参数类型为 Result<rt>，如果不能被调用，编译时会报错
        static_assert(std::is_invocable_v<decltype(cb), Result<rt>>, "callback type not support");
        RpcClient::ptr self = shared_from_this();

        // 
        go [cb = std::move(cb), name, tp = std::move(tp), self, this] {
            auto proxy = [&cb, &name, &tp, this]<std::size_t... Index>(std::index_sequence<Index...>) {
                cb(call<rt>(name, std::get<Index>(tp)...));
            }
            proxy(std::make_index_sequence<size-1>{});
            // 使用 self，防止当前对象在协程执行期间被销毁
            // 在协程中，由于协程的异步性质，协程的执行可能会跨越多个作用域。这就可能导致在协程开始执行时存在的对象，在协程执行过程中被销毁。
            reinterpret_cast<void>(self);
        }
    }

    /**
     * @brief 异步调用函数
     * 
     * @tparam R 远程函数的返回类型
     * @tparam Params 远程函数的参数类型
     * @param name 要调用的远程函数
     * @param ps 远程函数所需的参数
     * @return co_chan<Result<R>> 结果
     */
    template <typename R, typename... Params>
    co_chan<Result<R>> async_call(const std::string& name, Params&&... ps) {
        // 保存返回值
        co_chan<Result<R>> chan;
        // 这行代码获取了当前对象的 shared_ptr 并将其存储在 self 中。这是为了防止当前对象在协程执行期间被销毁。
        RpcClient::ptr self = shared_from_this();
        go [self, chan, name, ps..., this] () mutable {
            chan << call<R>(name, ps...);
            self = nullptr;
        };
        return chan;
    }

    // 是否正在订阅频道
    bool isSubscribe();

    /**
     * 取消订阅频道
     * @param channel 频道
     */
    void unsubscribe(const std::string& channel);

    /**
     * @brief 订阅频道，会阻塞
     * @tparam Args std::string ...
     * @tparam Listener 用于监听订阅事件
     * @param channels 订阅的频道列表
     * @return 当成功取消所有的订阅后返回 true，连接断开或其他导致订阅失败将返回 false
     */
    template <typename ...Args>
    bool subscribe(PubsubListener::ptr listener, const Args... channels) {
        static_assert(sizeof...(channels));
        return subscribe(std::move(listener), {channels...});
    }

    /**
     * @brief 订阅频道
     * 
     * @param listener 用于监听订阅事件
     * @param channels 订阅的频道列表
     * @return true 成功订阅所有频道
     * @return false 
     */
    bool subscribe(PubsubListener::ptr listener, const std::vector<std::string>& channels);
    
    /**
     * @brief 模式订阅，阻塞
     * 
     * @details 匹配规则：
     *          '?': 匹配 0 或 1 个字符
     *          '*': 匹配 0 或更多个字符
     *          '+': 匹配 1 或更多个字符
     *          '@': 如果任何模式恰好出现一次
     *          '!': 如果任何模式都不匹配
     * @tparam Args std::string ...
     * @param listener 用于监听订阅事件
     * @param patterns 订阅的模式列表
     * @return 当成功取消所有的订阅后返回 true，连接断开或其他导致订阅失败将返回 false
     */
    template <typename ...Args>
    void patternSubscribe(PubsubListener::ptr listener, const Args& ... patterns) {
        static_assert(sizeof...(patterns));
        return patternSubscribe(std::move(listener), {patterns...});
    }

    bool patternSubscribe(PubsubListener::ptr listener, const std::vector<std::string>& patterns);
    
    void patternUnsubscribe(const std::string& pattern);

    /**
     * 向一个频道发布消息
     * @param channel 频道
     * @param message 消息
     * @return 是否发生成功
     */
    bool publish(const std::string& channel, const std::string& message);

    template <typename T>
    bool publish(const std::string& channel, const T& data) {
        Serializer s;
        s << data;
        s.reset();
        return publish(channel, s.toString());
    }

    Socket::ptr getSocket() {
        return m_session->getSocket();
    }

    bool isClosed () {
        return !m_session || !m_session->isConnected();
    }

private:
    /**
     * @brief rpc 连接对象的发送协程，通过 Channel 收集调用请求，并转发请求给服务器。
     */
    void handleSend();

    /**
     * @brief rpc 连接对象的接收协程，负责接收服务器发送的 response 响应并根据响应类型进行处理
     */
    void handleRecv();

    // 处理发布订阅消息，将收到的发布订阅消息分发给相应的监听器
    void handlePublish(Protocol::ptr proto);

    // 通过序列号获取对应调用者的 Channel，将 response 放入 Channel 唤醒调用者
    void recvProtocol(Protocol::ptr response);

    // 发送请求协议，并等待响应。返回响应协议和是否超时的标志
    std::pair<Protocol::ptr, bool> sendProtocol(Protocol::ptr request);

    /**
     * @brief 实际调用
     * @param[in] s 序列化完的请求
     * @return 返回调用结果
     */
    template <typename R>
    Result<R> call(Serializer s) {
        // 用于存储调用的结果
        Result<R> val;

        // 检查会话是否存在并且是否已经连接
        if (!m_session || !m_session->isConnected()) { // 如果会话不存在或者没有连接，那么设置结果的代码为 RPC_CLOSED，设置结果的消息为 "socket closed"，然后返回结果。
            val.setCode(RPC_CLOSED);
            val.setMsg("socket closed");
            return val;
        }

        // 创建协议请求
        Protocol::ptr request = Protocol::Create(Protocol::MsgType::RPC_METHOD_REQUEST, s.toString());

        // 发送协议请求，并获取相应和是否超时
        auto [response, timeout] = sendProtocol(request);

        if (timeout) { // 检测是否超时
            val.setCode(RPC_TIMEOUT);
            val.setMsg("rpc call timeout");
            return val;
        }

        if (!response) { // 检测是否有响应
            val.setCode(RPC_CLOSED);
            val.setMsg("socket closed");
            return val;
        }

        if (response->getContent().empty()) {
            val.setCode(RPC_NO_METHOD);
            val.setMsg("no method");
            return val;
        }

        Serializer serializer(response->getContent());
        try {
            serializer >> val;
        } catch(...) {
            val.setCode(RPC_NO_MATCH);
            val.setMsg("return value not match");
        }
        return val;
    }

private:
    // 是否自动开启心跳包
    bool m_autoHeartbeat = true;
    // 表示 RPC 客户端是否已关闭
    std::atomic_bool m_isClose = true;
    // 表示心跳包是否已关闭
    std::atomic_bool m_isHeartClose = true;
    // 用于接收协程的关闭信号的Channel
    co::co_chan<bool> m_recvCloseChan;
    // RPC 调用的超时时间
    uint64_t m_timeout = -1;
    // 服务器的连接会话
    RpcSession::ptr m_session;
    // 序列号
    uint32_t m_sequenceId = 0;
    // 序列号到对应调用者协程的channel映射
    std::map<uint32_t, co::co_chan<Protocol::ptr>> m_responseHandle;
    // responseHandle的互斥锁
    MutexType m_mutex;
    // 消息发送通道，用于收集外部请求并发送给服务器
    co::co_chan<Protocol::ptr> m_chan;
    // 心跳定时器，用于定期发送心跳包维持连接
    CycleTimerTocken m_heartTimer;
    // 订阅监听器
    PubsubListener::ptr m_listener;
    // 订阅的频道, key 为频道名，value 为订阅的频道；其中 value 为 true 表示协程主动取消订阅， false 表示订阅有客户端关闭而被动种植
    std::map<std::string, co::co_chan<bool>> m_subs;
    // 保护订阅信息的互斥锁
    MutexType m_pubsubMutex;
};
}

#endif //RR_RPC_CLIENT_H