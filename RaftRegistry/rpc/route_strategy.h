//
// File created on: 2024/03/22
// Author: Zizhou

#ifndef RAFTREGISTRY_RPC_ROUTE_STRATEGY_H
#define RAFTREGISTRY_RPC_ROUTE_STRATEGY_H

#include <vector>
#include <string>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h> // Include the header file for ioctl function
#include <cstring> // Include the necessary header file for strcmp


namespace RR::rpc {

// 定义负载均衡策略的枚举类型
enum class Strategy {
    Random, // 随机路由策略
    Polling, // 轮询路由策略
    HashIp, // 源地址hash算法
};

/**
 * @brief 路由策略接口
 * 
 * @tparam T 负载均衡策略枚举类型
 * 
 * @details 通用类型负载均衡路由引擎（含随机、轮询、源地址hash等策略），由客户端使用，
 *          在客户端实现具体负载均衡。
 */
template <typename T>
class RouteStrategy {
public:
    using ptr = std::shared_ptr<RouteStrategy>;
    virtual ~RouteStrategy() {}

    /**
     * @brief 负载均衡策略的算法
     * 
     * @param list 原始列表
     * @return T& 负载均衡策略选择的结果
     */
    virtual T& select(std::vector<T>& list) = 0
};

namespace impl {

/**
 * @brief 随机策略的实现
 * 
 * @tparam T 
 */
template <typename T>
class RandomRouteStrategyImpl : public RouteStrategy<T> {
public:
    T& select(std::vector<T>& list) override {
        // 初始化随机数种子
        std::srand(static_cast<unsigned>(time(nullptr)));
        return list[rand()% list.size()];
    }

};

template <typename T>
class PollingRouteStrategyImpl : public RouteStrategy<T> {
public:
    T& select(std::vector<T>& list) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_index >=static_cast<int>(list.size())) {
            m_index = 0;
        }
        return list[m_index++];
    }

private:
    // 轮询索引
    int m_index = 0;
    // 互斥锁
    std::mutex m_mutex;
};

static std::string GetLocalHost();

template <typename T>
class HashIpRouteStrategyImpl : public RouteStrategy<T> {
public:
    T& select(<std::vector<T>& list) override {
        static std::string localHost = GetLocalHost();

        if (localHost.empty()) {
            return RandomRouteStrategyImpl{}.select(list);
        }

        size_t hashCode = std::hash<std::string>()(localHost);
        return list[hashCode%(list.size())];
    }
};

/**
 * @brief 获取本地主机IP地址
 * 
 * @return std::string 
 */
std::string GetLocalHost() {

    int sockfd; // 套接字文件描述符
    struct ifconf ifc; // 网卡接口配置结构体
    struct ifreq *ifr = nullptr; // 网卡接口请求结构体
    char buf[512]; // 缓冲区,存储网卡接口信息

    // 初始化ifconf结构体
    ifc.ifc_len = 512;
    ifc.ifc_buf = buf;

    // 创建套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return std::string{}; // 创建套接字失败
    }

    ioctl(sockfd, SIOCGIFCONF, &ifc); // 获取所有接口信息

    ifr = reinterpret_cast<struct ifreq*>(ifc.ifc_buf);
    // 遍历网络接口信息
    for (int i = (ifc.ifc_len / sizeof(struct ifreq)); i> 0;--i) {
        if (ifr->ifr_flags == AF_INET) { // 只处理IPv4地址，这里看似是访问ifr_flags，实际上是访问ifr_addr的port，即sockaddr_in的sin_port
            if (ifr->ifr_name == std::string("eth0")) { // 检查是否是"eth0"网络接口（一般为主网卡接口）
                // 将网络地址转换为字符串形式
                return std::string(inet_ntoa(reinterpret_cast<struct sockaddr_in*>(&(ifr->ifr_addr))->sin_addr));
            }
            ifr++; // 指向下一个网络接口
        }
    }
    return std::string{};
}

} // namespace impl

/**
 * @brief: 路由均衡引擎
 */
template <typename T>
class RouteEngine {
public:
    /**
     * @brief 查询指定的路由策略实例
     * 
     * @param RouteStrategy 策略枚举值
     * @return RouteStrategy<T>::ptr 
     */
    static typename RouteStrategy<T>::ptr queryStrategy(strategy RouteStrategy) {
        switch (RouteStrategy) {
            case Strategy::Random:
                return s_randomRouteStrategy;
            case Strategy::Polling:
                return std::make_shared<impl::PollingRouteStrategyImpl<T>>();
            case Strategy::HashIp:
                return s_hashIpRouteStrategy;
            default:
                return s_randomRouteStrategy;
        }
    }

private:
    static typename RouteStrategy<T>::ptr s_randomRouteStrategy;
    static typename RouteStrategy<T>::ptr s_hashIpRouteStrategy;
};

// 静态成员的初始化

template <typename T>
typename RouteStrategy<T>::ptr RouteEngine<T>::s_randomRouteStrategy = std::make_shared<impl::RandomRouteStrategyImpl<T>>();

template <typename T>
typename RouteStrategy<T>::ptr RouteEngine<T>::s_hashIpRouteStrategy = std::make_shared<impl::HashIpRouteStrategyImpl<T>>();


}


#endif // RAFTREGISTRY_RPC_ROUTE_STRATEGY_H