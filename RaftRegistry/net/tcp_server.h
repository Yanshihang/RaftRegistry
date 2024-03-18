//
// File created on: 2024/03/18
// Author: Zizhou

#ifndef RR_TCP_SERVER_H
#define RR_TCP_SERVER_H

#include <memory>
#include <vector>
#include <libgo/libgo.h>

#include "RaftRegistry/net/address.h"
#include "RaftRegistry/net/socket.h"

namespace RR {

class TcpServer {
public:
    TcpServer();
    virtual ~TcpServer();

    /**
     * @brief 绑定服务器到一个特定的网络地址
     * 
     * @param addr 和服务器绑定的地址
     * @return true 绑定成功
     * @return false 绑定失败
     */
    virtual bool bind(Address::ptr addr);

    /**
     * @brief 绑定服务器到多个网络地址，
     * 
     * @param addrs 和服务器绑定的地址列表
     * @param fail 绑定失败的地址列表
     */
    virtual bool bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fail);
    
    /**
     * @brief 启动服务器，开始监听和接收客户端连接
     */
    virtual void start();

    /**
     * @brief 定制服务器，关闭所有连接
     */
    virtual void stop();

    /**
     * @brief 获取接收超时时间
     * 
     * @return uint64_t 
     */
    uint64_t getRecvTimeout() const;

    /**
     * @brief 设置接收超时时间
     * 
     * @param timeout 
     */
    void setRecvTimeout(uint64_t timeout);
    
    /**
     * @brief 获取服务器名称
     * 
     * @return std::string 服务器名称
     */
    std::string getName() const;

    /**
     * @brief 设置服务器名称
     * 
     * @param name 
     */
    virtual void setName(const std::string& name);

    /**
     * @brief 返回服务器是否停止
     * 
     * @return true 服务器停止
     * @return false 服务器为停止
     */
    bool isStop() const;

protected:
    /**
     * @brief 开始接收来自特定套接字的连接
     * 
     * @param sock 指定接收连接的套接字
     */
    virtual void startAccept(Socket::ptr sock);

    /**
     * @brief 处理已接受的客户端连接
     * 
     * @param client 客户端的套接字
     */
    virtual void handleClient(Socket::ptr client);

    // 协程定时器
    co_timer m_timer;
private:
    // 监听套接字列表
    std::vector<Socket::ptr> m_listens;
    // 接收超时时间
    uint64_t m_recvTimeout;
    // 服务器名称
    std::string m_name;
    // 原子变量，标记服务器是否停止
    std::atomic_bool m_stop;
    // 协程通道，用于停止信号的传递
    co::co_chan<bool> m_stopCh;

};

}

#endif // RR_TCP_SERVER_H