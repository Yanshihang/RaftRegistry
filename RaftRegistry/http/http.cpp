//
// File created on: 2024/03/19
// Author: Zizhou

#include "http.h"
#include "RaftRegistry/common/util.h"
#include "third_party/spdlog/spdlog.h"

namespace RR::http {
static auto Logger = GetLoggerInstance();

HttpMethod StringToMethod(const std::string& method) {
#define METHOD(index, name, string) \
    if (strcmp(method.c_str(), #string) == 0) {  \
        return HttpMethod::name;    \
    }
    HTTP_METHOD_MAP(METHOD)
#undef METHOD
    return HttpMethod::INVALID_METHOD;
}

HttpMethod CharsToMethod(const char* method) {
#define METHOD(index, name, string) \
    if (strcmp(method, #string) == 0) { \
        return HttpMethod::name;     \
    }
    HTTP_METHOD_MAP(METHOD)
#undef METHOD
    return HttpMethod::INVALID_METHOD;
}

HttpContentType StringToContentType(const std::string& type) {
#define CONTENT_TYPE(name, string)  \
    if (strcmp(type, #string) == 0) {    \
        return HttpContentType::name;   \
    }
#undef CONTENT_TYPE
    return HttpContentType::INVALID_TYPE;
}

HttpContentType CharsToContentType(const char* type) {
#define CONTENT_TYPE(name, string)  \
    if (strcmp(type, #string) == 0) {    \
        return HttpContentType::name;   \
    }
    HTTP_CONTENT_TYPE_MAP(CONTENT_TYPE)
}

// 定义静态字符指针数组
static const char* s_method_string[] = {
#define METHOD(index, name, string) #string,
    HTTP_METHOD_MAP(METHOD)
#undef METHOD
};

std::string HttpMethodToString(HttpMethod method) {
    uint32_t index = static_cast<uint32_t>(method);
    if (index >= (sizeof(s_method_string) / sizeof(s_method_string[0]))) {
        return "INVALID_METHOD";
    }
    return s_method_string[index];
}


std::string HttpStatusToString(HttpStatus status) {
    switch (status) {
        #define STATUS(code, name ,string)  \
            case HttpStatus::name:  \
                return #string;
            HTTP_STATUS_MAP(STATUS)
        #undef STATUS
            default:
                return "INVALID_STATUS";
    }
}

std::string HttpContentTypeToString(HttpContentType contentType) {
    switch (contentType) {
        #define CONTENT_TYPE(name, string)   \
            case HttpContentType::name: \
                return #string;
            HTTP_CONTENT_TYPE_MAP(CONTENT_TYPE)
        #undef CONTENT_TYPE
            default:
                return "INVALID_TYPE";
    }
}

bool CaseInsensitiveLess::operator() (const std::string& lhs, const std::string& rhs) const {
    return (strcasecmp(lhs.c_str(), rhs.c_str()) < 0);
}

HttpRequest::HttpRequest(uint8_t version, bool close) : m_method(HttpMethod::GET), m_version(version), m_close(close), m_websocket(false), m_path("/") {}

Json HttpRequest::getJson() const {
    try {
        return Json::parse(m_body);
    } catch(...) {
        SPDLOG_LOGGER_ERROR(Logger, "HttpRequest::getJson() fail, body={}", m_body);
        return  Json(); // 解析失败，返回空的Json对象
    }
}

void HttpRequest::setJson(const Json& json) {
    setContentType(HttpContentType::APPLICATION_JSON); // 设置内容类型为application/json
    m_body = json.dump(); // 将Json对象序列化为字符串，并设置为请求体
}


void HttpRequest::setHeaders(const std::string& key, const std::string& val) {
    m_headers[key] = val;
}

void HttpRequest::setParams(const std::string& key, const std::string& val) {
    m_params[key] = val;
}

void HttpRequest::setCookies(const std::string& key, const std::string& val) {
    m_cookies[key] = val;
}

std::string HttpRequest::getHeaders(const std::string& key, const std::string& def = "") const {
    auto iter = m_headers.find(key);
    if (iter == m_headers.end()) {
        return def;
    }
    return iter->second;
}

std::string HttpRequest::getParams(const std::string& key, const std::string& def = "") const {
    auto iter = m_params.find(key);
    if (iter == m_params.end()) {
        return def;
    }
    return iter->second;
}

std::string HttpRequest::getCookies(const std::string& key, const std::string& def = "") const {
    auto iter = m_cookies.find(key);
    if (iter == m_cookies.end()) {
        return def;
    }
    return iter->second;
}

void HttpRequest::delHeaders(const std::string& key) {
    m_headers.erase(key);
}

void HttpRequest::delParams(const std::string& key) {
    m_params.erase(key);
}

void HttpRequest::delCookies(const std::string& key) {
    m_cookies.erase(key);
}

bool HttpRequest::hasHeaders(const std::string& key, std::string* val) {
    auto iter = m_headers.find(key);
    if (iter == m_headers.end()) {
        return false;
    }
    if (val) {
        *val = iter->second;
    }
    return true;
}

bool HttpRequest::hasParams(const std::string& key, std::string* val) {
    auto iter = m_params.find(key);
    if (iter == m_params.end()) {
        return false;
    }
    if (val) {
        *val = iter->second;
    }
    return true;
}

bool HttpRequest::hasCookies(const std::string& key, std::string* val) {
    auto iter = m_cookies.find(key);
    if (iter == m_cookies.end()) {
        return false;
    }
    if (val) {
        *val = iter->second;
    }
    return true;
}

std::ostream& HttpRequest::dump(std::ostream& os) const {
    os << HttpMethodToString(m_method) << " " << m_path <<
        << m_query.empty()? "" : "?" << m_query << m_params.empty()?"":"#" << m_params
        <<  "HTTP/" << (m_version >> 4) << "." << (m_version  & 0x0F) << "\r\n";

    if (!m_websocket) {
        os << "connection" << (m_close ? "close" : "keep-alive") << "\r\n";
    }

    // 遍历请求头，将请求头写入请求行
    for ( auto& i : m_headers) {
        // 如果请求头中有connection字段，且不是websocket请求，跳过（因为前面已经写入了connection字段）
        if (!m_websocket && strcasecmp(i.first.c_str(), "connection") == 0) {
            continue;
        }
        os << i.first << ": " << i.second << "\r\n";
        
    }

    // 如果有cookie，将cookie写入请求头
    if (m_cookies) {
        os << "Cookies: ";
        for (const auto& i : m_cookies) {
            os << i.first << "=" << i.second << "; ";
        }
    }

    // 判断请求体是否为空
    if (m_body.empty()) {
        os << "\r\n";
    } else {
        os << "Content-Length: " << m_body.size() << "\r\n\r\n" << m_body;
    }

    return os;
}

std::string HttpRequest::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

HttpResponse::HttpResponse(uint8_t version, bool close) : m_status(HttpStatus::OK), m_version(version), m_close(close), m_websocket(false) {}

const std::string& HttpResponse::getHeaders(const std::string& key, const std::string& def = "") const {
    auto iter = m_headers.find(key);
    if (iter == m_headers.end()) {
        return def;
    }
    return iter->second;
}

void HttpResponse::setHeader(const std::string& key, const std::string& val) {
    m_headers[key] = val;
}

void HttpResponse::delHeader(const std::string& key) {
    m_headers.erase(key);
}

bool HttpResponse::hasHeader(const std::string& key, std::string* val = nullptr) {
    auto iter =m_headers.find(key);
    if (iter == m_headers.end()) {
        return false;
    }

    if (val) {
        *val = iter->second;
    }
    return true;
}

Json HttpResponse::getJson() const {
    try {
        return Json.parse(m_body);
    } catch (...) {
        SPDLOG_LOGGER_ERROR(Logger, "HttpResponse::getJson() fail, body={}", m_body);
        return Json();
    }
}

void HttpResponse::setJson(const Json& json) {
    setContentType(HttpContentType::APPLICATION_JSON);
    m_body = json.dump();
}

std::ostream& HttpResponse::dump(std::ostream& os) const {
    auto it = m_headers.find(key);
    if(it == m_headers.end()){
        return false;
    }
    val = &it->second;
    return true;
}
std::ostream& HttpResponse::dump(std::ostream& os) const {
    os  << "HTTP/"
        << ((uint32_t)m_version >> 4)
        << "."
        << ((uint32_t)m_version & 0xF)
        << " "
        << (uint32_t)m_status
        << " "
        << (m_reason.empty() ? HttpStatusToString(m_status) : m_reason)
        << "\r\n";
    if(!m_websocket){
        os << "connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
    }
    for(auto& i : m_headers) {
        if(!m_websocket && strcasecmp(i.first.c_str(), "connection") == 0) {
            continue;
        }
        os << i.first << ": " << i.second << "\r\n";
    }
    for(auto& i : m_cookies) {
        os << "Set-Cookie: " << i << "\r\n";
    }

    if(m_body.empty()){
        os << "\r\n";
    } else {
        if (getHeader("content-length","").empty()) {
            os << "content-length: " << m_body.size();
        }
        os  << "\r\n\r\n" << m_body;
    }
    return os;
}

std::string HttpResponse::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}
} // namespace RR::http