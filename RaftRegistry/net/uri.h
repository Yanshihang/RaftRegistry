//
// File created on: 2024/03/16
// Author: Zizhou

#ifndef RR_URI_H
#define RR_URI_H

#include <cstdint>
#include <coroutine>
#include <memory>
#include <string>
#include <ostream>
#include <sstream>

#include "RaftRegistry/net/address.h"


namespace RR {


template <typename T>
class Task {
public:
    class promise_type;
    using co_handle = std::coroutine_handle<promise_type>; // 使用协程句柄管理协程

    class promise_type {
    public:
        Task get_return_object() {
            return {std::coroutine_handle<promise_type>::from_promise(*this)}; // 返回一个包含当前协程的句柄的列表，这个列表用于初始化Task对象
        }

        std::suspend_always initial_suspend() noexcept {
            return {};
        }

        std::suspend_always final_suspend() noexcept {
            return {};
        }

        void return_value(T value) {
            m_value = std::move(value);
        }

        // 返回类型决定了每次co_yield都会挂起
        std::suspend_always yield_value(T value) {
            m_val = std::move(value);
            return {};
        }

        void unhandled_exception() {
            std::terminate(); // 直接终止程序
        }

        // 存储协程的返回值或生成的值
        T m_value;
    };

    Task() : m_handle(nullptr) {};

    Task(co_handle handle) : m_handdle(handle) {};
    Task(const Task& rhs) = delete; // 禁止拷贝构造函数
    Task(Task&& rhs) noexcept : m_handdle(rhs.m_handle) {
        rhs.m_handle = nullptr;
    }
    Task& operator=(const Task& rhs) = delete; // 禁止拷贝赋值
    Task& operator=(Task&& rhs) noexcept {
        if (std::addressof(rhs)!=this) { // std::addressof()返回对象的实际地址
            if (m_handle) {
                m_handle.destroy();
            }
            m_handle = rhs.m_handle;
            rhs.m_handle = nullptr;
        }
        return *this;
    }

    // 获取协程的返回值
    T get() {
        return m_handle.promise().m_value;
    }

    void resume() {
        m_handle.resume();
    }

    bool done() {
        return !m_handle || m_handle.done();
    }

    void destroy() {
        // 调用协程的destroy函数之前需要先判断协程是否已经完成
        if (done()) {
            m_handle.destroy();
            m_handle = nullptr;
        }
    }

    ~Task() {
        if (m_handle) {
            m_handle.destroy();
        }
    }

private:
    co_handle m_handle = nullptr;
};

class Uri {
public:
    using ptr = std::shared_ptr<Uri>;

    Uri();

    // 根据传入的uri字符串创建Uri对象
    static Uri::ptr Create(const std::string& uri);

    Address::ptr createAddress();
    const std::string& getScheme() const;
    const std::string& getUserinfo() const;
    const std::string& getHost() const;
    const std::string& getPath() const;
    const std::string& getQuery() const;
    const std::string& getFragment() const;
    uint32_t getPort() const;

    void setScheme(const std::string& scheme);
    void setUserinfo(const std::string& userinfo);
    void setHost(const std::string& host);
    void setPath(const std::string& path);
    void setQuery(const std::string& query);
    void setFragment(const std::string& fragment);

    /**
     * @brief 将Uri对象的信息输出到ostream中
     * 
     * @param os 传入输出流对象
     * @return std::ostream& 返回输出流对象
     */
    std::ostream& dump(std::ostream& os) const;
    
    /**
     * @brief 将Uri对象的信息转换为字符串
     */
    std::string toString();

private:
    Task<bool> parse();
    bool isDefaultPort() const;

    // Uri类的数据成员，用于存储URI的不同部分
    std::string m_scheme;
    std::string m_userinfo;
    std::string m_host;
    std::string m_path;
    std::string m_query;
    std::string m_fragment;
    uint32_t m_port;

    const char* m_cur; // 当前解析位置指针
    bool m_error{}; // 解析错误标志，使用列表初始化，被初始化为布尔类型的默认值，即false
};

}

#endif // RR_URI_H