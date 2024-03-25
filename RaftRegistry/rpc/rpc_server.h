//
// File created on: 2024/03/23
// Author: Zizhou

#ifndef RAFTREGISTRY_RPC_SERVER_CPP
#define RAFTREGISTRY_RPC_SERVER_CPP

#include "RaftRegistry/net/tcp_server.h"
#include "RaftRegistry/rpc/serializer.h"
#include "RaftRegistry/rpc/rpc_session.h"
#include "RaftRegistry/net/address.h"
#include "RaftRegistry/common/traits.h"
#include "RaftRegistry/rpc/rpc.h"
#include "RaftRegistry/rpc/protocol.h"
#include <functional>
#include <memory>
#include <map>
#include <list>
#include <libgo/libgo.h>

namespace RR::rpc {

class RpcServer : public TcpServer {
public:
    using ptr = std::shared_ptr<RpcServer>;
    using MutexType = co::co_mutex;


    RpcServer();
    ~RpcServer();

    // 绑定地址
    bool bind(Address::ptr address) override;
    // 绑定注册中心地址
    bool bindRegistry(Address::ptr address);
    // 启动服务器
    void start() override;
    // 停止服务器
    void stop() override;

    /**
     * @brief 注册方法模板
     * 
     * @tparam Func 要注册的方法的类型
     * @param name 要注册的方法的名称
     * @param func 要注册的方法
     */
    template <typename Func>
    void registerMethod(const std::string& name, Func func) {
        m_handlers[name] = [func, this] (Serializer serializer, const std::string& arg) {
            proxy(func, serializer, arg);
        }
    }

    /**
     * @brief 设置rpc服务器名称
     * 
     * @param name 名字
     */
    void setName(const std::string& name) override;

    /**
     * @brief 对一个频道发布消息
     * 
     * @param channel 频道名
     * @param message 消息
     */
    void publish(const std::string& channel, const std::string& message);
    
protected:
    /**
     * @brief 像服务注册中心发起注册
     * 
     * @param name 用于注册的函数名
     */
    void registerService(const std::string& name);

    /**
     * @brief 调用服务端注册的函数，返回序列化完的结果
     * 
     * @param name 要调用的函数名
     * @param arg 函数参数字节流
     * @return Serializer 函数调用的序列化结果
     * 
     * @details 该函数一般由客户端调用，该函数会将函数名和参数序列化后发送给服务端，然后等待服务端返回结果
     */
    Serializer call(const std::string& name, const std::string& arg);

    /**
     * @brief 调用代理
     * 
     * @tparam F 调用的函数的类型
     * @param func 函数
     * @param serializer 返回调用结果
     * @param arg 函数参数字节流
     * 
     * @details 该函数一般用于服务器处理客户端的call函数发来的rpc，
     *          当RPC服务器接收到客户端的远程方法调用请求后，服务器需要找到并执行相应的方法。
     *          proxy函数在这里发挥作用，它充当客户端请求与服务器本地方法之间的代理或桥梁。
     *          proxy负责解析请求，包括反序列化方法名和参数，然后在服务器上找到对应的方法并执行。
     *          执行完毕后，proxy还需要处理执行结果，将其序列化并准备发送回客户端。
     */
    template <typename F>
    void proxy(F func, Serializer serializer, const std::string& arg) {
        typename function_traits<F>::stl_function_type func(fun);
        using Return = typename function_traits<F>::return_type;
        using Args = typename function_traits<F>::tuple_type;

        // 反序列化参数arg
        Serializer s(arg);
        Args args;
        try {
            s >> args;
        } catch (...) {
            Result<Return> val;
            val.setCode(RPC_NO_MATCH);
            val.setMsg("params not match");
            serializer << val;
            return;
        }

        // 之所以不直接用Return rt{}，是因为return_type_t会对void类型进行特化
        return_type_t<Return> rt{};

        constexpr auto size = std::tuple_size<typename std::decay<Args>::type>::value;
        auto invoke = [&func, &args]<std::size_t... Index>(std::index_sequence<Index...>) {
            return func(std::get<Index>(std::forward<Args>(args))...);
        }

        if constexpr (std::is_same_v<Return, void>) { // 如果函数的返回类型是void，则无需返回值
            invoke(std::make_index_sequence<size>{}); // 调用函数
        } else { // 否则需要返回值
            rt = invoke(std::make_index_sequence<size>{}); // 调用函数
        }

        Result<Return> val;
        val.setCode(RPC_SUCCESS);
        val.setMsg("success");
        val.setVal(rt);
        serializer << val;
    }

    /**
     * @brief 处理客户端连接
     * @param[in] client 客户端套接字
     */
    void handleClient(Socket::ptr client) override;

    /**
     * @brief 处理客户端过程调用请求
     */
    Protocol::ptr handleMethodCall(Protocol::ptr proto);

    /**
     * @brief 处理心跳包
     */
    Protocol::ptr handleHeartbeatPacket(Protocol::ptr proto);

    /**
     * @brief 处理发布订阅
     */
    Protocol::ptr handlePubsubRequest(Protocol::ptr proto, Socket::ptr client);
    
    /**
     * @brief 服务器端用于处理客户端订阅的请求
     * 
     * @param channel 要订阅的频道
     * @param client 客户端套接字
     */
    void subscribe(const std::string& channel, Socket::ptr client);
    void unsubscribe(const std::string& channel, Socket::ptr client);
    void patternSubscribe(const std::string& pattern, Socket::ptr client);
    void patternUnsubscribe(const std::string& pattern, Socket::ptr client);

private:
    // 保存注册的函数
    std::map<std::string, std::function<void(Serializer, const std::string&)>> m_handlers;
    // 服务中心连接
    RpcSession::ptr m_registry;
    // 服务中心心跳定时器
    CycleTimerTocken m_heartbeatTimer;
    // 开放服务端口
    uint32_t m_port;
    // 和客户端的心跳时间， 默认40s
    uint64_t m_aliveTime;
    // 用于于保存所有频道的订阅关系
    std::map<std::string, std::list<Socket::ptr>> m_pubsubChannels;
    // 保存所有模式订阅关系
    std::list<std::pair<std::string, Socket::ptr>> m_patternChannels;

    MutexType m_pubsubMutex;

};

}

#endif // RAFTREGISTRY_RPC_SERVER_CPP