//
// File created on: 2024/03/17
// Author: Zizhou

#include "socket.h"
#include <netinet/tcp.h> // Include the header file that defines TCP_NODELAY

#include <libgo/netio/unix/hook.h>
#include <libgo/netio/unix/hook_helper.h>

namespace RR {

static auto Logger = GetLoggerInstance();
// 构造函数和析构函数

Socket::Socket(int family, int type, int protocol) : m_socket(-1), m_family(family), m_type(type), m_protocol(protocol) {
    
}

Socket::~Socket() {
    close();
}

// 创建TCP/UDP套接字

Socket::ptr Socket::CreateTCP(Address::ptr address) {

}

Socket::ptr Socket::CreateUDP(Address::ptr address) {

}

// 不绑定地址

Socket::ptr Socket::CreateTCPSocket() {

}

Socket::ptr Socket::CreateUDPSocket() {

}

// 创建IPv6套接字

Socket::ptr Socket::CreateTCPSocket6() {

}

Socket::ptr Socket::CreateUDPSocket6() {

}

// 创建Unix套接字

Socket::ptr Socket::CreateUnixTCPSocket() {

}

Socket::ptr Socket::CreateUnixUDPSocket() {

}


// 发送和接收超时的获取和设置

void Socket::setSendTimeout(uint64_t time) {
    struct timeval tv = {int(time / 1000), int(time % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_SNDTIMEO,  &tv, sizeof(tv));
}

// Socket::getSendTimeout()，原项目中使用了libgo库提供的抽象来管理套接字的属性，这里我们不再使用libgo库，所以这里的实现需要重新设计

uint64_t Socket::getSendTimeout() {
    struct timeval tv;
    size_t optlen(sizeof(tv));
    getOption(SOL_SOCKET, SO_SNDTIMEO, &tv, &optlen);
    return static_cast<uint64_t>(tv.tv_sec) * 1000 + static_cast<uint64_t>(tv.tv_usec) / 1000;
}

void Socket::setRecvTimeout(uint64_t time) {
    struct timeval tv = {int(time / 1000), int(time % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

// Socket::getSendTimeout()，原项目中使用了libgo库提供的抽象来管理套接字的属性，这里我们不再使用libgo库，所以这里的实现需要重新设计

uint64_t Socket::getRecvTimeout() {
    struct timeval tv;
    size_t optlen(sizeof(tv));
    getOption(SOL_SOCKET, SO_RCVTIMEO, &tv, &optlen);
    return static_cast<uint64_t>(tv.tv_sec) * 1000 + static_cast<uint64_t>(tv.tv_usec) / 1000;
}

// 套接字选项设置和获取
bool Socket::getOption(int level, int option, void* result, size_t* len) {
    int res = getsockopt(m_socket, level, option, result, reinterpret_cast<socklen_t*>(len));
    if (res == -1) {
        SPDLOG_LOGGER_ERROR(Logger, "getOption socket={} level = {} option = {} errno = {} strerrno = {}",m_socket, level, option, errno, strerror(errno));
        return false;
    }
    return true;
}

bool Socket::setOption(int level, int option, void* resullt, size_t len) {
    int res = setsockopt(m_socket, level, option, resullt, static_cast<socklen_t>(len));
    if (res == -1) {
        SPDLOG_LOGGER_ERROR(Logger,"setOption socket = {} level ={} option = {} errno = {} strerrno = {}", m_socket, level, option, errno, strerror(errno));
        return false;
    }
    return true;
}


void Socket::initSocket() {
    // 启用套接字SO_REUSEADDR选项：允许套接字和在等待状态的套接字共享同一个端口。

    int val = 1;
    setOption(SOL_SOCKET, SO_REUSEADDR, &val);

    // 判断套接字的类型是否是面向连接的
    // 如果是TCP套接字，启用TCP_NODELAY选项：禁用Nagle算法，允许小数据包的发送，可以减少网络延迟。
    if (m_type == SOCK_STREAM) {
        setOption(IPPROTO_TCP, TCP_NODELAY, &val);
    }
}

void Socket::newSocket() {
    // 调用socket()系统调用创建套接字
    int m_socket = socket(m_family, m_type, m_protocol);
    if (m_socket == -1) {
        SPDLOG_LOGGER_ERROR(Logger, "newSocket failed, socket({} {} {}) errno = {} strerrno = {}",m_family, m_type, m_protocol, errno, strerror(errno));
    } else [[LIKELY]] {
        initSocket();
    }
}

// 对于Socket::init函数，原项目中使用了libgo库提供的抽象来管理套接字的属性，这里我们不再使用libgo库，所以这里的实现需要重新设计

bool Socket::init(int sock) {
    // 如果传入的套接字文件描述符为-1，则是非法的
    if (sock == -1) {
        return false;
    }

    // 检测socket是否是一个有效的套接字
    int optval;
    socklen_t optlen = sizeof(optval);
    if(getsockopt(sock, SOL_SOCKET, SO_TYPE, &optval, &optlen) == -1) {
        SPDLOG_LOGGER_ERROR(Logger, "init failed, sock = {}, errno = {}, strerrno = {}", sock, errno, strerror(errno));
        return false;
    }

    m_socket = sock;
    m_isConnected = true;
    initSocket();
    getRemoteAddress();
    getLocalAddress();
    return true;
}


Socket::ptr Socket::accept() {
    // 创建一个新的Socket对象，使用当前套接字的地址族、类型和协议
    ptr sock(std::make_shared<Socket>(m_family, m_type, m_protocol));
    
    // 调用系统的accept函数等待接受一个新的连接请求，如果成功，返回新的套接字文件描述符
    auto newSock = ::accept(m_socket, nullptr, nullptr);

    // 如果newSock小于0，说明accept失败
    if (newSock < 0) {
        SPDLOG_LOGGER_ERROR(Logger, "accept({} {} {}) errno = {} strerrno = {}", m_socket, nullptr, nullptr, errno, strerror(errno));
        return nullptr;
    }

    // 如果accept成功，初始化新的套接字对象
    if (sock->init(newSock)) {
        return sock;
    }
    return nullptr;
}

bool Socket::bind(const Address::ptr address) {
    if (!isValid()) [[UNLIKELY]] { // 如果套接字未创建或已关闭
        newSocket(); // 创建新套接字
        if(!isValid()) [[UNLIKELY]] { // 再次检查套接字是否有效
            return false; // 如果套接字无效，返回false
        }
    }

    if (m_family != address->getFamily()) {
        SPDLOG_LOGGER_ERROR(Logger, "bind sock.family{} address.family{} are not equal, address={}",m_family, address->getFamily(), address->toString());
        return false;
    }

    // 调用系统的bind函数绑定地址
    if(::bind(m_socket, address->getAddr(), address->getAddrLen())) {
        SPDLOG_LOGGER_ERROR(Logger,"bind address={} errno = {} strerrno = {}", address->toString(), errno, strerror(errno));
        return false;
    }

    // 绑定成功，更新本地地址信息
    getLocalAddress();
    return true;
}

bool Socket::connect(const Address::ptr address, uint64_t timeoutMs) {
    if (!isValid()) {
        newSocket();
        if (!isValid()) {
            return false;
        }
    }

    // 确保套接字的地址族与远程地址的地址族匹配
    if (m_family != address->getFamily()) {
        SPDLOG_LOGGER_ERROR(Logger, "connect sock.family{} address.family{} are not equal, address={}", m_family, address->getFamily(), address->toString());
        return false;
    }

    if (timeoutMs == std::numeric_limits<uint64_t>::max()) {
        if (::connect(m_socket, address->getAddr(), address->getAddrLen())) {
            close();
            return false;
        }
    } else { // 原项目中使用libgo库函数实现这里的逻辑，这里我们需要重新设计
        // 设置套接字为非阻塞模式
        int flags = fcntl(m_socket, F_GETFL, 0);
        fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);

        // 尝试连接
        int result = ::connect(m_socket, address->getAddr(), address->getAddrLen());
        if(result == 0) {
            // 如果连接成功，将套接字设置回阻塞模式
            fcntl(m_socket, F_SETFL, flags);
        } else if(errno == EINPROGRESS) {
            // 如果连接正在进行，使用 select 等待连接完成或超时
            fd_set set;
            FD_ZERO(&set);
            FD_SET(m_socket, &set);

            struct timeval timeout;
            timeout.tv_sec = timeoutMs / 1000;
            timeout.tv_usec = (timeoutMs % 1000) * 1000;

            result = select(m_socket + 1, NULL, &set, NULL, &timeout);
            if(result <= 0) {
                // select 调用失败或超时，关闭套接字并返回 false
                close();
                return false;
            }

            // 检查套接字是否有错误
            int error;
            socklen_t len = sizeof(error);
            if(getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
                // 如果有错误，关闭套接字并返回 false
                close();
                return false;
            }

            // 将套接字设置回阻塞模式
            fcntl(m_socket, F_SETFL, flags);
        } else {
            // 如果连接失败，关闭套接字并返回 false
            close();
            return false;
        }
    }


    // 连接建立成功，标记为已连接
    m_isConnected = true;
    // 更新本地地址
    getLocalAddress();
    // 更新远程地址
    getRemoteAddress();
    return true;
}

bool Socket::listen(int backLog = SOMAXCONN) {
    if (!isValid()) {
        // 这里不创建新的套接字，因为 listen 函数通常在套接字已经被创建并绑定到一个地址之后调用。
        // 如果在这个时候套接字无效，那么尝试创建一个新的套接字并不会有任何帮助
        // 因为新的套接字并没有被绑定到正确的地址。
        SPDLOG_LOGGER_ERROR(Logger, "listen invalid socket");
        return false;
    }

    // 调用系统的listen函数使套接字进入监听状态
    if (::listen(m_socket, backLog)) {
        SPDLOG_LOGGER_ERROR(Logger, "listen error, errno={} strerror={}", errno, strerror(errno));
        return false;
    }
    return true;
}

bool Socket::close() {
    // 如果套接字已关闭且无效，直接返回
    if (!m_isConnected && m_socket == -1) {
        return true;
    }

    m_isConnected = false;
    // 如果套接字文件描述符有效，调用系统的close函数关闭套接字
    if (m_socket != -1) {
        ::close(m_socket);
        // 将文件描述符设置为无效
        m_socket = -1;
    }
    return true;
}

Address::ptr Address::getRemoteAddress() {

}

Address::ptr Address::getLocalAddress() {

}

}