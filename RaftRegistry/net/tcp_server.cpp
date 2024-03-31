//
// File created on: 2024/03/18
// Author: Zizhou

#include "RaftRegistry/common/util.h"
#include "RaftRegistry/common/config.h"
#include "RaftRegistry/net/tcp_server.h"

namespace RR {
// 获取日志实例，用于记录日志信息
static auto Logger = GetLoggerInstance();

static ConfigVar<uint64_t>::ptr g_tcp_server_recv_timeout = Config::LookUp<uint64_t>("tcp_server.recv_timeout", static_cast<uint64_t>(60 * 1000 * 2), "tcp server recv timeout"); // 查找配置项，设置默认接收超时时间

TcpServer::TcpServer() : m_timer(), m_recvTimeout(g_tcp_server_recv_timeout->getValue()), m_name("RR/1.0.0"), m_stop(true) {}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::bind(Address::ptr addr) {
    // addrs用于保存要bind的address，fail用于保存bind失败的address
    std::vector<Address::ptr> addrs, fail;
    addrs.push_back(addr);
    return bind(addrs, fail);
}

bool TcpServer::bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fail) {
    // 遍历地址列表进行绑定
    for (auto addr : addrs) {
        // 创建一个套接字
        Socket::ptr socket = Socket::CreateTCP(addr);
        // 绑定套接字到地址
        if (!socket->bind(addr)) {
            // 如果绑定失败，记录失败的地址
            SPDLOG_LOGGER_ERROR(Logger, "bind fail errno = {} errstr = {} addr = {}", errno, strerror(errno), addr->toString());
            fail.push_back(addr);
        }
        // 绑定成功后，进行监听
        if (!socket->listen()) {
            // 如果监听失败，记录失败的地址
            SPDLOG_LOGGER_ERROR(Logger, "listen fail errno = {} errstr = {} addr = {}", errno, strerror(errno), addr->toString());
            fail.push_back(addr);
        }
        m_listens.push_back(socket);
    }

    // 如果有绑定失败的地址，清空监听列表，返回false
    if (!fail.empty()) {
        m_listens.clear();
        return false;
    }

    for ( auto& i : m_listens) {
        SPDLOG_LOGGER_INFO(Logger, "server {} bind {} success", m_name, i->toString());
    }
    return true;
}

void TcpServer::start() {
    if (!isStop()) {
        // 如果服务器已经启动，直接返回。false表示服务器已经启动
        return;
    }
    m_stop = false;
    m_stopCh = co::co_chan<bool>(); // 初始化协程通道
    for ( auto& i : m_listens) {
        go [i, this] {
            this->startAccept(i); // 为每个监听套接字创建一个协程来接受连接
        };
    }
    m_stopCh >> nullptr; // 阻塞，等待停止信号
}

void TcpServer::stop() {
    if (isStop()) {
        return;
    }
    m_stop = true;
    for ( auto& i : m_listens) {
        i->close(); // 关闭所有监听套接字
    }
    m_stopCh.Close(); // 关闭协程通道
}

bool TcpServer::isStop() const {
    return m_stop;
}

uint64_t TcpServer::getRecvTimeout() const {
    return m_recvTimeout;
}

void TcpServer::setRecvTimeout(uint64_t timeout) {
    m_recvTimeout = timeout;
}

std::string TcpServer::getName() const {
    return m_name;
}

void TcpServer::setName(const std::string& name) {
    m_name = name;
}

void TcpServer::handleClient(Socket::ptr client) {
    // 默认实现仅打印客户端信息，用户应该在子类中重写此方法来实现具体的处理逻辑
    SPDLOG_LOGGER_INFO(Logger, "handleClient: {}", client->toString());
}

void TcpServer::startAccept(Socket::ptr sock) {
    while(!isStop()) { // 只要服务器没停止，就一直循环接收连接
        Socket::ptr client = sock->accept(); // 接受客户端的连接
        if (client) {
            client->setRecvTimeout(m_recvTimeout); // 设置接收超时时间
            go [client, this] {
                this->handleClient(client); // 为每个客户端连接创建一个协程来处理
            };
        }else {
            if (!sock->isConnected()) {
                return; // 如果套接字已关闭，则退出循环
            }
        }
    }
}

}