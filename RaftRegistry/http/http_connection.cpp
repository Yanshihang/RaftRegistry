//
// File created on: 2024/03/20
// Author: Zizhou

#include "RaftRegistry/http/http_connection.h"
#include "RaftRegistry/common/util.h"
#include "RaftRegistry/http/parse.h"

namespace RR::http {
static auto Logger = GetLoggerInstance();

// HttpResult

std::string HttpResult::toString() const {
    std::string str = "[HttpResult result = " + std::to_string(static_cast<int>(result)) + " msg = " + msg
                        + " response = " + (response ? response->toString():"nullptr") + "]";
    return str;
}

// HttpConnection

HttpConnection::HttpConnection(Socket::ptr socket, bool owner) : SocketStream(socket, owner) {}

HttpConnection::~HttpConnection() {
    SPDLOG_LOGGER_DEBUG(Logger, "HttpConnection::~HttpConnection");
}

HttpResult::ptr HttpConnection::DoGet(const std::string& url, uint64_t timeoutMs, const std::map<std::string, std::string>& header, const std::string& body) {
    Uri::ptr uri = Uri::Create(url);
    if (!uri) {
        return std::make_shared<HttpResult>(HttpResult::INVALID_URL, nullptr, "invalid url:" + url);
    }
    return DoGet(uri, timeoutMs, header, body);
}

HttpResult::ptr HttpConnection::DoGet(Uri::ptr uri, uint64_t timeoutMs, const std::map<std::string, std::string>& header, const std::string& body) {
    return DoRequest(HttpMethod::GET, uri, timeoutMs, header, body);
}

HttpResult::ptr HttpConnection::DoPost(const std::string& url, uint64_t timeoutMs, const std::map<std::string, std::string>& header, const std::string& body) {
    Uri::ptr uri = Uri::Create(url);
    if (!uri) {
        return std::make_shared<HttpResult>(HttpResult::INVALID_URL, nullptr, "invalid url:" + url);
    }
    return DoPost(uri, timeoutMs, header, body);
}

HttpResult::ptr HttpConnection::DoPost(Uri::ptr uri, uint64_t timeoutMs, const std::map<std::string, std::string>& header, const std::string& body) {
    return DoRequest(HttpMethod::POST, uri, timeoutMs, header, body);
}

HttpResult::ptr HttpConnection::DoRequest(HttpMethod method, const std::string& url, uint64_t timeoutMs, const std::map<std::string, std::string>& header, const std::string& body) {
    Uri::ptr uri = Uri::Create(url);
    if (!uri) {
        return std::make_shared<HttpResult>(HttpResult::INVALID_URL, nullptr, "invalid url:" + url);
    }

    return DoRequest(method, uri, timeoutMs, header, body);
}

HttpResult::ptr HttpConnection::DoRequest(HttpMethod method, Uri::ptr uri, uint64_t timeoutMs, const std::map<std::string, std::string>& header, const std::string& body) {
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

HttpResult::ptr HttpConnection::DoRequest(HttpRequest::ptr request, Uri::ptr uri, uint64_t timeoutMs) {
    if (uri->getScheme().empty()) {
        uri->setScheme("http");
    }

    // 根据ur创建地址对象
    Address::ptr address = uri->createAddress();
    if (!address) {
        return std::make_shared<HttpResult>(HttpResult::INVALID_HOST, nullptr, "invalid host:" + uri->getHost());
    }

    // 创建套接字
    Socket::ptr sock = Socket::CreateTCP(address);
    if (!sock) {
        return std::make_shared<HttpResult>(HttpResult::CREATE_SOCKET_ERROR, nullptr, "create socket fail:" + address->toString() + "errno=" + std::to_string(errno) + " errstr=" + std::string(strerror(errno)));
    }

    // 连接到服务器
    if (!sock->connect(address)) {
        return std::make_shared<HttpResult>(HttpResult::CONNECT_FAIL, nullptr, "connect fail:" + address->toString());
    }

    // 创建HttpConnection对象
    HttpConnection::ptr connection = std::make_shared<HttpConnection>(sock);

    // 设置接收超时时间
    sock->setRecvTimeout(timeoutMs);

    // 发送请求
    // 这是目的，因为函数名为DoRequest
    int result = connection->sendRequest(request);

    if (result== 0) { // 如果连接被对方关闭， 则返回错误结果
        return std::make_shared<HttpResult>(HttpResult::SEND_CLOSE_BY_PEER, nullptr, "send request closed by peer:" + address->toString());
    }

    if (result < 0) { // 发送请求出错，返回错误结果
        return std::make_shared<HttpResult>(HttpResult::SEND_SOCKET_ERROR, nullptr, "send request socket error errno=" + std::to_string(errno) + "errstr=" + std::string(strerror(errno)));
    }

    // 如果result > 0，说明发送请求成功，接收响应
    HttpResponse::ptr response = connection->recvResponse();
    if (!response) { // 接收响应超时，返回错误结果
        return std::make_shared<HttpResult>(HttpResult::TIMEOUT, nullptr, "recv response timeout:" + address->toString() + " timeoutMs:" + std::to_string(timeoutMs));
    }

    // 请求成功，返回响应结果
    return std::make_shared<HttpResult>(HttpResult::OK, response, "ok");
}

HttpResponse::ptr HttpConnection::recvResponse() {
    // 创建HttpResponseParser对象
    HttpResponseParser::ptr parser(new HttpResponseParser);
    // 获取HttpResponseParser推荐的缓冲区大小
    uint64_t bufferSize = HttpResponseParser::GetHttpResponseBufferSize();
    // 分配缓冲区
    std::string buffer;
    buffer.resize(bufferSize);
    char* data = &buffer[0];
    size_t offset = 0;

    // 循环读取数据直到解析完成
    // 解析响应头
    while(!parser->isFinished()) {
        // 实际读取的字节数
        ssize_t len = read(data+offset, bufferSize-offset);
        if (len <=0) {
            close(); // 关闭连接
            return nullptr;
        }

        // 计算这次读取的数据长度和之前未解析的数据长度之后的总长度，用于此次解析
        len+=offset;

        // 执行解析，nparse是此次实际解析的数据长度
        size_t nparse = parser->execute(data, len);
        if (parser->hasError() || nparse == 0) {
            close();  // 关闭连接
            return nullptr;  // 返回空指针
        }

        // 计算剩余未解析数据长度，下次读取数据从未解析后开始
        // 最后一次解析后offset指向的位置是未解析数据的结束位置
        offset = len - nparse;
    }

    // 解析响应体
    // 处理chunked传输编码
    if (parser->isChunked()) {
        do {
            ssize_t len = read(data+offset, bufferSize-offset);
            if (len <=0) {
                close(); // 关闭连接
                return nullptr;
            }
            len+=offset;
            size_t nparse = parser->execute(data, len,true);
            if (parser->hasError() || nparse == 0) {
                close();  // 关闭连接
                return nullptr;  // 返回空指针
            }
            offset = len - nparse;
        } while(!parser->isFinished());
    } else {
        // 处理非chunked编码
        uint64_t length = parser->getContentLength();
        // 读取消息体
        if (length >0) {
            // 存储消息体
            std::string body;
            body.resize(length);
            // 已读取到body中的长度
            size_t len = 0;
            if (length >= offset) {
                // 先将data中（或buffer中）未解析的数据拷贝到body中，offset是未解析数据的长度
                memcpy(&body[0], data, offset);
                len = offset;
            } else {
                memcpy(&body[0], data, length);
                len = length;
            }
            length -= len;

            if (length>0) {
                // 如果消息体长度大于offset，说明还有未读取的数据
                // 然后调用readFixSize从系统缓冲区中读取剩余数据
                if (readFixSize(&body[len], length) <= 0) {
                    close();
                    return nullptr;
                }
            }
            // 将读取的正文存储到响应对象中
            parser->getData()->setBody(body);
        }
    }

    // 返回解析好的响应对象
    return parser->getData();
}

ssize_t HttpConnection::sendRequest(HttpRequest::ptr req) {
    // 将请求转换为字符串
    std::string str = req->toString();
    return writeFixSize(str.c_str(), str.size());
}

// HttpConnectionPool

HttpConnectionPool::HttpConnectionPool(const std::string& host, const std::string& vhost, uint32_t port, bool isHttps, uint32_t maxSize, uint32_t maxAliveTime, uint32_t maxRequest) : m_host(host), m_vhost(vhost), m_port(port), m_isHttps(isHttps), m_maxSize(maxSize), m_maxAliveTime(maxAliveTime), m_maxRequest(maxRequest) {}

// 创建连接池实例
HttpConnectionPool::ptr HttpConnectionPool::Create(const std::string& uri, const std::string& vhost, uint32_t maxSize, uint32_t maxAliveTime, uint32_t maxRequest) {
    // 通过URI创建Uri对象
    Uri::ptr uriPtr = Uri::Create(uri);
    if (!uriPtr) {
        SPDLOG_LOGGER_ERROR(Logger, "invalid uri = {}", uri);
    }

    // 创建并返回HttpConnectionPool对象
    return std::make_shared<HttpConnectionPool>(uriPtr->getHost(), vhost, uriPtr->getPort(), uriPtr->getScheme() == "https", maxSize, maxAliveTime, maxRequest);
}

HttpResult::ptr HttpConnectionPool::doGet(const std::string& url, uint64_t timeoutMs, const std::map<std::string, std::string>& header, const std::string& body) {
    return doRequest(HttpMethod::GET, url, timeoutMs, header, body);
}

HttpResult::ptr HttpConnectionPool::doGet(Uri::ptr uri, uint64_t timeoutMs, const std::map<std::string, std::string>& header, const std::string& body) {
    std::stringstream ss;
    ss << uri->getPath() << (uri->getQuery().empty()? "" : "?") << uri->getQuery() << (uri->getFragment().empty()? "" : "#") << uri->getFragment();
    return doGet(ss.str(), timeoutMs, header, body);
}

HttpResult::ptr HttpConnectionPool::doPost(const std::string& url, uint64_t timeoutMs, const std::map<std::string, std::string>& header, const std::string& body) {
    return doRequest(HttpMethod::POST, url, timeoutMs, header, body);
}

HttpResult::ptr HttpConnectionPool::doPost(Uri::ptr uri, uint64_t timeoutMs, const std::map<std::string, std::string>& header, const std::string& body) {
    std::stringstream ss;
    ss << uri->getPath() << (uri->getQuery().empty()? "" : "?") << uri->getQuery() << (uri->getFragment().empty()? "" : "#") << uri->getFragment();
    return doPost(ss.str(), timeoutMs, header, body);
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method, const std::string& url, uint64_t timeoutMs, const std::map<std::string, std::string>& header, const std::string& body) {
    // 创建HttpRequest对象，并设置请求的方法、路径和头部信息
    HttpRequest::ptr request = std::make_shared<HttpRequest>();
    request->setPath(url);
    request->setMethod(method);
    
    // 表示是否包含host头部，这是http请求中必须有的部分
    bool hasHost = false;
    // 遍历提供的头部信息，设置到HttpRequest对象中
    for (auto& i : header) {
        // 判断是否需要设置为自动关闭
        if (strcasecmp(i.first.c_str(), "connection") == 0) {
            if (strcasecmp(i.second.c_str(), "keep-alive") == 0) {
                request->setClose(false);
            }
            continue;
        }
        // 检查是否已经设置了Host头
        if (!hasHost && strcasecmp(i.first.c_str(), "host") == 0) {
            hasHost = !i.second.empty();
        }
        // 设置头部
        request->setHeader(i.first, i.second);
    }

    // 如果遍历完header后，还没有设置host头部，则使用连接池的主机名或虚拟主机名作为Host头部
    if (!hasHost) {
        // 如果有虚拟主机名则使用虚拟主机名，否则使用主机名
        if (m_vhost.empty()) {
            request->setHeader("Host", m_host);
        } else {
            request->setHeader("Host", m_vhost);
        }
    }

    // 设置请求体
    request->setBody(body);
    return doRequest(request, timeoutMs);

}

HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method, Uri::ptr uri, uint64_t timeoutMs, const std::map<std::string, std::string>& header, const std::string& body) {
    // 将uri转换为字符串url，然后调用相应的重载函数就可以

    std::stringstream ss;
    ss << uri->getPath() << (uri->getQuery().empty()? "" : "?") << uri->getQuery() << (uri->getFragment().empty()? "" : "#") << uri->getFragment();

    return doRequest(method, ss.str(), timeoutMs, header, body);
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpRequest::ptr request, uint64_t timeoutMs) {
    // 从连接池中获取一个可用的HttpConnection对象
    HttpConnection::ptr conn = getConnection();
    if (!conn) {
        return std::make_shared<HttpResult>(HttpResult::POOL_INVALID_CONNECTION, nullptr, "pool host:" + m_host + " port:" + std::to_string(m_port));
    }

    // 获取与HttpConnection对象关联的Socket对象
    Socket::ptr sock = conn->getSocket();
    if (!sock) {
        return std::make_shared<HttpResult>(HttpResult::POOL_INVALID_CONNECTION, nullptr, "pool host:" + m_host + " port:" + std::to_string(m_port));
    }

    // 设置接收超时时间
    sock->setRecvTimeout(timeoutMs);

    ssize_t result = conn->sendRequest(request);
    if (result == 0) {
        return std::make_shared<HttpResult>(HttpResult::SEND_SOCKET_ERROR, nullptr, "send request socket error errno=" + std::to_string(errno) + " errstr=" + std::string(strerror(errno)));
    }

    // 接收响应
    HttpResponse::ptr response = conn->recvResponse();
    if (!response) {
        return std::make_shared<HttpResult>(HttpResult::TIMEOUT, nullptr, "recv response timeout: "+ sock->getRemoteAddress()->toString() + " timeoutMs: " + std::to_string(timeoutMs));
    }
    return std::make_shared<HttpResult>(HttpResult::OK, response, "ok");
}

HttpConnection::ptr HttpConnectionPool::getConnection() {
    uint64_t nowMs = RR::GetCuurentTimeMs(); // 获取当前时间戳
    std::vector<HttpConnection*> invalidConns; // 无效连接列表

    HttpConnection* ptr = nullptr;
    {
        // 锁定互斥锁，保证线程安全
        std::unique_lock<std::mutex> lock(m_mutex);
        // 遍历连接池，寻找可用连接
        while(!m_conns.empty()) {
            auto conn = *m_conns.begin();
            m_conns.pop_front();
            // 检查连接是否有效
            if (!conn->isConnected()) {
                invalidConns.push_back(conn); // 如果连接无效，将其添加到无效连接列表
                continue;
            }
            // 检查连接是否已经超过最大存活时间
            if ((conn->m_createTime+m_maxAliveTime) > nowMs) {
                invalidConns.push_back(conn); // 如果连接超过最大存活时间，将其添加到无效连接列表
                continue;
            }
            ptr = conn; // 找到有效连接
            break;
        }
    } // 互斥锁在这里自动解锁

    for (auto& i : invalidConns) {
        delete i; // 删除无效连接
    }

    // 更新总连接数量
    m_total -= invalidConns.size();

    if (!ptr) {
        IpAddress::ptr addr = Address::LookUpAnyIpAddress(m_host);
        if (!addr) {
            SPDLOG_LOGGER_ERROR(Logger, "get address fail: {}", m_host);
            return nullptr;
        }

        addr->setPort(m_port);
        // 创建一个新的socket对象
        Socket::ptr sock = Socket::CreateTCP(addr);
        if (!sock) {
            SPDLOG_LOGGER_ERROR(Logger, "create socket fail: {}", addr->toString());
            return nullptr;
        }

        ptr = new HttpConnection(sock); // 使用新创建的socket对象创建一个httpconnection对象
        ++m_total;
    }
    // 返回HttpConnection对象的智能指针，当智能指针被销毁时，会调用RelesePtr函数处理HttpConnection对象
    return HttpConnection::ptr(ptr, std::bind(&HttpConnectionPool::ReleasePtr, this, std::placeholders::_1));
}

void HttpConnectionPool::ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool) {
    // 增加连接的请求计数
    ++ptr->m_request;
    // 检查连接是否已断开、是否超过最大存活时间或是否超过最大请求次数
    if (ptr->isConnected() || (ptr->m_createTime+pool->m_maxAliveTime) < RR::GetCuurentTimeMs() || (ptr->m_request >= pool->m_maxRequest)) {
        delete ptr;
        --pool->m_total;
        return;
    }

    // 如果连接仍然有效，将其回收到连接池中
    std::unique_lock<std::mutex> lock(pool->m_mutex);
    // 将HttpConnection对象放回连接池
    pool->m_conns.push_back(ptr);
}

} // namespace RR::http