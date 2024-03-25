//
// File created on: 2024/03/23
// Author: Zizhou

#ifndef RR_RPC_RPC_H
#define RR_RPC_RPC_H

#include <cstdint>
#include <memory>
#include "RaftRegistry/rpc/serializer.h"

namespace RR::rpc {

// 定义RPC服务订阅时使用的前缀常量
inline const char* RPC_SERVICE_NAME = "[[rpc service subscribe]]";

/**
 * @brief 定义一个模板结构体return_type，用于一般类型
 * 
 * @tparam T 
 */
template <typename T>
struct return_type {
    using type = T; // 直接使用模板参数作为类型
};

/**
 * @brief 特化return_type结构体模板，针对void类型
 * 
 * @tparam  
 */
template <>
struct return_type<void> {
    using type = uint8_t; // 如果模板参数是void，则使用uint8_t作为类型
};

/**
 * @brief 定义模板别名 return_type_t，便于获取 return_type 的内部类型
 * 
 * @tparam T 
 */
template <typename T>
using return_type_t = typename return_type<T>::type; // 使用别名模板定义

// 定义RPC调用可能的状态枚举
enum RpcState {
    RPC_SUCCESS = 0,    // RPC调用成功
    RPC_FAIL,           // RPC调用失败
    RPC_NO_MATCH,       // 没有匹配的函数
    RPC_NO_METHOD,      // 没有指定的方法
    RPC_CLOSED,         // RPC连接关闭
    RPC_TIMEOUT         // RPC调用超时
};

/**
 * @brief 定义一个模板类Result，用于封装RPC调用的结果
 * 
 * @tparam T 默认为void类型
 */
template <typename T = void>
class Result {
public:
    using ptr = std::shared_ptr<Result>; // 定义Result的智能指针类型
    using rowType = T; // 定义Result的原始类型
    using returnType = return_type_t<T>; // 定义Result的返回类型
    using msgType = std::string; // 用于存储消息的类型
    using codeType = uint16_t; // 用于存储状态码的类型

    Result() {}

    // 创建一个成功的Result对象
    static Result<T> Success() {
        Result<T> res;
        res.setCode(RPC_SUCCESS);
        res.setMsg("success");
        return res;
    }

    // 创建一个失败的Result对象
    static Result<T> Fail() {
        Result<T> res;
        res.setCode(RPC_FAIL);
        res.setMsg("fail");
        return res;
    }

    // 检查Result对象是否有效（即状态码为 0）
    bool valid() {
        return m_code == 0;
    }

    codeType getCode () {
        return m_code;
    }

    const msgType& getMsg() {
        return m_msg;
    }

    returnType& getVal() {
        return m_value;
    }

    void setCode(const codeType& code) {
        m_code = code;
    }

    void setMsg(const msgType& msg) {
        m_msg = msg;
    }

    void setVal(const returnType& val) {
        m_value = val;
    }

    // 重载->操作符，用于直接访问存储的值
    returnType* operator->() noexcept {
        return &m_value;
    }

    const returnType* operator->() const noexcept {
        return &m_value;
    }

    std::string toString() {
        std::stringstream ss;
        ss << "[ code = " << m_code << " msg = " << m_msg << " value = " << m_value << " ]";
        return ss.str();
    }

    /**
     * @brief 反序列化回result
     * 
     * @param in 序列化的结果
     * @param d 存储反序列化后的结果
     * @return Serializer& 
     */
    friend Serializer& operator >> (Serializer& in, Result<T>& d) {
        in >> d.m_code >> d.m_msg;
        if (d.m_code == 0) { // 如果状态码为0，表示成功，继续提取存储的值
            in >> d.m_value;
        }
        return in;
    }

    /**
     * @brief 将 Result 序列化
     * @param[in] out 序列化输出
     * @param[in] d 将Result序列化
     * @return Serializer
     */
    friend Serializer& operator << (Serializer& out, const Result<T>& d) {
        out << d.m_code << d.m_msg << d.m_value;
        return out;
    }

private:
    // RPC调用的状态码，用于标示调用成功或各种错误情况
    codeType m_code = 0;
    // RPC调用返回的消息，通常用于提供错误信息或其它说明
    msgType m_msg;
    // RPC调用的实际返回值，其类型根据模板参数 T 决定
    returnType m_value;
};

    
}

#endif // RR_RPC_RPC_H