//
// File created on: 2024/03/14
// Author: Zizhou

#ifndef RR_ADDRESS_H
#define RR_ADDRESS_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <iostream>
#include <sstream>
#include <sys/socket.h> // Include the header file that defines the 'sockaddr' type
#include <netinet/in.h>
#include <sys/un.h>
#include <spdlog/spdlog.h>
#include <ifaddrs.h>
#include <arpa/inet.h> // Include the header file that declares the inet_pton function

#include "RaftRegistry/common/util.h"

namespace RR {

/**
 * @brief 主机序转换为网络序
 * 
 * @tparam T 待转换数据的类型
 * @param value 待转换的数据
 * @return constexpr T 返回类型为传入的数据类型
 */
template <typename T>
constexpr T HostToNetwork(T value) {
    return EndianCast(value);
}

/**
 * @brief 网络序转换为主机序
 */
template <typename T>
constexpr T NetworkToHost(T value) {
    return EndianCast(value);
}

/**
 * @brief 创建掩码
 * 
 * @tparam T 表示掩码的数据类型
 * @param bit 掩码的位数
 * @return T 创建的掩码
 * 
 * @details 掩码为几位，则低位为1，高bit位为0。
 *          这里创建的不是网络序的掩码（即子网掩码），而是主机序的掩码。
 *          子网掩码会使用专门的函数对此函数的结果进行转换得到。
 * @example CreateMask<uint32_t>(8) 返回 0x00ffffff（这里指的是大端序）
 */
template <typename T>
static T CreateMask(uint32_t bit) {
    return (1 << sizeof(T) * 8 -bit) -1;
}

/**
 * @brief 计算输入的值中位为1的个数
 */
template <typename T>
static uint32_t CountBytes(T value) {
    int count = 0;
    for (int i = 0;i< value;++i) {
        // value - 1 会将最低位的1变为0，
        // 然后与原值进行与运算，与运算的结果是从刚才的最低位开始一直到二进制末尾都是0，最低位之前的保持原样
        // 将与运算的结果赋给value，这样每次value都会减少一个1
        value &= value - 1;
    }
}

class IpAddress;


class Address {
public:
    using ptr = std::shared_ptr<Address>;

    /**
     * 根据sockaddr结构创建Address对象
     * 
     * @param addr sockaddr结构指针，包含地址信息
     * @param addrlen sockaddr结构的长度
     * @return 创建的Address对象智能指针
     */

    static Address::ptr Create(const sockaddr*, socklen_t addrlen);

    /**
     * @brief 通过host地址返回对应条件的所有Address，即通过域名或者服务器名返回对应的地址
     * @param[out] result 保存满足条件的所有Address
     * @param[in] host 域名,服务器名等.举例: www.google.com[:80] (方括号为可选内容)
     * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
     * @param[in] type 套接字类型SOCK_STREAM、SOCK_DGRAM 等
     * @param[in] protocol 协议,IPPROTO_TCP、IPPROTO_UDP 等
     * @return 返回是否转换成功
     */
    static bool LookUp(std::vector<Address::ptr>& result, const std::string& host, int family = AF_INET, int type = 0, int protocol = 0);
    
    /**
     * @brief 通过host地址返回对应条件的任意一个Address
     * @return 返回满足条件的任意一个Address,失败返回nullptr
     */
    static Address::ptr LookUpAny(const std::string& host, int family = AF_INET, int type=0,int protocol = 0);

    /**
     * @brief 通过host地址返回对应条件的任意IPAddress
     * @return 返回满足条件的任意IPAddress,失败返回nullptr
     */
    static std::shared_ptr<IpAddress> LookUpAnyIpAddress(const std::string& host, int family = AF_INET, int type = 0, int protocol = 0);

    /**
     * @brief 返回本机所有网卡中符合指定family的<网卡名, 地址, 子网掩码位数>
     * @param[out] result 保存本机所有地址
     * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
     * @return 是否获取成功
     */
    static bool GetInterfaceAddresses(std::multimap<std::string,std::pair<Address::ptr, uint32_t>>& result, int family = AF_INET);

    /**
     * @brief 获取指定网卡的地址和子网掩码位数<地址，子网掩码数>
     * @param[out] result 保存指定网卡所有地址
     * @param[in] interfaceName 网卡名称
     * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
     * @return 是否获取成功
     */
    static bool GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t>>& result, const std::string& interfaceName, int family = AF_INET);
    
    virtual ~Address() {};

    /**
     * @brief 获取地址族
     */
    int getFamily() const;

    virtual const sockaddr* getAddr() const = 0;
    virtual sockaddr*  getAddr() = 0;
    virtual const socklen_t getAddrLen() const = 0;

    // 将地址信息插入到输出流
    virtual std::ostream& insert(std::ostream& out) = 0;

    // 将地址信息转换为字符串
    std::string toString();

    // 重载运算符用于地址比较
    bool operator<(const Address& rhs) const;
    bool operator==(const Address& rhs) const;
    bool operator!=(const Address& rhs) const;

};

class IpAddress : public Address {
public:
    using ptr = std::shared_ptr<IpAddress>;

    /**
     * @brief 根据主机号和端口号创建IpAddress对象
     * 
     * @param hostName 主机号
     * @param port 端口号
     * @return IpAddress::ptr 创建的IpAddress对象的智能指针
     */
    static IpAddress::ptr Create(const char* hostName, uint16_t port = 0);

    virtual IpAddress::ptr broadcastAddress(uint32_t prefixLen) = 0;
    virtual IpAddress::ptr networkAddres(uint32_t prefixLen) = 0;
    virtual IpAddress::ptr subnetMask(uint32_t prefixLen) = 0;

    /**
     * @brief 获取端口号
     * 
     * @return uint32_t 端口号
     */
    virtual uint32_t getPort() const = 0;

    /**
     * @brief 设置端口号
     * 
     * @param port 端口号
     */
    virtual void setPort(uint16_t port) = 0;
};

class Ipv4Address : public IpAddress {
public:
    using ptr = std::shared_ptr<Ipv4Address>;

    static Ipv4Address::ptr Create(const char* ip, uint16_t port = 0);
    
    Ipv4Address();
    Ipv4Address(const sockaddr_in& address) ;
    Ipv4Address(uint32_t ip, uint16_t port = 0);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    const socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& out) override;

    IpAddress::ptr broadcastAddress(uint32_t prefixLen) override;
    IpAddress::ptr networkAddres(uint32_t prefixLen) override;
    IpAddress::ptr subnetMask(uint32_t prefixLen) override;

    uint32_t getPort() const override;
    void setPort(uint16_t port) override;

private:
    sockaddr_in m_addr;
};

class Ipv6Address : public IpAddress {
public:
    using ptr = std::shared_ptr<Ipv6Address>;

    static Ipv6Address::ptr Create(const char* ip, uint16_t port = 0);

    Ipv6Address();
    Ipv6Address(const sockaddr_in6& address);
    Ipv6Address(const uint8_t ip[16], uint16_t port = 0);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    const socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& out) override;

    IpAddress::ptr broadcastAddress(uint32_t prefixLen) override;
    IpAddress::ptr networkAddres(uint32_t prefixLen) override;
    IpAddress::ptr subnetMask(uint32_t prefixLen) override;

    uint32_t getPort() const override;
    void setPort(uint16_t port) override;
private:
    sockaddr_in6 m_addr;
};

// Unix域套接字是一种提供本地（非网络）进程间通信的方式
// 其地址是通过文件系统路径或抽象命名空间确定的。

class UnixAddress : public Address {
public:
    using ptr = std::shared_ptr<UnixAddress>;
    UnixAddress();
    UnixAddress(const std::string& path);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    const socklen_t getAddrLen() const override;
    void setAddrLen(uint32_t len);
    std::ostream& insert(std::ostream& out) override;

private:
    sockaddr_un m_addr;

    // 给unix域套接字地址结构体添加长度成员
    // 原因：因为 Unix 域套接字（AF_UNIX）的地址长度不是固定的，虽然最大值是确定的（最大108），但是具体地址需要取决于套接字路径的长度。
    // 而ipv4和ipv6的地址长度是固定的，所以不需要添加长度成员
    socklen_t m_length;
};

class UnknownAddress : public Address {
public:
    using ptr = std::shared_ptr<UnknownAddress>;
    UnknownAddress(int family);
    UnknownAddress(const sockaddr& addr);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    const socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& out) override;

private:
    sockaddr m_addr;
};

}
#endif // RR_ADDRESS_H