//
// File created on: 2024/03/20
// Author: Zizhou

#ifndef RR_HTTP_SESSION_H
#define RR_HTTP_SESSION_H

#include <memory>
#include "RaftRegistry/net/socket_stream.h"
#include "RaftRegistry/http/http.h"

namespace RR::http {
// 用于管理HTTP会话
class HttpSession : public SocketStream {
public:
    using ptr = std::shared_ptr<HttpSession>;

    /**
     * @brief 初始化一个http会话
     * 
     * @param 指向socket对象的智能指针，用于网络通信
     * @param owner 表示HttpSession是否拥有socket的所有权
     */
    HttpSession(Socket::ptr socket, bool owner = true);

    /**
     * @brief 接收HTTP请求的函数
     * 
     * @return 返回指向HttpRequest对象的智能指针
     */
    HttpRequest::ptr recvRequest();

    /**
     * @brief 发送HTTP响应的函数
     * 
     * @param response 指向HttpResponse对象的智能指针
     * @return ssize_t 发送的字节数
     */
    ssize_t sendResponse(HttpResponse::ptr response);
};


}

#endif // RR_HTTP_SESSION_H