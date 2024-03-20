//
// File created on: 2024/03/20
// Author: Zizhou

#include "RaftRegistry/http/http_session.h"
#include "RaftRegistry/common/util.h"
#include "RaftRegistry/http/parse.h"

namespace RR::http {
static auto Logger = GetLoggerInstance();

HttpSession::HttpSession(Socket::ptr socket, bool owner) : SocketStream(socket, owner) {}

HttpRequest::ptr HttpSession::recvRequest() {
    // 创建HTTP请求解析器
    HttpRequestParser::ptr parser(new HttpRequestParser);

    // 获取解析所需的缓冲区大小
    uint64_t buffer_size = HttpRequestParser::GetHttpRequestBufferSize();

    // 创建缓冲区
    std::string buffer;
    buffer.resize(buffer_size);

    // 获取缓冲区的指针
    char* data = &buffer[0];
    // 设置偏移量
    size_t offset = 0;

    // 循环读取数据并解析，知道解析完成（这里只解析请求行和请求头）
    while(!parser->isFinished()) {
        // 本次从Socket中读取数据的数据长度
        ssize_t len = read(data+offset, buffer_size-offset);
        // 如果读取失败，关闭连接并返回空指针
        if (len <= 0) {
            close(); // 关闭套接字
            return nullptr;
        }

        // 如果读取成功，则更新数据长度
        len += offset;
        // 解析数据
        size_t nparse = parser->execute(data, len);

        if (parser->hasError() || nparse == 0) {
            SPDLOG_LOGGER_DEBUG(Logger, "parser error code = {}", parser.hasError());
            close();
            return nullptr;
        }
        // 这里这么算，是因为execute会将为解析的数据移到最前面
        offset = len - nparse;
    }
    
    // 消息体长度（正文长度）
    uint64_t length = parser->getContentLength();
    // 处理HTTP请求正文
    if (length >= 0) {
        std::string body;
        body.resize(length);
        size_t len = 0;

        // 如果正文长度大于已读取的数据（读取的长度大于解析的长度），进行处理
        if (length > offset) {
            memcpy(&body[0], data, offset);
            len = offset;
        } else {
            memcpy(&body[0], data, length);
            len = length;
        }
        length -= len;

        // 如果每处理完，继续处理
        if (length > 0) {
            if (readFixSize(&body[len], length) <= 0) {
                close();
                return nullptr;
            }
        }
        parser->getData()->setBody(body);
    }

    return parser->getData();
}

ssize_t HttpSession::sendResponse(HttpResponse::ptr response) {
    std::string str = response->toString();
    return writeFixSize(str.c_str(), str.size());
}

}