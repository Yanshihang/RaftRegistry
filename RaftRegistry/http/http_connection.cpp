//
// File created on: 2024/03/20
// Author: Zizhou

#include "RaftRegistry/http/http_connection.h"
#include "RaftRegistry/common/util.h"

namespace RR::http {
static auto Logger = GetLoggerInstance();

std::string HttpResult::toString() const {
    std::string str = "[HttpResult result = " + std::to_string(static_cast<int>(result)) + " msg = " + msg
                        + " response = " + (response ? response->toString():"nullptr") + "]";
    return str;
}

HttpConnection::HttpConnection(Socket::ptr socket, bool owner = true) : SocketStream(socket, owner) {}

HttpConnection::~HttpConnection() {
    SPDLOG_LOGGER_DEBUG(Logger, "HttpConnection::~HttpConnection");
}

HttpResult::ptr HttpConnection::DoGet(const std::string& url, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header={}, const std::string& body = "") {
    Uri::ptr uri = Uri::Create(url);
    if (!uri) {
        return std::make_shared<HttpResult>(HttpResult::INVALID_URL, nullptr, "invalid url:" + url);
    }
    return DoGet(uri, timeoutMs, header, body);
}

HttpResult::ptr HttpConnection::DoGet(Uri::ptr uri, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header={}, const std::string& body = "") {
    return DoRequest(HttpMethod::GET, uri, timeoutMs, header, body);
}

HttpResult::ptr HttpConnection::DoPost(const std::string& url, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header={}, const std::string& body = "") {
    Uri::ptr uri = Uri::Create(url);
    if (!uri) {
        return std::make_shared<HttpResult>(HttpResult::INVALID_URL, nullptr, "invalid url:" + url);
    }
    return DoPost(uri, timeoutMs, header, body);
}

HttpResult::ptr HttpConnection::DoPost(Uri::ptr uri, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header={}, const std::string& body = "") {
    return DoRequest(HttpMethod::POST, uri, timeoutMs, header, body);
}

HttpResult::ptr HttpConnection::DoRequest(HttpMethod method, const std::string& url, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header={}, const std::string& body = "") {
    Uri::ptr uri = Uri::Create(url);
    if (!uri) {
        return std::make_shared<HttpResult>(HttpResult::INVALID_URL, nullptr, "invalid url:" + url);
    }

    return DoRequest(method, uri, timeoutMs, header, body);
}

HttpResult::ptr HttpConnection::DoRequest(HttpMethod method, Uri::ptr uri, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header, const std::string& body) {
    // 创建HttpRequest对象并设置属性
    HttpRequest::ptr request = std::make_shared<HttpRequest>();
    request->setMethod(method);
    request->setPath(uri->getPath());
    request->setQuery(uri->getQuery());
    request->setFragment(uri->getFragment());

    // 检查和设置请求头部
    bool hasHost = false;
    for (auto& i : header) {
        // 如果有connection字段，判断是否要设为长连接
        if (strcasecmp(i.first.c_str(), "connection") == 0) {
            if (strcasecmp(i.second.c_str(), "keep-alive") == 0) {
                request->setClose(false); // 设置为长连接(不主动关闭)
            }
            continue;
        }

        // 如果还没设置host，且存在字段名host，则根据host键对应的值是否为空设置hasHost
        if (!hasHost && strcasecmp(i.first.c_str(), "host") == 0) {
            hasHost = !i.second.empty();
        }

        // 原项目中没有这行代码，也就是没有把参数header设置到请求头中
        // 但是应该是设置请求头部的
        request->setHeader(i.first, i.second);
    }


    // 如果传入的header中没有host字段，则使用uri的host数据成员
    if (!hasHost) {
        request->setHeader("Host", uri->getHost());
    }
    request->setBody(body);

    return DoRequest(request, uri, timeoutMs);
}

HttpResult::ptr HttpConnection::DoRequest(HttpRequest::ptr request, Uri::ptr uri, uint64_t timeoutMs = -1) {

}

HttpResponse::ptr HttpConnection::recvResponse() {

}

ssize_t HttpConnection::sendRequest(HttpRequest::ptr req) {

}

} // namespace RR::http