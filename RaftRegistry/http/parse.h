//
// File created on: 2024/03/19
// Author: Zizhou

#ifndef RR_PARSE_H
#define RR_PARSE_H

#include <coroutine>
#include <string>
#include "third_party/json/single_include/nlohmann/json.hpp"
#include "RaftRegistry/common/lexical_cast.h"
#include "RaftRegistry/http/http.h"

namespace RR::http {

template <typename T>
class Task {
public:
    struct promise_type;
    using handle = std::coroutine_handle<promise_type>;

    struct promise_type {
        Task get_return_object() {
            return {handle::from_promise(*this)};
        }

        std::suspend_always initial_suspend() {
            return {};
        }
        std::suspend_always final_suspend() {
            return {};
        }

        void return_value(T val) {
            m_val = std::move(val);
        }

        std::suspend_always yield_value(T val) {
            m_val = std::move(val);
            return {};
        }

        void unhandled_exception() {
            std::terminate();
        }

        T m_val;
    };

    Task() : m_handle(nullptr) {}

    Task(handle h) : m_handle(h) {}

    // Task不能用于拷贝，但是可以移动

    Task(const Task& rhs) = delete;

    Task& operator= (const Task& rhs) = delete;

    Task(Task&& rhs) noexcept {
        m_handle = rhs.m_handle;
        rhs.m_handle = nullptr;
    }

    Task& operator= (Task&& rhs) noexcept {
        if (std::addressof(rhs) == this) {
            return *this;
        }

        if (m_handle) {
            m_handle.destroy();
        }
        m_handle = rhs.m_handle;
        rhs.m_handle = nullptr;
    }

    T get() {
        return m_handle.promise().m_val;
    }

    void resume() {
        m_handle.resume();
    }

    bool done() {
        return !m_handle || m_handle.done();
    }

    void destroy() {
        m_handle.destroy();
        m_handle = nullptr;
    }

    ~Task() {
        if (m_handle) {
            m_handle.destroy();
        }
    }

private:
    handle m_handle = nullptr;
};

class HttpParser {
public:
    // 错误码枚举
    enum Error {
        NO_ERROR = 0,
        INVALID_METHOD,
        INVALID_PATH,
        INVALID_VERSION,
        INVALID_LINE,
        INVALID_HEADER,
        INVALID_CODE,
        INVALID_REASON,
        INVALID_CHUNK
    };

    // 定义了HTTP解析过程中可能的状态
    enum CheckState {
        NO_CHECK, // 初始状态，表示还没有开始解析。
        CHECK_LINE, // 正在解析HTTP的起始行
        CHECK_HEADER, // 正在解析HTTP的头部
        CHECK_CHUNK // 正在解析HTTP的chunk
    };

    HttpParser();
    virtual ~HttpParser();

    /**
     * @brief 解析协议
     * 
     * @param data 协议文本内容
     * @param len 协议文本长度
     * @param chunk 是否有chunk的标识
     * @return size_t 返回实际解析长度，并将已解析的数据移除
     */
    size_t execute(char* data, size_t len, bool chunk = false);

    // 判断是否解析完成的函数
    int isFinished();

    // 判断是否有错误的函数
    int hasError();

    // 设置错误的函数
    void setError(Error err);

protected:
    // 纯虚函数，解析请求行
    virtual Task<Error> parse_line() = 0;
    // 纯虚函数，解析请求头
    virtual Task<Error> parse_header() = 0;
    // 纯虚函数，解析chunk
    virtual Task<Error> parse_chunk() = 0;

protected:
    // 解析过程的错误码
    int m_error = 0;
    // 是否解析完成
    bool m_finish = false;
    // 当前解析位置
    char* m_cur = nullptr;

    Task<Error> m_parser{};
    CheckState m_checkstate = NO_CHECK;
};

class HttpRequestParser : public HttpParser {
public:
    using ptr = std::shared_ptr<HttpRequestParser>;
    HttpRequestParser() : HttpParser(), m_data(new HttpRequest) {}

    /**
     * @brief 返回解析的HttpRequest对象
     */
    HttpRequest::ptr getData() const { return m_data;}

    /**
     * @brief 获取消息体长度的函数
     */
    uint64_t getContentLength();

    // 获取请求的buffer大小
    static uint64_t GetHttpRequestBufferSize();

    // 获取请求的最大body大小
    static uint64_t GetHttpRequestMaxBodySize();


private:
    // 处理请求方法的函数
    void on_request_method(const std::string& str);

    // 处理请求路径的函数
    void on_request_path(const std::string& str);
    // 处理请求查询参数的函数
    void on_request_query(const std::string& str);
    // 处理请求片段的参数
    void on_request_fragment(const std::string& str);
    // 处理请求版本的函数
    void on_request_version(const std::string& str);
    // 处理请求头的函数
    void on_request_header(const std::string& key, const std::string& val);
    // 请求头部解析完成的回调函数
    void on_request_header_done();

protected:
    Task<Error> parse_line() override;
    Task<Error> parse_header() override;
    Task<Error> parse_chunk() override;


    HttpRequest::ptr m_data;

};


class HttpResponseParser : public HttpParser {
public:
    using ptr = std::shared_ptr<HttpResponseParser>;
    HttpResponseParser() : HttpParser(), m_data(new HttpResponse) {}

    HttpResponse::ptr getData() const { return m_data;}

    uint64_t getContentLength();

    /**
     * @brief 获取消息体是否为chunk格式
     */
    bool isChunked();

    static uint64_t GetHttpResponseBufferSize();
    static uint64_t GetHttpResponseMaxBodySize();

private:
    void on_response_version(const std::string& str);
    void on_response_status(const std::string& str);
    void on_response_reason(const std::string& str);
    void on_response_header(const std::string& key, const std::string& val);
    void on_response_header_done();
    void on_response_chunk(const std::string& str);

protected:
    Task<Error> parse_line() override;
    Task<Error> parse_header() override;
    Task<Error> parse_chunk() override;

    // 成员变量，存储 HttpResponse 对象
    HttpResponse::ptr m_data;
};


} // namespace RR::http


#endif // RR_PARSE_H