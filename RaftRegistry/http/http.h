//
// File created on: 2024/03/19
// Author: Zizhou

#include "third_party/json/single_include/nlohmann/json.hpp"
#include "RaftRegistry/common/lexical_cast.h"

// #include <cstdint>

namespace RR::http {
using Json = nlohmann::json;

// http请求方法的宏定义
#define HTTP_METHOD_MAP(METHOD) \
    METHOD(0,  DELETE,      DELETE)       \
    METHOD(1,  GET,         GET)          \
    METHOD(2,  HEAD,        HEAD)         \
    METHOD(3,  POST,        POST)         \
    METHOD(4,  PUT,         PUT)          \
    /* pathological */                \
    METHOD(5,  CONNECT,     CONNECT)      \
    METHOD(6,  OPTIONS,     OPTIONS)      \
    METHOD(7,  TRACE,       TRACE)        \
    /* WebDAV */                      \
    METHOD(8,  COPY,        COPY)         \
    METHOD(9,  LOCK,        LOCK)         \
    METHOD(10, MKCOL,       MKCOL)        \
    METHOD(11, MOVE,        MOVE)         \
    METHOD(12, PROPFIND,    PROPFIND)     \
    METHOD(13, PROPPATCH,   PROPPATCH)    \
    METHOD(14, SEARCH,      SEARCH)       \
    METHOD(15, UNLOCK,      UNLOCK)       \
    METHOD(16, BIND,        BIND)         \
    METHOD(17, REBIND,      REBIND)       \
    METHOD(18, UNBIND,      UNBIND)       \
    METHOD(19, ACL,         ACL)          \
    /* subversion */                  \
    METHOD(20, REPORT,      REPORT)       \
    METHOD(21, MKACTIVITY,  MKACTIVITY)   \
    METHOD(22, CHECKOUT,    CHECKOUT)     \
    METHOD(23, MERGE,       MERGE)        \
    /* upnp */                        \
    METHOD(24, MSEARCH,     M-SEARCH)     \
    METHOD(25, NOTIFY,      NOTIFY)       \
    METHOD(26, SUBSCRIBE,   SUBSCRIBE)    \
    METHOD(27, UNSUBSCRIBE, UNSUBSCRIBE)  \
    /* RFC-5789 */                    \
    METHOD(28, PATCH,       PATCH)        \
    METHOD(29, PURGE,       PURGE)        \
    /* CalDAV */                      \
    METHOD(30, MKCALENDAR,  MKCALENDAR)   \
    /* RFC-2068, section 19.6.1.2 */  \
    METHOD(31, LINK,        LINK)         \
    METHOD(32, UNLINK,      UNLINK)       \
    /* icecast */                     \
    METHOD(33, SOURCE,      SOURCE)       \

// 状态码的宏定义
#define HTTP_STATUS_MAP(STATUS) \
    STATUS(100,  CONTINUE,                        Continue)                       \
    STATUS(101,  SWITCHING_PROTOCOLS,             Switching Protocols)            \
    STATUS(102,  PROCESSING,                      Processing)                     \
    STATUS(103,  EARLY_HINTS,                     Early Hints)                    \
    STATUS(200,  OK,                              OK)                             \
    STATUS(201,  CREATED,                         Created)                        \
    STATUS(202,  ACCEPTED,                        Accepted)                       \
    STATUS(203, NON_AUTHORITATIVE_INFORMATION,   Non-Authoritative Information)   \
    STATUS(204, NO_CONTENT,                      No Content)                      \
    STATUS(205, RESET_CONTENT,                   Reset Content)                   \
    STATUS(206, PARTIAL_CONTENT,                 Partial Content)                 \
    STATUS(207, MULTI_STATUS,                    Multi-Status)                    \
    STATUS(208, ALREADY_REPORTED,                Already Reported)                \
    STATUS(226, IM_USED,                         IM Used)                         \
    STATUS(300, MULTIPLE_CHOICES,                Multiple Choices)                \
    STATUS(301, MOVED_PERMANENTLY,               Moved Permanently)               \
    STATUS(302, FOUND,                           Found)                           \
    STATUS(303, SEE_OTHER,                       See Other)                       \
    STATUS(304, NOT_MODIFIED,                    Not Modified)                    \
    STATUS(305, USE_PROXY,                       Use Proxy)                       \
    STATUS(307, TEMPORARY_REDIRECT,              Temporary Redirect)              \
    STATUS(308, PERMANENT_REDIRECT,              Permanent Redirect)              \
    STATUS(400, BAD_REQUEST,                     Bad Request)                     \
    STATUS(401, UNAUTHORIZED,                    Unauthorized)                    \
    STATUS(402, PAYMENT_REQUIRED,                Payment Required)                \
    STATUS(403, FORBIDDEN,                       Forbidden)                       \
    STATUS(404, NOT_FOUND,                       Not Found)                       \
    STATUS(405, METHOD_NOT_ALLOWED,              Method Not Allowed)              \
    STATUS(406, NOT_ACCEPTABLE,                  Not Acceptable)                  \
    STATUS(407, PROXY_AUTHENTICATION_REQUIRED,   Proxy Authentication Required)   \
    STATUS(408, REQUEST_TIMEOUT,                 Request Timeout)                 \
    STATUS(409, CONFLICT,                        Conflict)                        \
    STATUS(410, GONE,                            Gone)                            \
    STATUS(411, LENGTH_REQUIRED,                 Length Required)                 \
    STATUS(412, PRECONDITION_FAILED,             Precondition Failed)             \
    STATUS(413, PAYLOAD_TOO_LARGE,               Payload Too Large)               \
    STATUS(414, URI_TOO_LONG,                    URI Too Long)                    \
    STATUS(415, UNSUPPORTED_MEDIA_TYPE,          Unsupported Media Type)          \
    STATUS(416, RANGE_NOT_SATISFIABLE,           Range Not Satisfiable)           \
    STATUS(417, EXPECTATION_FAILED,              Expectation Failed)              \
    STATUS(421, MISDIRECTED_REQUEST,             Misdirected Request)             \
    STATUS(422, UNPROCESSABLE_ENTITY,            Unprocessable Entity)            \
    STATUS(423, LOCKED,                          Locked)                          \
    STATUS(424, FAILED_DEPENDENCY,               Failed Dependency)               \
    STATUS(426, UPGRADE_REQUIRED,                Upgrade Required)                \
    STATUS(428, PRECONDITION_REQUIRED,           Precondition Required)           \
    STATUS(429, TOO_MANY_REQUESTS,               Too Many Requests)               \
    STATUS(431, REQUEST_HEADER_FIELDS_TOO_LARGE, Request Header Fields Too Large) \
    STATUS(451, UNAVAILABLE_FOR_LEGAL_REASONS,   Unavailable For Legal Reasons)   \
    STATUS(500, INTERNAL_SERVER_ERROR,           Internal Server Error)           \
    STATUS(501, NOT_IMPLEMENTED,                 Not Implemented)                 \
    STATUS(502, BAD_GATEWAY,                     Bad Gateway)                     \
    STATUS(503, SERVICE_UNAVAILABLE,             Service Unavailable)             \
    STATUS(504, GATEWAY_TIMEOUT,                 Gateway Timeout)                 \
    STATUS(505, HTTP_VERSION_NOT_SUPPORTED,      HTTP Version Not Supported)      \
    STATUS(506, VARIANT_ALSO_NEGOTIATES,         Variant Also Negotiates)         \
    STATUS(507, INSUFFICIENT_STORAGE,            Insufficient Storage)            \
    STATUS(508, LOOP_DETECTED,                   Loop Detected)                   \
    STATUS(510, NOT_EXTENDED,                    Not Extended)                    \
    STATUS(511, NETWORK_AUTHENTICATION_REQUIRED, Network Authentication Required) \

// 内容类型的宏定义
#define HTTP_CONTENT_TYPE_MAP(CONTENT_TYPE)  \
    CONTENT_TYPE(TEXT_HTML,                 text/html)                  \
    CONTENT_TYPE(TEXT_PLAIN,                text/plain)                 \
    CONTENT_TYPE(TEXT_XML,                  text/xml)                   \
    CONTENT_TYPE(IMAGE_GIF,                 image/gif)                  \
    CONTENT_TYPE(IMAGE_JPEG,                image/jpeg)                 \
    CONTENT_TYPE(IMAGE_PNG,                 image/png)                  \
    CONTENT_TYPE(APPLICATION_XHTML,         application/xhtml+xml)      \
    CONTENT_TYPE(APPLICATION_ATOM,          application/atom+xml)       \
    CONTENT_TYPE(APPLICATION_JSON,          application/json)           \
    CONTENT_TYPE(APPLICATION_PDF,           application/pdf)            \
    CONTENT_TYPE(APPLICATION_MSWORD,        application/msword)         \
    CONTENT_TYPE(APPLICATION_STREAM,        application/octet-stream)   \
    CONTENT_TYPE(APPLICATION_URLENCODED,    application/x-www-form-urlencoded) \
    CONTENT_TYPE(APPLICATION_FORM_DATA,     application/form-data)      \

// http请求方法的枚举定义
enum class HttpMethod {
    #define METHOD(index, name, description) name = index,
    HTTP_METHOD_MAP(METHOD)
    #undef MTTHOD
    INVALID_METHOD
};

// 状态码的枚举定义
enum class HttpStatus {
    #define STATUS(code, name, description) name = code,
    HTTP_STATUS_MAP(STATUS)
    #undef STATUS

};

// 内容类型的枚举定义
enum class HttpContentType {
    #define CONTENT_TYPE(name, description) name,
    HTTP_CONTENT_TYPE_MAP(CONTENT_TYPE)
    #undef CONTENT_TYPE
    INVALID_TYPE
};

// 字符串转换为方法枚举
HttpMethod StringToMethod(const std::string& method);

// 字符数组转换为方法枚举
HttpMethod CharsToMethod(const char* method);

// 字符串转换为内容类型枚举
HttpContentType StringToContentType(const std::string& type);

// 字符数组转换为内容类型枚举
HttpContentType CharsToContentType(const char* type);

// 方法枚举转换为字符串
std::string HttpMethodToString(HttpMethod method);

// 状态枚举转换为字符串
std::string HttpStatusToString(HttpStatus status);

// 内容类型枚举转换为字符串
std::string HttpContentTypeToString(HttpContentType contentType);

// CaseInsensitiveLess结构体：用于实现忽略大小写的字符串比较
struct CaseInsensitiveLess {
    bool operator() (const std::string& lhs, const std::string& rhs) const;
};

/**
 * @brief 用于从一个映射（如 std::map 或任何具有类似接口的类型）中检索一个键对应的值，并尝试将这个值转换为指定的类型 T
 *
 * @tparam MapType 映射的类型，必须支持find方法。
 * @tparam T 要转换到的目标类型。
 * @param m 一个常量引用，指向从中检索值的映射。
 * @param key 要在映射中查找的键，类型为std::string。
 * @param def 如果键不存在或转换失败时返回的默认值。默认为T类型的默认构造实例。T() 创建了 T 类型的一个默认实例。
 * @return 转换后的值，如果键存在且转换成功；否则返回def。
 * @throws 依赖于LexicalCastFunc的实现，如果转换失败可能抛出异常。
 * 
 * @details 此函数尝试在映射m中找到由key指定的值，并将其转换为类型T。如果键不存在或转换失败，
 *          函数将返回提供的默认值def。转换是通过LexicalCastFunc实现的，可能会抛出异常。
 */
template <typename MapType, typename T>
static T GetAs (const MapType& m, const std::string& key, const T def = T()) {
    auto iter = m.find(key);
    if (iter == m.end()) {
        return def;
    }
    try {
        return LexicalCastFunc<T>(iter->second);
    } catch (...) {
        return def;
    }
}

/**
 * @brief 检查映射中是否存在指定的键，并尝试将其值转换为给定类型，然后将转换后的值存储在val中。
 * 
 * @details 此函数尝试在映射m中查找由key指定的键。如果找到键，它将尝试将键对应的值
 *          转换为类型T，并将转换后的值存储在val中。如果键不存在或转换失败，val将被设置为默认值def。
 *          函数返回一个布尔值，指示操作是否成功。
 *
 * @tparam MapType 映射的类型，必须支持find方法。
 * @tparam T 要转换到的目标类型。
 * @param m 一个常量引用，指向从中检索值的映射。
 * @param key 要在映射中查找的键，类型为std::string。
 * @param val 引用参数，用于存储转换后的值或默认值。
 * @param def 如果键不存在或转换失败时使用的默认值。默认为T类型的默认构造实例。
 * @return 如果键存在且值成功转换，则返回true；否则返回false。
 * @throws 依赖于lexical_cast的实现，如果转换失败可能抛出异常。
 */
template <typename MapType, typename T>
static bool CheckAndGetAs(const MapType& m, const std::string& key, T& val, const T def = T()) {
    auto iter = m.find(key);
    if (iter == m.end()) {
        val = def;
        return false;
    }
    try {
        val = LexicalCastFunc<T>(iter->second);
        return true;
    } catch (...) {
        val = def;
        return false;
    }
}

// POST请求方式的请求格式，第一行是请求行，第二行到空白行之前是请求头，空白行之后是请求体
// GET没有请求体，参数在URL中；POST有请求体，参数一般在请求体中
/*
POST /submit-form HTTP/1.1
Host: www.example.com
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.3
Content-Type: application/x-www-form-urlencoded
Content-Length: 27
Connection: keep-alive
Cookie: sessionId=abc123; username=john_doe

firstName=John&lastName=Doe
*/

class HttpRequest {
public:
    using ptr = std::shared_ptr<HttpRequest>;
    using MapType = std::map<std::string, std::string, CaseInsensitiveLess>;

    // 构造函数
    HttpRequest(uint8_t version = 0x11, bool close = true);

    HttpMethod getMethod() const { return m_method;}
    uint8_t getVersion() const { return m_version;}
    bool isClose() const { return m_close;}
    bool isWebsocket() const { return m_websocket;}
    const std::string& getPath() const { return m_path;}
    const std::string& getQuery() const { return m_query;}
    const std::string& getFragment() const { return m_fragment;}
    const std::string& getBody() const { return m_body;}

    /**
     * @brief 获取整个请求头
     * 
     * @return const MapType& 
     */
    const MapType& getHeaders() const { return m_headers;}

    /**
     * @brief 获取整个请求参数
     * 
     * @return const MapType& 
     */
    const MapType& getParams() const { return m_params;}

    /**
     * @brief 获取整个cookie
     * 
     * @return const MapType& 
     */
    const MapType& getCookies() const { return m_cookies;}

    /**
     * @brief 将HTTP请求体中的JSON字符串解析为Json对象
     * 
     * @return Json对象，如果解析失败，返回空的Json对象
     */
    Json getJson() const;

    void setMethod(HttpMethod method) { m_method = method; }
    void setVersion(uint8_t version) { m_version = version; }
    void setClose(bool close) { m_close = close; }
    void setWebsocket(bool websocket) { m_websocket = websocket; }
    void setPath(const std::string& path) { m_path = path; }
    void setQuery(const std::string& query) { m_query = query; }
    void setFragment(const std::string& fragment) { m_fragment = fragment; }
    void setBody(const std::string& body) { m_body = body; }
    void setHeaders(const MapType& headers) { m_headers = headers; }
    void setParams(const MapType& params) { m_params = params; }
    void setCookies(const MapType& cookies) { m_cookies = cookies; }
    void setContentType(HttpContentType contentType) { setHeader("Content-Type", HttpContentTypeToString(contentType));}
    void setJson(const Json& json);
    /**
     * @brief 添加指定键名的请求头
     * 
     * @param key 指定键名
     * @param val 对应的值
     */
    void setHeader(const std::string& key, const std::string& val);

    /**
     * @brief 添加指定键名的请求参数
     * 
     * @param key 指定键名
     * @param val 对应的值
     */
    void setParams(const std::string& key, const std::string& val);

    /**
     * @brief 添加指定键名的cookie
     * 
     * @param key 指定键名
     * @param val 对应的值
     */
    void setCookies(const std::string& key, const std::string& val);

    /**
     * @brief 获取请求头中的内容类型
     * 
     * @return 返回请求头中的内容类型
     */
    HttpContentType getContentTyep() const { return StringToContentType(getHeaders("Content-Type", ""));}
    
    /**
     * @brief 获取指定键名的请求头的值
     * 
     * @param key 指定的键名
     * @param def 返回的默认值
     * @return std::string 
     */
    std::string getHeaders(const std::string& key, const std::string& def = "") const;
    
    /**
     * @brief 获取指定键名的请求参数的值
     * 
     * @param key 指定的键名
     * @param def 返回的默认值
     * @return std::string 
     */
    std::string getParams(const std::string& key, const std::string& def = "") const;
    
    /**
     * @brief 获取指定键名的cookie的值
     * 
     * @param key 指定的键名
     * @param def 返回的默认值
     * @return std::string 
     */
    std::string getCookies(const std::string& key, const std::string& def = "") const;

    /**
     * @brief 删除指定键名的请求头
     * 
     * @param key 
     */
    void delHeaders(const std::string& key);

    /**
     * @brief 删除指定键名的请求参数
     * 
     * @param key 
     */
    void delParams(const std::string& key);

    /**
     * @brief 删除指定键名的cookie
     * 
     * @param key 
     */
    void delCookies(const std::string& key);

    /**
     * @brief 检测请求头中是否存在指定键名的键值对
     * 
     * @param key 指定键名
     * @param val 如果存在，则将对应的值存储在val中
     * @return true 
     * @return false 
     */
    bool hasHeaders(const std::string& key, std::string* val = nullptr);
    
    /**
     * @brief 检测请求参数中是否存在指定键名的键值对
     * 
     * @param key 指定键名
     * @param val 如果存在，则将对应的值存储在val中
     * @return true 
     * @return false 
     */
    bool hasParams(const std::string& key, std::string* val = nullptr);
    
    /**
     * @brief 检测cookie中是否存在指定键名的键值对
     * 
     * @param key 指定键名
     * @param val 如果存在，则将对应的值存储在val中
     * @return true 
     * @return false 
     */
    bool hasCookies(const std::string& key, std::string* val = nullptr);

    // get系列函数中，如果结尾是As，则会尝试将获取到的值转换为指定类型
    // 如果没有结尾As，则直接返回获取到的值

    /**
     * @brief 从请求头中获取指定键名的值，并尝试将其转换为指定类型
     * 
     * @tparam T 目标类型
     * @param key 指定键名
     * @param def 如果键不存在或转换失败时返回的默认值
     * @return T 
     */
    template <typename T>
    T getHeaderAs(const std::string& key, const T& def = T()) {
        return GetAs(m_headers, key, def);
    }

    template <typename T>
    T getParamAs(const std::string& key, const T& def = T()) {
        return GetAs(m_params, key, def);
    }

    template <typename T>
    T getCookieAs(const std::string& key, const T& def = T()) {
        return GetAs(m_cookies, key, def);
    }

    /**
     * @brief 检测请求头中是否存在key对应的项，并尝试将其值转换为指定类型，然后将转换后的值存储在val中。
     * 
     * @tparam T 目标类型
     * @param key 指定键名
     * @param val 将转换后的值存储在val中
     * @param def 如果键不存在或转换失败时使用的默认值
     */
    template <typename T>
    bool checkAndGetHeaderAs(const std::string& key, T& val, const T& def = T()) {
        return CheckAndGetAs(m_headers, key, val, def);
    }

    template <typename T>
    bool checkAndGetParamAs(const std::string& key, T& val, const T& def = T()) {
        return CheckAndGetAs(m_params, key, val, def);
    }

    template <typename T>
    bool checkAndGetCookieAs(const std::string& key, T& val, const T& def = T()) {
        return CheckAndGetAs(m_cookies, key, val, def);
    }

    // 将HttpRequest对象的内容转储到输出流
    std::ostream& dump(std::ostream& os) const;

    // 将HttpRequest对象转换为字符串表示
    std::string toString() const;
private:
    // 请求方法
    HttpMethod m_method;
    // HTTP版本
    uint8_t m_version;
    // 是否主动关闭
    bool m_close;
    // 是否websocket
    bool m_websocket;
    // 请求路径
    std::string m_path;
    // 请求查询参数
    std::string m_query;
    // 请求的fragment
    std::string m_fragment;
    // 请求体
    std::string m_body;
    // 请求头
    MapType m_headers;
    // 请求参数
    MapType m_params;
    // 请求的cookie
    MapType m_cookies;

};

// HTTP响应的格式，第一行是响应行，第二行到空白行之前是响应头，空白行之后是响应体
/*
HTTP/1.1 200 OK
Date: Mon, 18 Oct 2024 10:36:20 GMT
Server: Apache/2.4.1 (Unix)
Last-Modified: Wed, 12 Oct 2024 14:36:27 GMT
Content-Type: text/html; charset=UTF-8
Content-Length: 1234
Connection: close
Set-Cookie: UserID=JohnDoe; expires=Wed, 19 Oct 2024 23:59:59 GMT; path=/
Cache-Control: max-age=3600, must-revalidate

<!DOCTYPE html>
<html lang="en">
<head>
    <title>Sample Web Page</title>
</head>
<body>
    <h1>Hello, World!</h1>
    <p>This is a sample web page.</p>
</body>
</html>

*/

class HttpResponse {
public:
    using ptr = std::shared_ptr<HttpResponse>;
    using MapType = std::map<std::string, std::string, CaseInsensitiveLess>;
    
    // 普通getter

    HttpResponse(uint8_t version = 0x11, bool close = true);
    HttpStatus getStatus() const { return m_status;}
    uint8_t getVersion() const { return m_version;}
    bool isClose() const { return m_close;}
    bool isWebsocket() const { return m_websocket;}
    const std::string& getBody() const { return m_body;}
    const std::string& getReason() const { return m_reason;}
    const MapType& getHeaders() const { return m_headers;}
    const std::vector<std::string>& getCookies() const { return m_cookies;}

    // 普通setter

    void setStatus(HttpStatus status) { m_status = status;}
    void setStatus(uint32_t status) { m_status = static_cast<HttpStatus>(status);}
    void setVersion(uint8_t version) { m_version = version;}
    void setClose(bool close) { m_close = close;}
    void setWebsocket(bool websocket) { m_websocket = websocket;}
    void setBody(const std::string& body) { m_body = body;}
    void setReason(const std::string& reason) { m_reason = reason;}
    void setHeaders(const MapType& headers) { m_headers = headers;}
    void setCookies(const std::vector<std::string>& cookies) { m_cookies = cookies;}

    // 返回请求头中指定键名的值
    const std::string& getHeaders(const std::string& key, const std::string& def = "") const;
    void setHeader(const std::string& key, const std::string& val);
    void delHeader(const std::string& key);
    bool hasHeader(const std::string& key, std::string* val = nullptr);

    HttpContentType getContentType() { return StringToContentType(getHeaders("Content-Type", ""));}
    Json getJson() const;

    void setContentType(HttpContentType contentType) { setHeader("Content-Type", HttpContentTypeToString(contentType));}
    void setContentType( const std::string& type ) { setHeader("Content-Type", type);}
    void setJson(const Json& json);

    // 返回请求头中指定键名的值，并尝试将其转换为指定类型
    template <typename T>
    T getHeaderAs(const std::string& key, const T& def = T()) {
        return GetAs(m_headers,key, def);
    }
    template <typename T>
    bool checkAndGetHeaderAs(const std::string& key, T& val, const T& def = T()) {
        return CheckAndGetAs(m_headers, key, val, def);
    }

    std::ostream& dump(std::ostream& os) const;
    std::string toString() const;
private:
    // 响应状态
    HttpStatus m_status;
    // 版本
    uint8_t m_version;
    // 是否自动关闭
    bool m_close;
    // 是否为websocket
    bool m_websocket;
    // 响应消息体
    std::string m_body;
    // 相应原因
    std::string m_reason;
    // 响应头
    MapType m_headers;
    // 响应cookie
    std::vector<std::string> m_cookies;
};


} // namespace RR::http
