//
// File created on: 2024/03/17
// Author: Zizhou

#ifndef RR_SOCKET_H
#define RR_SOCKET_H

#include <memory>
#include <ostream>
#include <sys/time.h>

#include <RaftRegistry/net/address.h>

namespace RR {

class Socket : public std::enable_shared_from_this<Socket> {
public:
    using ptr = std::shared_ptr<Socket>;
    using weak_ptr = std::weak_ptr<Socket>;

    // 定义套接字地址族的枚举
    enum Family {
        IPV4 = AF_INET,
        IPV6 = AF_INET6,
        UNIX = AF_UNIX,
    };

    // 定义套接字类型的枚举
    enum Type {
        TCP = SOCK_STREAM,
        UDP = SOCK_DGRAM,
    };

    // 构造函数和析构函数
    Socket(int family, int type, int protocol);
    virtual ~Socket();

    // 创建TCP/UDP套接字
    static ptr CreateTCP(Address::ptr address);
    static ptr CreateUDP(Address::ptr address);

    // 不绑定地址
    static ptr CreateTCPSocket();
    static ptr CreateUDPSocket();

    // 创建IPv6套接字
    static ptr CreateTCPSocket6();
    static ptr CreateUDPSocket6();

    // 创建Unix套接字
    static ptr CreateUnixTCPSocket();
    static ptr CreateUnixUDPSocket();

    // 发送和接收超时的获取和设置
    void setSendTimeout(uint64_t time);
    uint64_t getSendTimeout();
    void setRecvTimeout(uint64_t time);
    uint64_t getRecvTimeout();

    // 套接字选项的获取
    bool getOption(int level, int option, void* result, size_t* len);

    template <typename T>
    bool getOption(int level, int option, T* result) {
        size_t len = sizeof(T);
        return getOption(level, option, result, &len);
    }
    // 套接字选项的设置
    bool setOption(int level, int option, void* resullt, size_t len);

    template <typename T>
    bool setOption(int level, int option, T* result) {
        size_t len = sizeof(T);
        return setOption(level, option, result, &len);
    }

    // 接受连接请求，返回新的套接字对象
    ptr accept();
    // 绑定地址到套接字上
    bool bind(const Address::ptr address);
    // 连接到指定地址
    bool connect(const Address::ptr address, uint64_t timeoutMs = -1);
    // 监听端口
    bool listen(int backLog = SOMAXCONN);
    // 关闭套接字
    bool close();
    
    // 数据发送函数

    /**
     * @brief 发送数据
     * 
     * @param[in] buffer 待发送数据的缓冲区
     * @param[in] length 待发送数据的长度
     * @param[in] flags 标志字
     * @return
     *      @retval >0 发送成功对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t send(const void* buffer, size_t length, int flags = 0);
    
    /**
     * @brief 发送数据
     * 
     * @param[in] buffer 待发送数据的iovec数组
     * @param[in] length 待发送数据的iovec长度
     * @param[in] flags 标志字
     * @return
     *      @retval >0 发送成功对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t send(const iovec* buffer, size_t length, int flags = 0);
    
    /**
     * @brief 发送数据
     * 
     * @param[in] buffer 待发送数据的缓冲区
     * @param[in] length 待发送数据的长度
     * @param[in] to 发送的目标地址
     * @param[in] flags 标志字
     * @return
     *      @retval >0 发送成功对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0);
    
    /**
     * @brief 发送数据
     * 
     * @param[in] buffer 待发送数据的iovec数组
     * @param[in] length 待发送数据的iovec长度
     * @param[in] to 发送的目标地址
     * @param[in] flags 标志字
     * @return
     *      @retval >0 发送成功对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t sendTo(const iovec* buffer, size_t length, const Address::ptr to, int flags = 0);

    // 数据接收函数

    /**
     * @brief 接受数据
     * @param[out] buffer 接收数据的缓冲区
     * @param[in] length 接收数据的大小
     * @param[in] flags 标志字
     * @return
     *      @retval >0 接收到对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t recv(void* buffer, size_t length, int flags = 0);
    
    /**
     * @brief 接受数据
     * @param[out] buffer 接收数据的iovec数组
     * @param[in] length 接收数据的iovec数组长度
     * @param[in] flags 标志字
     * @return
     *      @retval >0 接收到对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t recv(iovec* buffer, size_t length, int flags = 0);
    
    /**
     * @brief 接受数据
     * @param[out] buffer 接收数据的缓冲区
     * @param[in] length 接收数据的大小
     * @param[out] from 发送端地址
     * @param[in] flags 标志字
     * @return
     *      @retval >0 接收到对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0);
    
    /**
     * @brief 接受数据
     * @param[out] buffer 接收数据的iovec数组
     * @param[in] length 接收数据的iovec数组长度
     * @param[out] from 发送端地址
     * @param[in] flags 标志字
     * @return
     *      @retval >0 接收到对应大小的数据
     *      @retval =0 socket被关闭
     *      @retval <0 socket出错
     */
    virtual ssize_t recvFrom(iovec* buffer, size_t length, Address::ptr from, int flags = 0);

    // 获取远程地址和本地地址
    Address::ptr getRemoteAddress();
    Address::ptr getLocalAddress();

    // 获取套接字的基本类型
    int getSocket() const;
    int getFamily() const;
    int getType() const;
    int getProtocol() const;

    bool isConnected() const;
    bool isValid() const;
    int getError(); // 不能设置为const，因为内部调用了getsockopt来获取错误码，而getsockopt获取错误码后会清除错误码（这是一个修改动作）

    // 打印套接字的详细信息
    std::ostream& dump(std::ostream& os) const;
    // 将套接字的信息转换为字符串
    std::string toString() const;

    bool cancelRead();
    bool cancelWrite();
    bool cancelAccept();
    bool cancelAll();

private:
    // 初始化套接字的设置
    void initSocket();
    // 创建新的套接字
    void newSocket();
    // 初始化已经存在的套接字对象
    bool init(int sock);

    int m_socket; // 套接字文件描述符
    int m_family; // 地址族
    int m_type; // 套接字类型
    int m_protocol; // 使用的协议

    bool m_isConnected = false; // 连接状态标志

    Address::ptr m_remoteAddress; // 远程地址
    Address::ptr m_localAddress; // 本地地址
};

} // namespace RR



#endif // RR_SOCKET_H