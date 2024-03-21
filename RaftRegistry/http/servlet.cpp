//
// File created on: 2024/03/21
// Author: Zizhou

#include "RaftRegistry/http/servlet.h"
#include <fnmatch.h> // 用于匹配通配符路径

namespace RR::http {
int32_t FunctionServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
    // 直接调用成员变量m_cb指向的回调函数处理HTTP请求
    return m_cb(request, response, session);
}

NotFoundServlet::NotFoundServlet(std::string name) : Servlet("NotFoundServlet"), m_name(name) {
    // 初始化Servlet名称，并设置404页面的HTML内容
    // 设置404页面内容
    m_content = "<html>"
                "<head><title>404 Not Found</title></head>"
                "<body>"
                "<center><h1>404 Not Found</h1></center>"
                "<hr>"
                "<center>" + m_name + "</center>"
                "</body>"
                "</html>";
}

int32_t NotFoundServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
    // 设置响应状态为404，并设置响应体为404页面内容
    response->setStatus(HttpStatus::NOT_FOUND);
    response->setContentType("text/html");
    response->setHeader("Server", "RaftRegistry");
    response->setBody(m_content); // 设置响应体
}

// 初始化Servlet名称，并设置默认的NotFoundServlet
ServletDispatch::ServletDispatch() : Servlet("ServletDispatch") {
    m_default.reset(new NotFoundServlet("RaftRegistry/1.0.0")); // 设置默认的404页面
}

int32_t ServletDispatch::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
    // 根据请求URI获取匹配的Servlet，并调用其handle函数处理请求
    auto servlet = getMatchedServlet(request->getPath()); // 根据请求URI获取匹配的Servlet
    if (servlet) {
        servlet->handle(request, response, session); // 调用匹配的Servlet处理请求
    }
    return 0; // 表示成功
}
void ServletDispatch::addServlet(const std::string& uri, Servlet::ptr servlet) {
    std::unique_lock<co_wmutex> lock(m_mutex.Writer());
    m_datas[uri] = servlet;
}
void ServletDispatch::addServlet(const std::string& uri, FunctionServlet::callback cb) {
    std::unique_lock<co_wmutex> lock(m_mutex.Writer());
    m_datas[uri] = std::make_shared<FunctionServlet>(cb);
}
void ServletDispatch::addGlobalServlet(const std::string& uri, Servlet::ptr servlet) {
    std::unique_lock<co_wmutex> lock(m_mutex.Writer());
    m_globs[uri] = servlet;
}
void ServletDispatch::addGlobalServlet(const std::string& uri, FunctionServlet::callback cb) {
    std::unique_lock<co_wmutex> lock(m_mutex.Writer());
    m_globs[uri] = std::make_shared<FunctionServlet>(cb);
}
void ServletDispatch::delServlet(const std::string& uri) {
    std::unique_lock<co_wmutex> lock(m_mutex.Writer());
    m_datas.erase(uri);
}
void ServletDispatch::delGlobalServlet(const std::string& uri) {
    std::unique_lock<co_wmutex> lock(m_mutex.Writer());
    m_globs.erase(uri);
}

Servlet::ptr ServletDispatch::getServlet(const std::string& uri) {
    std::unique_lock<co_rmutex> lock(m_mutex.Reader());
    auto iter = m_datas.find(uri);
    if (iter == m_datas.end()) {
        return nullptr;
    }
    return iter->second;
}

Servlet::ptr ServletDispatch::getGlobalServlet(const std::string& uri) {
    std::unique_lock<co_rmutex> lockf64(m_mutex.Reader());
    auto iter = m_globs.find(uri);
    if (iter == m_globs.end()) {
        return nullptr;
    }
    return iter->second;
}

Servlet::ptr ServletDispatch::getMatchedServlet(const std::string& uri) {
    std::unique_lock<co_rmutex> lock(m_mutex.Reader());
    // 首先在精确匹配的Servlet映射中查找URI对应的Servlet
    auto dataIt = m_datas.find(uri);
    if (dataIt!= m_datas.end()) {
        return dataIt->second;
    }

    // 如果没有找到精确匹配的Servlet，则在通配符Servlet映射中查找
    for (auto it : m_globs) {
        if (!fnmatch(it.first.c_str(), uri.c_str(), 0)) {
            return it.second;
        }
    }
    return m_default; // 返回默认的404页面
}

} // namespace RR::http