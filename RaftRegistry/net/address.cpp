//
// File created on: 2024/03/14
// Author: Zizhou

#include "address.h"

#include <netdb.h>

namespace RR {

static auto Logger = GetLoggerInstance();

// Address类的实现
Address::ptr Create(const sockaddr* addr, socklen_t addrLen) {
    if (!addr) {
        return nullptr;
    }

    Address::ptr result;
    // 判断addr的地址族，然后转换为对应的套接字类型
    switch(addr->sa_family) {
        case AF_INET:
            result.reset(new Ipv4Address(*(reinterpret_cast<const sockaddr_in*>(addr))));
            break;
        case AF_INET6:
            result.reset(new Ipv6Address(*(reinterpret_cast<const sockaddr_in6*>(addr))));
            break;
        default:
        result.reset(new UnknownAddress(*addr));
    }
    return result;
}

Address::ptr Address::LookUpAny(const std::string& host, int family, int type, int protocol) {
    // 使用result存储LookUp中满足条件的所有Address
    std::vector<Address::ptr> result;
    if (LookUp(result, host, family, type, protocol)) {
        return result[0];
    }
    return nullptr;
}

IpAddress::ptr Address::LookUpAnyIpAddress(const std::string& host, int family, int type, int protocol) {
    // 使用result存储LookUp中满足条件的Address
    std::vector<Address::ptr> result;
    if (LookUp(result, host, family, type, protocol)) {
        for (const auto& i : result) {
            IpAddress::ptr v = std::dynamic_pointer_cast<IpAddress>(i);
            if (v) {
                return v;
            }
        }
    }
    return nullptr;
}

bool LookUp(std::vector<Address::ptr>& result, const std::string& host, int family = AF_INET, int type = 0, int protocol = 0) {
    addrinfo hints, *results = nullptr, *next = nullptr;
    hints.ai_flags = 0;
    hints.ai_family = family;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;
    hints.ai_addrlen = 0;
    hints.ai_canonname = nullptr;
    hints.ai_next = nullptr;
    hints.ai_addr = nullptr;

    std::string node; // 保存ip地址
    const char* service = nullptr; // 保存端口号起始地址

    // 判断host是否为ipv6地址，如果是则将ipv6地址保存入node，端口号保存入service
    if (!host.empty() && host[0] == '[') {
        // 通过查找']'来确定为ipv6地址的范围
        auto endipv6 = static_cast<const char*>(memchr(host.c_str(),']',host.size()));
        if (endipv6) {
            if (*(endipv6+1)==':') {
                service = endipv6+2;
            }
            // 将ipv6地址保存入node
            node = host.substr(1, endipv6 - host.c_str()-1);
        }
    }

    // 判断node是否为空，如果为空则证明不是ipv6
    // 那么判断是否为ipv4地址，如果是则将ipv4地址保存入node，端口号保存入service
    if (node.empty()) {
        service = static_cast<const char*>(memchr(host.c_str(),':',host.size()));
        if (service) {
            if (!memchr(service+1,':',host.c_str()+host.size()-service-1)) {
                node = host.substr((0, service - host.c_str()));
                ++service;
            }
        }
    }

    // 如果既不是ipv6地址也不是ipv4地址，则将host保存入node
    if (node.empty()) {
        node = host;
    }
    // 调用getaddrinfo函数获取地址信息
    // 返回0则代表成功，非0代表失败
    int error = getaddrinfo(node.c_str(),service,&hints,&results);
    if (error) {
        SPDLOG_LOGGER_DEBUG(Logger,"Address::Lookup getaddress({},{},{}) err={} errstr={}",host,family,type,error,gai_strerror(error));
        return false;
    }

    // 将results中的地址信息保存入result
    next = results;
    while(next) {
        result.push_back(Create(next->ai_addr, next->ai_addrlen));
        next = next->ai_next;
    }
    
    // 释放results
    freeaddrinfo(results);
    return !result.empty();
}


bool Address::GetInterfaceAddresses(std::multimap<std::string,std::pair<Address::ptr, uint32_t>>& result, int family) {
    struct ifaddrs *next=nullptr, *results=nullptr; // 定义指向ifaddrs结构的指针，用于遍历和存储getifaddrs函数返回的网络接口地址列表
    int err = getifaddrs(&results); // 调用getifaddrs函数获取网络接口地址列表，并将结果存储在results中
    if (err) { // 检查getifaddrs调用是否成功，若err=0则成功，否则失败
        SPDLOG_LOGGER_DEBUG(Logger,"Address::GetInterfaceAddresses getifaddrs err={} errstr={}",err,gai_strerror(err));
        return false;
    }

    try { // 使用try-catch语句捕获异常，便于在处理地址时捕获异常
        // 遍历网络接口地址列表，每次循环先将筛选出来的地址存入addr，然后再将addr存入result
        for (next = results;next;next = next->ifa_next) {
            Address::ptr addr;
            uint32_t prefix_len = ~0u;
            // 传入的family参数是用于筛选地址族的，如果family不是未指定的地址族，也不是获得的网络接口地址的地址族，则跳过
            if (family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
                continue;
            }
            switch (next->ifa_addr->sa_family) {
                case AF_INET: 
                    addr = Create(next->ifa_addr, sizeof(sockaddr_in));
                    uint32_t netmast = reinterpret_cast<sockaddr_in*>(next->ifa_netmask)->sin_addr.s_addr ;
                    prefix_len = CountBytes(netmast);
                    break;
                case AF_INET6:
                    addr = Create(next->ifa_addr, sizeof(sockaddr_in6));
                    // 这里的处理与ipv4不同，不直接获取子网掩码，而是获取包含子网掩码的结构体，然后再获取子网掩码
                    in6_addr netmast6 = reinterpret_cast<sockaddr_in6*>(next->ifa_netmask)->sin6_addr;
                    prefix_len = 0;
                    for (int i=0;i < 16;++i) {
                        prefix_len += CountBytes(netmast6.s6_addr[i]);
                    }
                    break;
                default:
                break;
            }

            // 如果addr不为空，则将addr存入result
            if (addr) {
                result.emplace(next->ifa_name,std::make_pair(addr, prefix_len));
            }
        }
    }catch (...) { // catch (...)捕获所有类型的异常。
        SPDLOG_LOGGER_ERROR(Logger,"Address::GetInterfaceAddresses exception");
        freeifaddrs(results);
        return false;
    }
    freeifaddrs(results);
    return !result.empty();
}

bool Address::GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t>>& result, const std::string& interfaceName, int family) {
    // 用于处理特殊情况：当用户没有指定特定的网络接口名称时，则返回一个默认的结果
    if (interfaceName.empty() || interfaceName == "*") {
        if (family = AF_INET || family == AF_UNSPEC) {
            result.push_back(std::make_pair(Address::ptr(new Ipv4Address()), 0u));
        }
        if (family == AF_INET6 || family == AF_UNSPEC) {
            result.push_back(std::make_pair(Address::ptr(new Ipv6Address()),0u));
        }
        return true;
    }

    std::multimap<std::string,std::pair<Address::ptr, uint32_t>> results;
    
    // 如果调用获取所有网卡信息的函数后返回false，证明获取失败
    if(!GetInterfaceAddresses(results, family)) {
        return false;
    }

    // 如果上一步调用获取所有网卡信息的函数后返回true，证明获取成功，此时results中存储了所有网卡的信息
    // 使用equeal_range函数获取所有键等于interfaceName的元素
    auto its = results.equal_range(interfaceName); //equal_range函数返回一个包含两个迭代器的std::pair，这两个迭代器分别指向multimap中键等于iface的第一个元素和最后一个元素之后的位置。
    for (;its.first != its.second;++its.first) {
        result.push_back(its.first->second);
    }
    return !result.empty();

}

int Address::getFamily() const {
    return getAddr()->sa_family;
}

std::string Address::toString() {
    std::stringstream ss;
    insert(ss);
    return ss.str();
}

bool Address::operator<(const Address& rhs) const {
    socklen_t minlen = std::min(getAddrLen(),rhs.getAddrLen());
    int result = memcmp(getAddr(),rhs.getAddr(),minlen); // memcmp通常用于比较二进制数据
    if (result <0) {
        return true;
    }else if (result > 0) {
        return false;
    }else {
        return rhs.getAddrLen() <= getAddrLen() ? false : true;
    }
}
bool Address::operator==(const Address& rhs) const {
    // 比较地址长度和地址内容
    return getAddrLen() == rhs.getAddrLen() && memcmp(getAddr(),rhs.getAddr(),getAddrLen()) == 0;
}
bool Address::operator!=(const Address& rhs) const {
    return !(*this == rhs);
}

// IpAddress类的实现
IpAddress::ptr IpAddress::Create(const char* hostName, uint16_t port) {
    addrinfo hints, *results=nullptr;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;

    // 获取指定主机名对应的addrinfo链表，一个节点对应一个ip地址
    int error = getaddrinfo(hostName, nullptr, &hints, &results);
    if (error) {
        SPDLOG_LOGGER_DEBUG(Logger,"IpAddress::Create getaddrinfo({},{}) err={} errstr={}",hostName,port,error,gai_strerror(error));
        return nullptr;
    }

    try {
        // 之所以不进行遍历，只取第一个addrinfo，是因为只需要一个地址
        Address::ptr address = Address::Create(results->ai_addr, results->ai_addrlen);
        IpAddress::ptr result = std::dynamic_pointer_cast<IpAddress>(address);
        if (result) {
            result->setPort(port);
        }
        freeaddrinfo(results);
        return result;
    }catch (...) {
        freeaddrinfo(results);
        return nullptr;
    }
}


Ipv4Address::ptr Ipv4Address::Create(const char* ip, uint16_t port) {
    Ipv4Address::ptr result(new Ipv4Address);
    result->m_addr.sin_port = EndianCast(port);
    int error = inet_pton(AF_INET, ip, &(result->m_addr.sin_addr)); // Fix the undefined identifier error
    if (error) {
        SPDLOG_LOGGER_DEBUG(Logger,"IPv4Address::Create({},{}) rt={} errno={} errstr={}",ip, port, result, errno, gai_strerror(errno));
        return nullptr;
    }
    return result;
}

Ipv4Address::Ipv4Address() {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
}

Ipv4Address::Ipv4Address(const sockaddr_in& address) {
    m_addr = address;
}

Ipv4Address::Ipv4Address(uint32_t ip, uint16_t port) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = HostToNetwork(port);
    m_addr.sin_addr.s_addr = HostToNetwork(ip);
}

const sockaddr* Ipv4Address::getAddr() const {
    return reinterpret_cast<const sockaddr*>(&m_addr);
}
sockaddr* Ipv4Address::getAddr() {
    return reinterpret_cast<sockaddr*>(&m_addr);
}
const socklen_t Ipv4Address::getAddrLen() const {
    return sizeof(m_addr);
}
std::ostream& Ipv4Address::insert(std::ostream& out) {
    // 因为要将地址信息插入到输出流，所以要将网络字节序转换为主机字节序
    auto addr = NetworkToHost(m_addr.sin_addr.s_addr);
    out << (addr >> 24) << "."
        << ((addr >>16) & 0xFF) << "."
        <<((addr >>8) & 0xFF) << "."
        << (addr & 0xFF);

    out << ":" <<NetworkToHost(m_addr.sin_port);
    return out;
}


IpAddress::ptr Ipv4Address::broadcastAddress(uint32_t prefixLen) {
    if (prefixLen> 32) {
        return nullptr;
    }
    sockaddr_in broadAddr(m_addr);
    broadAddr.sin_addr.s_addr |= EndianCast(CreateMask<uint32_t>(prefixLen));
    Ipv4Address::ptr result(new Ipv4Address(broadAddr));
    return result;
}

IpAddress::ptr Ipv4Address::networkAddres(uint32_t prefixLen) {
    if (prefixLen > 32) {
        return nullptr;
    }
    sockaddr_in netAddr(m_addr);
    netAddr.sin_addr.s_addr &= EndianCast(CreateMask<uint32_t>(prefixLen));
    IpAddress::ptr result(new Ipv4Address(netAddr));
    return result;
}

IpAddress::ptr Ipv4Address::subnetMask(uint32_t prefixLen) {
    sockaddr_in subnet(m_addr);
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin_family = AF_INET;
    subnet.sin_addr.s_addr = ~EndianCast(CreateMask<uint32_t>(prefixLen));
    Ipv4Address::ptr result(new Ipv4Address(subnet));
    return result;
}

uint32_t Ipv4Address::getPort() const {
    return NetworkToHost(m_addr.sin_port);
}
void Ipv4Address::setPort(uint16_t port) {
    auto newPort = HostToNetwork(port);
    m_addr.sin_port = newPort;
}

Ipv6Address::ptr Ipv6Address::Create(const char* ip, uint16_t port = 0) {
    Ipv6Address::ptr result(new Ipv6Address);
    result->m_addr.sin6_port = HostToNetwork(port);
    int error = inet_pton(AF_INET6, ip, &result->m_addr.sin6_addr);
    if (result) {
        SPDLOG_LOGGER_DEBUG(Logger,"IPv6Address::Create({},{}) result={} errno={} errstr={}",ip, port, result, errno, gai_strerror(errno));
        return nullptr;
    }
    return result;
}

Ipv6Address::Ipv6Address() {
    memset(&m_addr, 0, sizeof(m_addr));
}
Ipv6Address::Ipv6Address(const sockaddr_in6& address) {
    m_addr = address;
}
Ipv6Address::Ipv6Address(const uint8_t ip[16], uint16_t port = 0) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    m_addr.sin6_port = HostToNetwork(port);
    // 由于存储ipv6的数据结构是16个元素的数组，每个元素占用一个字节，故不需要进行字节序转换
    memcpy(&m_addr.sin6_addr.s6_addr, ip, 16);
}

const sockaddr* Ipv6Address::getAddr() const {
    return reinterpret_cast<const sockaddr*>(&m_addr);
}
sockaddr* Ipv6Address::getAddr() {
    return reinterpret_cast<sockaddr*>(&m_addr);
}
const socklen_t Ipv6Address::getAddrLen() const {
    return sizeof(m_addr);
}
std::ostream& Ipv6Address::insert(std::ostream& out) {
    auto addr = reinterpret_cast<uint16_t*>(m_addr.sin6_addr.s6_addr);

    bool prefixZero = false; // 是否使用过一次双冒号压缩，如果使用过则不再使用
    // 根据ipv6的压缩地址方式，进行格式化输出
    out << "[";
    for (int i = 0;i <8;++i) {
        if (addr[i] == 0 && !prefixZero) {
            continue;
        }
        if (i && addr[i-1] == 0 && !prefixZero) {
            out << ":";
            prefixZero = true;
        }
        if (i) {
            out << ":";
        }
        out << std::hex << NetworkToHost(addr[i]) << std::dec;
    }
    if (!prefixZero && addr[7] == 0) {
        out << "::";
    }

    out << "]:" << NetworkToHost(m_addr.sin6_port);
    return out;
}

// std::ostream& Ipv6Address::insert(std::ostream& out) {
//     auto addr = reinterpret_cast<const uint16_t*>(m_addr.sin6_addr.s6_addr);

//     // 查找最长的零序列
//     int zeroSeqStart = -1, zeroSeqLength = 0;
//     int currentSeqStart = -1, currentSeqLength = 0;
//     for (int i = 0; i < 8; ++i) {
//         if (NetworkToHost(addr[i]) == 0) {
//             if (currentSeqStart == -1) currentSeqStart = i;
//             ++currentSeqLength;
//         } else {
//             if (currentSeqLength > zeroSeqLength) {
//                 zeroSeqStart = currentSeqStart;
//                 zeroSeqLength = currentSeqLength;
//             }
//             currentSeqStart = -1;
//             currentSeqLength = 0;
//         }
//     }
//     if (currentSeqLength > zeroSeqLength) {
//         zeroSeqStart = currentSeqStart;
//         zeroSeqLength = currentSeqLength;
//     }

//     out << "[";
//     for (int i = 0; i < 8; ++i) {
//         // 跳过零序列
//         if (zeroSeqLength > 1 && i == zeroSeqStart) {
//             out << (i == 0 ? "::" : ":");
//             i += zeroSeqLength - 1; // 跳过零序列
//             continue;
//         }

//         if (i != 0) out << ":";

//         // 十六进制输出，并去除前导零
//         if (NetworkToHost(addr[i]) == 0) {
//             out << "0";
//         } else {
//             std::stringstream ss;
//             ss << std::hex << NetworkToHost(addr[i]);
//             std::string segment = ss.str();
//             segment.erase(0, segment.find_first_not_of('0'));  // 删除前导零
//             out << segment;
//         }
//     }

//     out << "]:" << NetworkToHost(m_addr.sin6_port);
//     return out;
// }


IpAddress::ptr Ipv6Address::broadcastAddress(uint32_t prefixLen) {
    sockaddr_in6 broadAddr(m_addr);

    // 判断前缀是否是8的倍数，如果不是，则需要对某个字节中的网络位和主机位单独处理
    // 如果是8的倍数，则直接将所有字节置为1
    // 这里要对得到的掩码进行取反，因为不需要进行字节序转换，所以得取反。这里和ipv4不同
    broadAddr.sin6_addr.s6_addr[prefixLen/8] |= ~CreateMask<uint8_t>(prefixLen%8);
    for (int i = prefixLen/8+1;i<16;++i) {
        broadAddr.sin6_addr.s6_addr[i] = 0xFF;
    }
    return Ipv6Address::ptr(new Ipv6Address(broadAddr));
}

IpAddress::ptr Ipv6Address::networkAddres(uint32_t prefixLen) {
    sockaddr_in6 netAddr(m_addr);

    // 与上面的broadcastAddress函数类似
    // 这里要对得到的掩码进行取反
    netAddr.sin6_addr.s6_addr[prefixLen/8] &= ~CreateMask<uint8_t>((prefixLen%8));
    for (int i = prefixLen/8+1;i<16;++i) {
        netAddr.sin6_addr.s6_addr[i] = 0x00;
    }
    return Ipv6Address::ptr(new Ipv6Address(netAddr));
}

IpAddress::ptr Ipv6Address::subnetMask(uint32_t prefixLen) {
    sockaddr_in6 subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin6_family = AF_INET6;
    subnet.sin6_addr.s6_addr[prefixLen/8] = ~CreateMask<uint8_t>(prefixLen%8);
    for (int i = prefixLen/8-1;i>=0;--i) {
        subnet.sin6_addr.s6_addr[i] = 0xFF;
    }
    return Ipv6Address::ptr(new Ipv6Address(subnet));
}

uint32_t Ipv6Address::getPort() const {
    return m_addr.sin6_port;
}

void Ipv6Address::setPort(uint16_t port) {
    m_addr.sin6_port = HostToNetwork(port);
}

// 对于Unix域套接字来说，使用它进行进程间通信可以有两种不同类型的地址：基于文件系统的路径名和 "抽象命名空间" 的地址
// 如果sun_path的第一个字符是'\0'，则这个 Unix 套接字地址属于抽象命名空间，否则就是基于文件系统的路径名

static constexpr size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;


UnixAddress::UnixAddress() {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    
    // -1确保留有一个\0结尾符
    m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN - 1;
    // 这里之所以不使用sizeof(sockaddr_un) - 1，它的结果取决于结构的具体布局和编译器的填充/对齐策略。
    // 理论上两个表达式应该得到相同的结果，但是如果sockaddr_un结构中使用了填充字节或者额外的字段，会导致sizeof大于offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN
}

UnixAddress::UnixAddress(const std::string& path) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;

    // 路径长度，path.size()返回的长度不包括'\0'，所以需要+1
    auto pathLen = path.size() + 1;

    // 如果路径的第一个字符是'\0'则，这个地址是抽象命名空间；
    // 而抽象路径不需要 \0 结束符，所以计算实际长度时要-1
    if (!path.empty() && path[0] == '\0') {
        --m_length;
    }

    if (pathLen > sizeof(m_addr.sun_path)) {
        SPDLOG_LOGGER_ERROR(Logger,"UnixAddress::UnixAddress path too long err={} errstr={}",errno,strerror(errno));
        throw std::invalid_argument("path too long");
    }

    memcpy(m_addr.sun_path, path.c_str(), pathLen);

    m_length = offsetof(sockaddr_un, sun_path) + pathLen;
}

const sockaddr* UnixAddress::getAddr() const {
    return reinterpret_cast<const sockaddr*>(&m_addr);
}

sockaddr* UnixAddress::getAddr() {
    return reinterpret_cast<sockaddr*>(&m_addr);
}

const socklen_t UnixAddress::getAddrLen() const {
    return sizeof(m_addr);
}

void UnixAddress::setAddrLen(uint32_t len) {
    m_length = len;
}

std::ostream& UnixAddress::insert(std::ostream& out) {
    if (m_length > offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
        return out << "\\0" << std::string(m_addr.sun_path + 1, m_length - offsetof(sockaddr_un, sun_path) -1);
    }
    return out << m_addr.sun_path;
}


UnknownAddress::UnknownAddress(int family) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sa_family = family;
}

UnknownAddress::UnknownAddress(const sockaddr& addr) {
    m_addr = addr;
}

const sockaddr* UnknownAddress::getAddr() const {
    return reinterpret_cast<const sockaddr*>(&m_addr);
}

sockaddr* UnknownAddress::getAddr() {
    return reinterpret_cast<sockaddr*>(&m_addr);
}

const socklen_t UnknownAddress::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream& UnknownAddress::insert(std::ostream& out) {
    out << "[UnknownAddress family=" << m_addr.sa_family << "]";
    out << "[UnknownAddress data=" << m_addr.sa_data << "]";
    return out;
}

}