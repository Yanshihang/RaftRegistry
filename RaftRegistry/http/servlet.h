//
// File created on: 2024/03/21
// Author: Zizhou


#ifndef RR_SERVLET_H
#define RR_SERVLET_H

#include <memory>
#include <string>
#include <functional>
#include "RaftRegistry/http/http.h"
#include "RaftRegistry/http/http_session.h"

#include <libgo/libgo.h>

namespace RR::http {
// 定义了处理HTTP请求的接口
class Servlet {
public:
    using ptr = std::shared_ptr<Servlet>;
    Servlet(const std::string& name) : m_name(name) {}
    virtual ~Servlet() {};

    // 处理HTTP请求，由派生类实现具体逻辑
    virtual int32_t handle(HttpRequest::ptr reuqest, HttpResponse::ptr response, HttpSession::ptr session) = 0;

    // 获取Servlet的名称
    const std::string& getName() const { return m_name;}

private:
    //  Servlet的名称
    std::string m_name;
};

// 使用函数作为请求处理器的Servlet
class FunctionServlet : public Servlet {
public:
    using ptr = std::shared_ptr<FunctionServlet>;
    // 定义回调函数类型
    using callback = std::function<int32_t(HttpRequest::ptr, HttpResponse::ptr, HttpSession::ptr)>;

    // 构造函数，接收一个回调函数作为请求处理器
    FunctionServlet(callback cb) : Servlet("FunctionServlet"), m_cb(cb) {}

    // 实现基类的handle函数
    // 直接调用回调函数处理请求
    int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override ;
private:
    // 回调函数
    callback m_cb;
};

// 负责管理和分发Servlet
class ServletDispatch : public Servlet {
public:
    using ptr = std::shared_ptr<ServletDispatch>;
    using RWMutexType = co::co_rwmutex; // 使用协程兼容的读写锁

    ServletDispatch();

    // 实现handle函数，根据请求的URI分发给对应的Servlet处理
    int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;
    // 添加uri和servlet的映射
    void addServlet(const std::string& uri, Servlet::ptr servlet);
    // 添加uri和回调函数的映射
    void addServlet(const std::string& uri, FunctionServlet::callback cb);
    
    // 添加通配符uri和servlet的映射
    void addGlobalServlet(const std::string& uri, Servlet::ptr servlet);
    // 添加通配符uri和回调函数的映射
    void addGlobalServlet(const std::string& uri, FunctionServlet::callback cb);
    
    // 删除uri对应的映射
    void delServlet(const std::string& uri);
    void delGlobalServlet(const std::string& uri);
    
    // 根据uri获取精确的Servlet
    Servlet::ptr getServlet(const std::string& uri);
    // 根据uri获取通配符Servlet
    Servlet::ptr getGlobalServlet(const std::string& uri);
    // 先获取精确的Servlet，如果没有再获取通配符Servlet
    Servlet::ptr getMatchedServlet(const std::string& uri);
    
    Servlet::ptr getDefault() const { return m_default;}
    void setDefault(Servlet::ptr def) {m_default = def;}
private:
    RWMutexType m_mutex;
    //  精确URI和Servlet的映射
    std::map<std::string, Servlet::ptr> m_datas;
    std::map<std::string, Servlet::ptr> m_globs;
    // 默认的Servlet，处理404
    Servlet::ptr m_default;
};

class NotFoundServlet : public Servlet {
public:
    using ptr = std::shared_ptr<NotFoundServlet>;
    NotFoundServlet(std::string name);
    int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;

private:
    std::string m_name;
    std::string m_content;
};

}

#endif // RR_SERVLET_H