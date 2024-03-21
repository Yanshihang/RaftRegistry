//
// File created on: 2024/03/20
// Author: Zizhou

#ifndef RR_HTTP_CONNECTION_H
#define RR_HTTP_CONNECTION_H

#include <memory>
#include <string>
#include <list>
#include <mutex>
#include "RaftRegistry/http/http.h"
#include "RaftRegistry/net/socket_stream.h"
#include "RaftRegistry/net/uri.h"

namespace RR::http {
// HttpResult 结构体：封装HTTP请求的结果
class HttpResult {
public:
    using ptr = std::shared_ptr<HttpResult>;

    // 枚举类型，定义可能的请求结果
    enum Result {
        OK = 0,
        INVALID_URL,
        INVALID_HOST,
        CREATE_SOCKET_ERROR,
        CONNECT_FAIL,
        SEND_CLOSE_BY_PEER,
        SEND_SOCKET_ERROR,
        TIMEOUT,
        POOL_INVALID_CONNECTION,
    };

    // 构造函数，初始化结果、响应和消息
    HttpResult(Result result, HttpResponse::ptr response, std::string msg) : result(result), response(response), msg(msg) {}

    // 将结果转换为字符串的方法
    std::string toString() const;

    // 请求结果
    Result result;
    // 响应对象智能指针
    HttpResponse::ptr response;
    // 结果消息
    std::string msg;
};

class HttpConnectionPool;

// 表示一个http连接
class HttpConnection : public SocketStream {
friend HttpConnectionPool;
public:
    using ptr = std::shared_ptr<HttpConnection>;

    // 构造函数，用于初始化一个Http连接
    HttpConnection(Socket::ptr socket, bool owner = true);
    ~HttpConnection();

    // 下面函数的返回值是HttpResult::ptr类型的智能指针
    // 这个类型中的数据成员有一个是HttpResponse::ptr类型的智能指针
    // 有这个数据成员，是因为连接过程有三次握手，发送请求之后必须得到响应，才算是连接成功。

    /**
     * @brief 执行HTTP GET请求
     * 
     * @param url 要发送GET请求的URL
     * @param timeoutMs 请求的超时时间（毫秒）。默认值为-1（无超时）
     * @param header 请求中包含的头部。默认为空映射
     * @param body 请求的主体。默认为空字符串
     * @return HttpResult::ptr HTTP请求的结果
     */
    static HttpResult::ptr DoGet(const std::string& url, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header={}, const std::string& body = "");

    /**
     * @brief 执行HTTP GET请求
     * 
     * @param uri 要发送GET请求的URI
     * @param timeoutMs 请求的超时时间（毫秒）。默认值为-1（无超时）
     * @param header 请求中包含的头部。默认为空映射
     * @param body 请求的主体。默认为空字符串
     * @return HttpResult::ptr HTTP请求的结果
     */
    static HttpResult::ptr DoGet(Uri::ptr uri, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header={}, const std::string& body = "");

    /**
     * @brief 执行HTTP POST请求
     * 
     * @param url 要发送POST请求的URL
     * @param timeoutMs 请求的超时时间（毫秒）。默认值为-1（无超时）
     * @param header 请求中包含的头部。默认为空映射
     * @param body 请求的主体。默认为空字符串
     * @return HttpResult::ptr HTTP请求的结果
     */
    static HttpResult::ptr DoPost(const std::string& url, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header={}, const std::string& body = "");

    /**
     * @brief 执行HTTP POST请求
     * 
     * @param uri 要发送POST请求的URI
     * @param timeoutMs 请求的超时时间（毫秒）。默认值为-1（无超时）
     * @param header 请求中包含的头部。默认为空映射
     * @param body 请求的主体。默认为空字符串
     * @return HttpResult::ptr HTTP请求的结果
     */
    static HttpResult::ptr DoPost(Uri::ptr uri, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header={}, const std::string& body = "");

    /**
     * @brief 执行指定方法的HTTP请求
     * 
     * @param method 请求的HTTP方法
     * @param url 要发送请求的URL
     * @param timeoutMs 请求的超时时间（毫秒）。默认值为-1（无超时）
     * @param header 请求中包含的头部。默认为空映射
     * @param body 请求的主体。默认为空字符串
     * @return HttpResult::ptr HTTP请求的结果
     */
    static HttpResult::ptr DoRequest(HttpMethod method, const std::string& url, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header={}, const std::string& body = "");
    
    /**
     * @brief 执行指定方法的HTTP请求
     * 
     * @param method 请求的HTTP方法
     * @param uri 要发送请求的URI
     * @param timeoutMs 请求的超时时间（毫秒）。默认值为-1（无超时）
     * @param header 请求中包含的头部。默认为空映射
     * @param body 请求的主体。默认为空字符串
     * @return HttpResult::ptr HTTP请求的结果
     */
    static HttpResult::ptr DoRequest(HttpMethod method, Uri::ptr uri, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header={}, const std::string& body = "");
    
    /**
     * @brief 使用指定的HttpRequest对象和URI执行HTTP请求
     * 
     * @param request 用于请求的HttpRequest对象
     * @param uri 要发送请求的URI
     * @param timeoutMs 请求的超时时间（毫秒）。默认值为-1（无超时）
     * @return HttpResult::ptr HTTP请求的结果
     */
    static HttpResult::ptr DoRequest(HttpRequest::ptr request, Uri::ptr uri, uint64_t timeoutMs = -1);
    
    // 接收http响应
    HttpResponse::ptr recvResponse();

    // 发送http请求
    ssize_t sendRequest(HttpRequest::ptr req);
private:
    uint64_t m_createTime = 0; // 连接创建时间
    uint64_t m_request = 0; // 请求次数

};

// 管理HTTP连接的池
class HttpConnectionPool {
public:
    using ptr = std::shared_ptr<HttpConnectionPool>;
    using MutexType = std::mutex;

    // 构造函数，初始化连接池
    HttpConnectionPool(const std::string& host, const std::string& vhost, uint32_t port, bool isHttps, uint32_t maxSize, uint32_t maxAliveTime, uint32_t maxRequest);

    // 创建连接池实例
    static HttpConnectionPool::ptr Create( const std::string& uri, const std::string& vhost, uint32_t maxSize, uint32_t maxAliveTime, uint32_t maxRequest);

    // 通过连接池执行一个HTTP GET请求
    HttpResult::ptr doGet(const std::string& url, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header = {}, const std::string& body = "");
    HttpResult::ptr doGet(Uri::ptr uri, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header = {}, const std::string& body = "");
    HttpResult::ptr doPost(const std::string& url, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header = {}, const std::string& body = "");
    HttpResult::ptr doPost(Uri::ptr uri, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header = {}, const std::string& body = "");
    HttpResult::ptr doRequest(HttpMethod method, const std::string& url, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header = {}, const std::string& body = "");
    HttpResult::ptr doRequest(HttpMethod method, Uri::ptr uri, uint64_t timeoutMs = -1, const std::map<std::string, std::string>& header = {}, const std::string& body = "");
    HttpResult::ptr doRequest(HttpRequest::ptr request, uint64_t timeoutMs = -1);

    // 从连接池中获取一个可用的HttpConnection对象
    HttpConnection::ptr getConnection();
private:
    // 释放或回收HttpConnection对象到连接池
    static void ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool);

    // 主机名
    std::string m_host;
    // 虚拟主机名
    std::string m_vhost;
    // 端口号
    uint32_t m_port;
    // 连接池最大大小
    uint32_t m_maxSize;
    // 连接最大存活时间
    uint32_t m_maxAliveTime;
    // 每个连接的最大请求次数
    uint32_t m_maxRequest;
    // 是否为HTTPS连接
    bool m_isHttps;
    // 互斥锁
    MutexType m_mutex;
    // 连接列表
    std::list<HttpConnection*> m_conns;
    // 总连接数
    std::atomic<uint32_t> m_total{0};
};

}


#endif // RR_HTTP_CONNECTION_H