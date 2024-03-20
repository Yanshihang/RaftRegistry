//
// File created on: 2024/03/19
// Author: Zizhou

#include "RaftRegistry/http/parse.h"
#include "RaftRegistry/common/config.h"
#include "spdlog/spdlog.h"

namespace RR::http {

#ifdef co_yield
#undef co_yield
#endif

static auto Logger = GetLoggerInstance();

// 定义配置变量，用于设置 HTTP 请求缓冲区大小
static RR::ConfigVar<uint64_t>::ptr g_http_request_buffer_size = RR::Config::LookUp("http.request.buffer_size", static_cast<uint64_t>(4*1024), "http request buffer size");

// 定义配置变量，用于设置 HTTP 请求最大正文大小
static RR::ConfigVar<uint64_t>::ptr g_http_request_max_body_size = RR::Config::LookUp("http.request.max_body_size", static_cast<uint64_t>(64*1024*1024), "http request max body size");

// 定义配置变量，用于设置 HTTP 响应缓冲区大小
static RR::ConfigVar<uint64_t>::ptr g_http_response_buffer_size = RR::Config::LookUp("http.response.buffer_size", static_cast<uint64_t>(4*1024), "http response buffer size");

// 定义配置变量，用于设置 HTTP 响应最大正文大小
static RR::ConfigVar<uint64_t>::ptr g_http_response_max_body_size = RR::Config::LookUp("http.response.max_body_size", static_cast<uint64_t>(64*1024*1024), "http v max body size");

// 定义静态变量，用于存储 HTTP 请求缓冲区大小
static uint64_t s_http_request_buffer_size = 0;
// 定义静态变量，用用户存储 HTTP 请求最大正文大小
static uint64_t s_http_request_max_body_size = 0;
// 定义静态变量，用于存储 HTTP 响应缓冲区大小
static uint64_t s_http_response_buffer_size = 0;
// 定义静态变量，用于存储 HTTP 响应最大正文大小
static uint64_t s_http_response_max_body_size = 0;

// 获取http请求缓冲区大小的函数实现
uint64_t HttpRequestParser::GetHttpRequestBufferSize() {
    return s_http_request_buffer_size;
}

// 获取http请求体最大大小的函数实现
uint64_t HttpRequestParser::GetHttpRequestMaxBodySize() {
    return s_http_request_max_body_size;
}

// 获取http响应缓冲区大小的函数实现
uint64_t HttpResponseParser::GetHttpResponseBufferSize() {
    return s_http_response_buffer_size;
}

// 获取http响应体最大大小的函数实现
uint64_t HttpResponseParser::GetHttpResponseMaxBodySize() {
    return s_http_response_max_body_size;
}

// 初始化请求和响应大小的匿名命名空间，用于初始化配置变量
namespace {
// 定义一个（初始化器）结构体，用于初始化请求和响应大小
struct RequestSizeIniter {
    RequestSizeIniter() {
        // 初始化请求和响应缓冲区的大小，以及最大请求体和响应体的大小
        s_http_request_buffer_size = g_http_request_buffer_size->getValue();
        s_http_request_max_body_size = g_http_request_max_body_size->getValue();
        s_http_response_buffer_size = g_http_response_buffer_size->getValue();
        s_http_response_max_body_size = g_http_response_max_body_size->getValue();

        g_http_request_buffer_size->addListener(
            [](const uint64_t& ov, const uint64_t& nv) {
                s_http_request_buffer_size = nv;
            }
        );

        g_http_request_max_body_size->addListener(
            [](const uint64_t& ov, const uint64_t& nv) {
                s_http_request_max_body_size = nv;
            }
        );

        g_http_response_buffer_size->addListener(
            [](const uint64_t& ov, const uint64_t& nv) {
                s_http_response_buffer_size = nv;
            }
        );

        g_http_response_max_body_size->addListener(
            [](const uint64_t& ov, const uint64_t& nv) {
                s_http_response_max_body_size = nv;
            }
        );
    }
// 静态实例化初始化器，以自动执行初始化
static RequestSizeIniter init;
};
}

HttpParser::HttpParser() {

}

HttpParser::~HttpParser() {

}

/**
 * @brief 解析协议
 * 
 * @param data 协议文本内容
 * @param len 协议文本长度
 * @param chunk 是否有chunk的标识
 * @return size_t 返回实际解析长度，并将已解析的数据移除
 */
size_t HttpParser::execute(char* data, size_t len, bool chunk) {
    // 如果是chunked传输编码，设置状态并初始化解析任务
    if (chunk) {
        if (m_checkstate != CHECK_CHUNK) {
            m_checkstate = CHECK_CHUNK;
            m_finish = false;
            m_parser = parse_chunk();
        }
    }

    // 初始化偏移量和索引
    size_t offset = 0;
    size_t i = 0;
    // 根据当前的检查状态，决定解析的逻辑
    switch (m_checkstate) {
        case NO_CHECK: // 如果未检查，则开始进行第一步，即解析请求行
            m_checkstate = CHECK_LINE;
            m_parser = parse_line();
        case CHECK_LINE: // 处理HTTP请求/响应的起始行解析
            for (;i < len; ++i) {
                m_cur = data +i; // 指向要解析的字符
                m_parser.resume(); // 恢复解析器
                m_error = m_parser.get(); // 获取解析器的错误状态
                ++offset; // 偏移量加一
                if (isFinished()) { // 判断是否结束（包括发生错误）
                    memmove(data, data+offset, len-offset); // 将data中未被解析的数据移到数组的开始位置
                    return offset; // 返回解析的长度
                }
                if (m_checkstate == CHECK_HEADER) { // 判断是否应该进入解析头部的状态
                    ++i; // 跳过空白行
                    m_parser = parse_header();
                    break; // 用于跳出for循环
                }
            }
            if (m_checkstate != CHECK_HEADER) {
                memmove(data, data+offset, len-offset);
                return offset;
            }
        case CHECK_HEADER: // 处理http请求头
            for (;i < len; ++i) {
                m_cur = data+i;
                m_parser.resume();
                m_error = m_parser.get();
                ++offset;
                if (isFinished()) {
                    memmove(data, data+offset, len-offset);
                    return offset;
                }
                if (m_checkstate == CHECK_CHUNK) {
                    ++i;
                    m_parser = parse_chunk();
                    break;
                }
            }
            if (m_checkstate != CHECK_CHUNK) {
                memmove(data, data+offset, len-offset);
                return offset;
            }
        case CHECK_CHUNK: 
            for(; i < len; ++i){
                m_cur = data + i;
                m_parser.resume();
                m_error = m_parser.get();
                ++offset;
                if(isFinished()){
                    memmove(data, data + offset, (len - offset));
                    return offset;
                }
            }
            break;
    }
    memmove(data, data + offset, len - offset);
    return offset;
}

// 判断是否解析完成的函数
int HttpParser::isFinished() {
    if (hasError()) {
        return -1; // 解析出错
    } else if (m_finish) {
        return 1; // 解析完成
    } else {
        return 0; // 解析未完成
    }
}

// 判断是否有错误的函数
int HttpParser::hasError() {
    return m_error;
}

// 设置错误的函数
void HttpParser::setError(Error err) { m_error = err;}

uint64_t HttpRequestParser::getContentLength() {
    return m_data->getHeaderAs<uint64_t>("content-length", 0);
}


void HttpRequestParser::on_request_method(const std::string& str) {
    HttpMethod method = StringToMethod(str);
    if (method == HttpMethod::INVALID_METHOD) {
        SPDLOG_LOGGER_ERROR(Logger, "invalid http request method = {}", str);
        return ;
    }
    m_data->setMethod(method);
}

void HttpRequestParser::on_request_path(const std::string& str) {
    m_data->setPath(str);
}

void HttpRequestParser::on_request_query(const std::string& str) {
    // 先解析出路径中的参数
    std::string key, value; // 保存参数的键和值
    size_t i,j;
    for (i = 0, j=0;i < str.size();++i) {
        if (str[i] == '=') {
            key = str.substr(j, i-j); // 将键保存到key中
            j=i+1; // 更新j的值
        }else if (str[i] == '&') {
            value = str.substr(j, i-j);
            j = i+1;
            m_data->setParams(key,value);
            key.clear();
            value.clear();
        }
    }
    // 处理字符串末尾的键值对
    if (key.size()) {
        value = str.substr(j, i-j);
        if (value.size()) {
            m_data->setParams(key, value);
        }
    }

    m_data->setQuery(std::move(str));
}

void HttpRequestParser::on_request_fragment(const std::string& str) {
    m_data->setFragment(str);
}

void HttpRequestParser::on_request_version(const std::string& str) {
    if (str == "1.1") {
        m_data->setVersion(static_cast<uint8_t>(0x11));
    } else if (str == "1.0") {
        m_data->setVersion(static_cast<uint8_t>(0x10));
    } else {
        SPDLOG_LOGGER_WARN(Logger, "invalid http request version {}", str);
        setError(INVALID_VERSION);
    }
}

void HttpRequestParser::on_request_header(const std::string& key, const std::string& val) {
    if (key.empty()) {
        SPDLOG_LOGGER_ERROR(Logger, "invalid http request field length = 0");
        return;
    }
    m_data->setHeader(key, val);
}

void HttpRequestParser::on_request_header_done() {
    m_data->setClose(m_data->getHeaders("Connection", "") != "keep-alive");
}

Task<HttpParser::Error> HttpRequestParser::parse_line() {
    std::string buffer; // 临时存储解析完的那部分数据

    // 读取method
    while (std::isalpha(*m_cur)) {
        buffer.push_back(*m_cur);
        co_yield NO_ERROR;
    }

    if (buffer.empty()) { // 如果读完method后，method为空，则格式错误
        co_return INVALID_METHOD;
    }

    if (*m_cur != ' ') { // 如果method之后不是空格，否则格式错误。
        co_return INVALID_METHOD;
    } else {
        on_request_method(buffer);
        if (m_error) {
            co_return INVALID_METHOD;
        }
        buffer.clear();
        co_yield NO_ERROR;
    }

    // 读取path
    while(std::isprint(*m_cur) && strchr(" ?", *m_cur) == nullptr) {
        buffer.push_back(*m_cur);
        co_yield NO_ERROR;
    }
    if (buffer.empty()) { // path为空，格式错误
        co_return INVALID_PATH;
    }

    if (*m_cur == '?') { // 如果path之后有参数，则解析参数
        on_request_path(buffer);
        buffer.clear();
        co_yield NO_ERROR;
        
        // 读取query
        while(std::isprint(*m_cur) && strchr(" #", *m_cur) == nullptr) {
            buffer.push_back(*m_cur);
            co_yield NO_ERROR;
        }

        if (*m_cur == '#') { // 如果query之后是#，则下面是fragment
            on_request_query(buffer);
            buffer.clear();

            // 读取fragment
            while(std::isprint(*m_cur) && strchr(" ",*m_cur) == nullptr) {
                buffer.push_back(*m_cur);
                co_yield NO_ERROR;
            }
            if (*m_cur != ' ') { // 如果fragment之后不是空格，格式错误
                co_return INVALID_PATH;
            } else {
                on_request_fragment(buffer);
                buffer.clear();
                co_yield NO_ERROR;
            }
        } else if(*m_cur != ' ') { // 如果query之后既不是#也不是空格，格式错误
            co_return INVALID_PATH;
        } else { // 如果query之后是空格，则解析完成
            on_request_query(buffer);
            buffer.clear();
            co_yield NO_ERROR;
        }
    } else if (*m_cur != ' ') {
        co_return INVALID_PATH;
    } else {
        on_request_path(buffer);
        buffer.clear();
        co_yield NO_ERROR;
    }

    // 判断path之后的版本是否是HTTP/1.0或者HTTP/1.1
    const char* version = "HTTP/1.";
    while(*version) {
        if (*m_cur != *version) {
            co_return INVALID_VERSION;
        }
        version++;
        co_yield NO_ERROR;
    }
    if (*m_cur != '1' && *m_cur != '0') {
        co_return INVALID_VERSION;
    } else {
        buffer = "1.";
        buffer.push_back(*m_cur);
        on_request_version(buffer);
        if (m_error) {
            co_return INVALID_VERSION;
        }
        buffer.clear();
        co_yield NO_ERROR;
    }

    if (*m_cur != '\r') {
        co_return INVALID_LINE;
    } else {
        co_yield NO_ERROR;
    }
    if (*m_cur != '\n') {
        co_return INVALID_LINE;
    } else {
        co_yield NO_ERROR;
    }

    // 请求行解析完成，进入头部解析
    m_checkstate = CHECK_HEADER;
    co_return NO_ERROR;
}

Task<HttpParser::Error> HttpRequestParser::parse_header() {
    std::string key, value;
    while(!isFinished()) { // 这里循环检测是否解析完成，是因为最后一次解析完成后，会将m_finish置为true
        while(std::isprint(*m_cur) && *m_cur != ':') { // 读取所有连续的字符，存入key中
            key.push_back(*m_cur);
            co_yield NO_ERROR;
        }
    }

    if (*m_cur != ':') { // 如果key之后不是冒号，格式错误
        co_return INVALID_HEADER;
    }
    // 如果key之后是冒号，跳过冒号
    co_yield NO_ERROR;
    while(*m_cur == ' ') { // 一直循环空格字符，知道出现第一个非空格，则是value的开始
        co_yield NO_ERROR;
    }

    // 将value存入value中
    while (std::isprint(*m_cur)) {
        value.push_back(*m_cur);
        co_yield NO_ERROR;
    }
    

    // 如果value后面不是\r，格式错误
    if (*m_cur != '\r') {
        co_return INVALID_HEADER;
    }
    co_yield NO_ERROR;
    // 如果\r后，不是\n，格式错误
    if (*m_cur != '\n') {
        co_return INVALID_HEADER;
    }

    co_yield NO_ERROR;

    // 如果\r\n后，还是\r\n，则头部解析完成
    if (*m_cur == '\r') {
        co_yield NO_ERROR;
        if (*m_cur != '\n') {
            co_return INVALID_HEADER;
        }
        on_request_header(key,value);
        on_request_header_done();
        m_finish = true;
        m_checkstate = NO_CHECK;
    } else { // 如果\r\n后，不是\r\n，则继续解析头部
        on_request_header(key, value);
        key.clear();
        value.clear();
        co_yield NO_ERROR;
    }
    m_checkstate = NO_CHECK;
    co_return NO_ERROR;

}

Task<HttpParser::Error> HttpRequestParser::parse_chunk() {
    // 暂不解析chunk
    co_return NO_ERROR;
}

uint64_t HttpResponseParser::getContentLength() {
    return m_data->getHeaderAs<uint64_t>("content-length", 0);
}

bool HttpResponseParser::isChunked() {
    return m_data->getHeaders("Transfer-Encoding", "").size();
}

void HttpResponseParser::on_response_version(const std::string& str) {
    if (str == "1.1") {
        m_data->setVersion(0x11);
    } else if (str == "1.0") {
        m_data->setVersion(0x10);
    } else {
        SPDLOG_LOGGER_WARN(Logger, "invalid http response version = {}", str);
        setError(INVALID_VERSION);
        return;
    }
}
void HttpResponseParser::on_response_status(const std::string& str) {
    HttpStatus status = static_cast<HttpStatus>(std::stoi(str));
    m_data->setStatus(status);
}
void HttpResponseParser::on_response_reason(const std::string& str) {
    m_data->setReason(std::move(str));
}
void HttpResponseParser::on_response_header(const std::string& key, const std::string& val) {
    if (key.empty()) {
        SPDLOG_LOGGER_ERROR(Logger, "invalid http response field length = 0");
        return;
    }
    m_data->setHeader(key, val);
}
void HttpResponseParser::on_response_header_done() {
    m_data->setClose(m_data->getHeaders("Connection", "") != "keep-alive");
}
void HttpResponseParser::on_response_chunk(const std::string& str) {
    m_data->setBody(str);
}

Task<HttpParser::Error> HttpResponseParser::parse_line() {
    std::string buffer;

    // 读取版本
    const char* version = "HTTP/1.";
    while(*version) {
        if (*m_cur == *version) {
            version++;
            co_yield NO_ERROR;
        }
        co_return INVALID_VERSION;
    }

    if (*m_cur == '1' || *m_cur == '0') {
        buffer = "1.";
        buffer.push_back(*m_cur);
        on_response_version(buffer);
        if (m_error) {
            co_return INVALID_VERSION;
        }
        buffer.clear();
        co_yield NO_ERROR;
    }

    // 跳过空格
    while(*m_cur == ' ') {
        co_yield NO_ERROR;
    }

    // 读取status
    while(std::isdigit(*m_cur)) {
        buffer.push_back(*m_cur);
        co_yield NO_ERROR;
    }
    if (buffer.empty()) {
        co_return INVALID_CODE;
    }
    if (*m_cur != ' ') {
        co_return INVALID_CODE;
    } else {
        on_response_status(buffer);
        buffer.clear();
        co_yield NO_ERROR;
    }

    // 跳过空格
    while(*m_cur == ' ') {
        co_yield NO_ERROR;
    }

    // 读取reason
    while (std::isalpha(*m_cur) || *m_cur == ' ') {
        buffer.push_back(*m_cur);
        co_yield NO_ERROR;
    }
    if (buffer.empty()) {
        co_return INVALID_REASON;
    }
    if (*m_cur != '\r') {
        co_return INVALID_REASON;
    }
    co_yield NO_ERROR;
    if (*m_cur != '\n') {
        co_return INVALID_REASON;
    }
    on_response_reason(buffer);
    buffer.clear();
    co_yield NO_ERROR;
    
    // 进入头部解析
    m_checkstate = CHECK_HEADER;
    co_return NO_ERROR;
}
Task<HttpParser::Error> HttpResponseParser::parse_header() {
        std::string key, value;
    while(!isFinished()) { // 这里循环检测是否解析完成，是因为最后一次解析完成后，会将m_finish置为true
        while(std::isprint(*m_cur) && *m_cur != ':') { // 读取所有连续的字符，存入key中
            key.push_back(*m_cur);
            co_yield NO_ERROR;
        }
    }

    if (*m_cur != ':') { // 如果key之后不是冒号，格式错误
        co_return INVALID_HEADER;
    }
    // 如果key之后是冒号，跳过冒号
    co_yield NO_ERROR;
    while(*m_cur == ' ') { // 一直循环空格字符，知道出现第一个非空格，则是value的开始
        co_yield NO_ERROR;
    }

    // 将value存入value中
    while (std::isprint(*m_cur)) {
        value.push_back(*m_cur);
        co_yield NO_ERROR;
    }
    

    // 如果value后面不是\r，格式错误
    if (*m_cur != '\r') {
        co_return INVALID_HEADER;
    }
    co_yield NO_ERROR;
    // 如果\r后，不是\n，格式错误
    if (*m_cur != '\n') {
        co_return INVALID_HEADER;
    }

    co_yield NO_ERROR;

    // 如果\r\n后，还是\r\n，则头部解析完成
    if (*m_cur == '\r') {
        co_yield NO_ERROR;
        if (*m_cur != '\n') {
            co_return INVALID_HEADER;
        }
        on_response_header(key,value);
        on_response_header_done();
        m_finish = true;
        m_checkstate = NO_CHECK;
    } else { // 如果\r\n后，不是\r\n，则继续解析头部
        on_response_header(key, value);
        key.clear();
        value.clear();
        co_yield NO_ERROR;
    }
    m_checkstate = NO_CHECK;
    co_return NO_ERROR;
}
Task<HttpParser::Error> HttpResponseParser::parse_chunk()  {
    // 存储chunk的内容
    std::string body; 
    // 存储chunk的长度
    size_t len = 0;

    // 无限循环，读取和解析chunk
    while (true) {
        len = 0;
        // 读取chunk的长度，由于chunk的长度是16进制，所以需要特殊处理
        while(true) {
            if (std::isdigit(*m_cur)) {
                len = len*16 + *m_cur - '0'; // 因为
            } else if (*m_cur >= 'a' && *m_cur <='f') {
                len = len*16 + *m_cur-'a' + 10;
            } else if (*m_cur >= 'A' && *m_cur <= 'F') {
                len = len*16 + *m_cur - 'A' + 10;
            } else {
                break;
            }
            co_yield NO_ERROR;
        }

        // 如果不是\r\n，格式错误

        if (*m_cur != '\r') {
            co_return INVALID_CHUNK;
        }
        co_yield NO_ERROR;
        if (*m_cur != '\n') {
            co_return INVALID_CHUNK;
        }
        co_yield NO_ERROR;

        // 读取长度为len的数据，并将其添加到body字符串中
        for (size_t i = 0;i < len; ++i) {
            body.push_back(*m_cur);
            co_yield NO_ERROR;
        }

        // 如果不是\r\n，格式错误
        if (*m_cur != '\r') {
            co_return INVALID_CHUNK;
        }
        co_yield NO_ERROR;
        if (*m_cur != '\n') {
            co_return INVALID_CHUNK;
        }

        // 如果len为0，表示所有数据都已接收，chunk解析完成
        if (!len) {
            break;
        }
        co_yield NO_ERROR;
    }

    on_response_chunk(body);
    m_finish = true;

}

}