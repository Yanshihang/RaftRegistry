//
// File created on: 2024/03/16
// Author: Zizhou

#include "uri.h"

namespace RR {

Uri::Uri() : m_port(0), m_cur(nullptr) {

}

Uri::ptr Uri::Create(const std::string& uri) {
    // 如果uri为空，则直接返回空指针
    if ( uri.empty()) {
        return nullptr;
    }

    // 创建Uri对象，并开始解析uri
    Uri::ptr res(new Uri);

    // 调用parse成员协程开始解析URI
    Task<bool> parse_task = res->parse();

    // 遍历uri中的每个字符
    for (size_t i =0;i < uri.size();++i) {
        res->m_cur = &uri[i];
        parse_task.resume();
        // 一旦循环中某一次解析失败则返回nullptr
        if (!parse_task.get()) {
            return nullptr;
        }
        
    }

    // 整个字符串遍历结束，将m_cur置为空指针，然后继续进行最后一次解析
    res->m_cur = nullptr;
    parse_task.resume();

    // 检查解析结果，如果解析失败则返回nullptr
    if (!parse_task.get()) {
        return nullptr;
    }
    return res;
}

Address::ptr Uri::createAddress() {
    IpAddress::ptr addr = Address::LookUpAnyIpAddress(m_host);
    if (addr) {
        addr->setPort(getPort());
    }
    return addr;
}

const std::string& Uri::getScheme() const {
    return m_scheme;
}

const std::string& Uri::getUserinfo() const {
    return m_userinfo;
}

const std::string& Uri::getHost() const {
    return m_host;
}

// 返回Uri的路径，特殊处理了magnet协议和默认路径
const std::string& Uri::getPath() const {
    // 如果是其他协议，路径为空则返回默认路径
    // 如果是magnet协议，即便路径为空也返回（而不是返回默认路径）

    static std::string default_path = "/";
    if (m_scheme == "magnet") {
        return m_path;
    }
    return m_path.empty() ? default_path : m_path;
}

const std::string& Uri::getQuery() const {
    return m_query;
}

const std::string& Uri::getFragment() const {
    return m_fragment;
}

uint32_t Uri::getPort() const {
    // 如果已经设置了端口号，则直接返回
    if (m_port) return m_port;

    // 根据不同的协议类型返回默认端口号
    if (m_scheme == "http" || m_scheme == "ws") {
        return 80;
    } else if (m_scheme == "https" || m_scheme == "wss") {
        return 443;
    }
    return m_port;
    
}

void Uri::setScheme(const std::string& scheme) {
    m_scheme = scheme;
}

void Uri::setUserinfo(const std::string& userinfo) {
    m_userinfo = userinfo;
}

void Uri::setHost(const std::string& host) {
    m_host = host;
}

void Uri::setPath(const std::string& path) {
    m_path = path;
}

void Uri::setQuery(const std::string& query) {
    m_query = query;
}

void Uri::setFragment(const std::string& fragment) {
    m_fragment = fragment;
}

std::ostream& Uri::dump(std::ostream& os) const {
    // 如果m_scheme为空，则直接进行下面的输出操作，如果不为空则按照协议格式输出
    if (!m_scheme.empty()) {
        os << m_scheme << ":";
        if (m_scheme == "magnet") {
            
        } else {
            os << "//";
        }
    }
    os << m_userinfo << (m_userinfo.empty() ? "" :"@") << m_host << (isDefaultPort() ? "" : std::to_string(m_port)) << getPath() << (m_query.empty()? "" : "?") << m_query << (m_fragment.empty() ? "" : "#") << m_fragment;
    return os;
}

std::string Uri::toString() {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

static inline bool IsValid(const char* p) {
    char c = *p;

    switch (c) {
        case 'a'...'z':
        case 'A'...'Z':
        case '0'...'9':
        case '-':
        case '_':
        case '.':
        case '~':
        case '!':
        case '*':
        case '\'':
        case '(':
        case ')':
        case ';':
        case ':':
        case '@':
        case '&':
        case '=':
        case '+':
        case '$':
        case ',':
        case '/':
        case '?':
        case '#':
        case '[':
        case ']':
        case '%':
            return true;
        default:
            return false;
    }
}

#ifdef yield
// 如果 yield 已经定义，将导致编译错误，并显示消息 "yield already defined"
#error "yield already defined"
#endif

// 定义一个协程挂起宏，用于挂起协程，并且检查协程恢复后的指针是否为空，此时指针已经指向挂起之前的下一个字符
// 只要m_cur不为空，就证明解析还没有结束，继续解析；如果为空则证明字符串不合法，解析失败
#define yield           \
co_yield true;          \
if(!m_cur) {            \
    co_return false;    \
}

Task<bool> Uri::parse() {
    size_t port_idx = 0;
    std::string buffer;

    // 解析协议部分，如果是协议中的特殊字符：:?/#或者不是合法字符则返回false；如果是合法字符则存入buffer后继续解析
    while(*m_cur != ':' && *m_cur != '?' && *m_cur != '/' && *m_cur != '#' && IsValid(m_cur)) {
        buffer.push_back(*m_cur);

        // 挂起当前协程,并返回true；协程恢复后，m_cur指向下一个字符
        co_yield true;

        // 如果当前解析位置指针为空，证明整个uri字符串没有任何特殊字符和不合法字符
        // 只能是全是host部分
        if (!m_cur) {
            m_host = std::move(buffer);
            co_return true;
        }
    }

    // 如果没有触发while循环中的co_return证明是字符串没有完全解析完成就结束了
    // 那一定是有特殊字符了，或者不合法字符
    // 此时应该根据当前位置的特殊字符进行判断和进一步的解析

    // 先判断当前位置的字符是否是host后的/
    if (*m_cur == '/') {
        m_host = std::move(buffer);
        goto parse_path;
    }

    yield; // 挂起当前携程，恢复后m_cur指向下一个字符

    // 如果当前字符是数字，证明刚才的字符是:，那么接下来的字符应该是端口号，前面的是host
    if (isdigit(*m_cur)) {
        m_host = std::move(buffer);
        goto parse_port;
    }

    // 如果当前字符不是数字，证明刚才的只能是scheme
    m_scheme = std::move(buffer);

    // 解析字符："//"
    if (*m_cur == '/') {
        yield;
        if (*m_cur != '/') { // 如果不是连续的两个/，则解析失败
            co_return false;
        }
        yield;

        // 如果不是连续的三个/，则下面部分内容是authority部分
        if (*m_cur != '/') {
            while(*m_cur != '@' && *m_cur != '/' && IsValid(m_cur)) {
                buffer.push_back(*m_cur);
                if (*m_cur == ':' && !port_idx) {
                    port_idx = buffer.size();
                }
                co_yield true;
                if (!m_cur) {
                    // 如果字符串解析完了，则检测一下刚才解析的内容是否包含port
                    // 如果包含port则将host和port分开
                    // 如果不包含port则直接将host赋值
                    if (port_idx) {
                        m_host = buffer.substr(0,port_idx-1);
                        try {
                            m_port == std::stoul(buffer.substr(port_idx));
                        } catch (...) {
                            co_return false;
                        }
                    } else {
                    m_host = std::move(buffer);
                    }
                    co_return true;
                }
            }

            // 如果遇到@、/之前没有其它字符，或者在autority中遇到不合法字符，则解析失败
            if (buffer.empty() || !IsValid(m_cur)) {
                co_return false;
            }

            // 如果是遇到的@导致while退出，则解析userinfo
            if (*m_cur == '@') {
                // 将port_idex置为0，因为userinfo部分不可能包含port
                port_idx = 0;
                m_userinfo = std::move(buffer);

                // 解析userinfo后，继续解析host
                yield;

                while(*m_cur != ':' && *m_cur != '/' && IsValid(m_cur)) {
                    buffer.push_back(*m_cur);
                    co_yield true;
                    if (!m_cur) {
                        m_host = std::move(buffer);
                        co_return true;
                    }
                }

                // 如果遇到:、/之前没有其它字符，则解析失败；或者解析host时遇到不合法字符则解析失败
                if (buffer.empty() || !IsValid(m_cur)) {
                    co_return false;
                }

                // 解析host
                m_host = std::move((buffer));

                // 如果是遇到了:，则解析port
                if (*m_cur == ':') {
                    yield;
                    parse_port:
                    while(isdigit(*m_cur)) {
                        buffer.push_back(*m_cur);
                        co_yield true;
                        if (!m_cur) {
                            try {
                                m_port = std::stoul(buffer);
                                co_return true;
                            } catch (...) {
                                co_return false;
                            }
                        }
                    }

                    // 只要:之后没有数字，则解析失败
                    // 如果:之后有数字，但是解析到的第一个非数字字符不是/，则解析失败
                    if (*m_cur != '/' || buffer.empty()) {
                        co_return false;
                    }
                    try {
                        m_port = std::stoul(buffer);
                    } catch (...) {
                        co_return false;
                    }
                    buffer.clear();
                }
            } else { // 如果是遇到的/导致while退出，则解析host
                if (port_idx) { // 如果port_idx不为0，则证明刚才解析的内容包含port
                    m_host = buffer.substr(0,port_idx-1);
                    try {
                        m_port = std::stoul(buffer.substr(port_idx));
                    } catch(...) {
                        co_return false;
                    }
                    buffer.clear();
                } else { // 如果port_idx为0，则证明刚才解析的内容不包含port
                    m_host = std::move(buffer);
                }
            }
                
            
        }
        // 如果//后是/，则证明是本地地址，开始解析
        if (*m_cur == '/') {
            parse_path:
            buffer.push_back('/');
            co_yield true;
            if (!m_cur) {
                m_path = std::move(buffer);
                co_return true;
            }
            // 如果遇到?或者#，则证明是路径部分的结束
            while (*m_cur!='?' && *m_cur != '#' && IsValid(m_cur)) {
                buffer.push_back(*m_cur);
                co_yield true;
                if (!m_cur) {
                    m_path = std::move(buffer);
                    co_return true;
                }

                // 如果解析path时遇到不合法字符，则解析失败
                if (!IsValid(m_cur)) {
                    co_return false;
                }
            }

            // 如果遇到?或者#，则证明是路径部分的结束
            // 或者解析path时，在/后的下一个字符就是不合法字符，则根路径是path
            m_path = std::move(buffer);
        }

    }


    // 解析query
    if (*m_cur == '?') {
        yield;
        while(*m_cur != '#' && IsValid(m_cur)) {
            buffer.push_back(*m_cur);
            co_yield true;
            if (!m_cur) {
                m_query = std::move(buffer);
                co_return true;
            }

            // 在解析query时遇到不合法字符，则解析失败
            if (!IsValid(m_cur)) {
                co_return false;
            }
        }

        // 如果遇到#，则证明是query部分的结束
        // 或者解析query时，在?后的下一个字符就是不合法字符，则query为空
        m_query = std::move(buffer);
    }

    // 解析fragment
    if (*m_cur == '#') {
        yield;
        while(IsValid(m_cur)) {
            buffer.push_back(*m_cur);
            co_yield true;
            if (!m_cur) {
                m_fragment = std::move(buffer);
                co_return true;
            }

            // 在解析fragment时遇到不合法字符，则解析失败
            if (!IsValid(m_cur)) {
                co_return false;
            }
        }

        // 如果解析fragment时，在#后的下一个字符就是不合法字符，则fragment为空
        m_fragment = std::move(buffer);
    }

    // 如果某个字符一直没有触发co_return，则证明解析失败
    co_return false;
}

#undef yield

} // namespace RR