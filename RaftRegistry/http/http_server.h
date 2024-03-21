//
// File created on: 2024/03/20
// Author: Zizhou

#ifndef RR_HTTP_SERVER_H
#define RR_HTTP_SERVER_H

#include "RaftRegistry/net/tcp_server.h"
#include "RaftRegistry/http/servlet.h"

namespace RR::http {

class HttpServer : public TcpServer {
public:
    // 构造函数，接收一个表示是否保持连接的布尔值
    HttpServer(bool isKeepAlive = false);

    // 获取 ServletDispatch 对象的指针，用于处理具体的 HTTP 请求
    ServletDispatch::ptr getDispatch() const { return m_dispatch; }

    // 设置 ServletDispatch 对象，允许自定义请求处理逻辑
    ServletDispatch::ptr setDispatch(ServletDispatch::ptr dispatch) { m_dispatch = dispatch;}

    // 设置服务器名称
    void setName(const std::string& name) override;

protected:
    // 重写处理客户端连接的方法
    void handleClient(Socket::ptr client) override;
private:
    // 成员变量，标识是否保持连接
    bool m_isKeepAlive;
    // ServletDispatch 对象，用于分发处理 HTTP 请求
    ServletDispatch::ptr m_dispatch;
};

}

#endif // RR_HTTP_SERVER_H