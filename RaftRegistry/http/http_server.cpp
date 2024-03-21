//
// File created on: 2024/03/20
// Author: Zizhou

#include "RaftRegistry/http/http_server.h"
#include "RaftRegistry/http/parse.h"

namespace RR::http {
static auto Logger = GetLoggerInstance();

HttpServer::HttpServer(bool isKeepAlive = false) : m_isKeepAlive(isKeepAlive) {
    // 初始化 ServletDispatch，负责分发处理 HTTP 请求
    m_dispatch.reset(new ServletDispatch);
}

void HttpServer::handleClient(Socket::ptr client) {
    SPDLOG_LOGGER_DEBUG(Logger, "handleClient {}", client->toString());

    // 创建 HttpRequestParser 实例，用于解析 HTTP 请求
    HttpRequestParser::ptr parser(new HttpRequestParser);

    //创建 HttpSession实例，表示一次HTTP会话
    HttpSession::ptr session(new HttpSession(client));
    // 循环处理请求
    while (true) {
        // 接受请求
        HttpRequest::ptr request = session->recvRequest();
        // 如果请求为空，表示接受失败，关闭连接
        if (!request) {
            SPDLOG_LOGGER_DEBUG(Logger, "recvRequest failed, errno = {} errstr = {} client = {} keep-alive = {}", errno, strerror(errno), client->toString(), m_isKeepAlive);
            break;
        }

        // 创建 HttpResponse 实例，准备发送响应
        HttpResponse::ptr response(new HttpResponse(request->getVersion(), request->isClose() || !m_isKeepAlive));
        // 设置响应头部的server字段
        response->setHeader("Server", getName());
        
        // 调用 ServletDispatch 的 handle 方法，处理请求，并发送响应
        if (m_dispatch->handle(request, response, session) == 0) {
            session->sendResponse(response);
        }

        if (request->isClose() || !m_isKeepAlive) {
            break;
        }
    }
    session->close();
}

void HttpServer::setName(const std::string& name) {
    // 调用基类 TcpServer 的 setName 方法
    TcpServer::setName(name);
    // 设置默认的 NotFoundServlet，当找不到对应的处理器时使用
    m_dispatch->setDefault(std::make_shared<NotFoundServlet>(name));

}

}