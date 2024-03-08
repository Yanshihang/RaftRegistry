//
// File created on: 2024/03/02
// Author: Zizhou


#ifndef RR_UTIL_H
#define RR_UTIL_H

#include <cstdint> // Add missing include for uint64_t
#include <concepts> // Add missing include for std::integral
#include <byteswap.h> // Add missing include for bswap_16, bswap_32, bswap_64
#include <memory>
#include <spdlog/logger.h> // Add missing include for spdlog::logger



#if RR_ENABLE_DEBUGGER
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#endif

namespace co {
class Scheduler;
}

namespace RR {

/**
 * @brief 获取当前时间的毫秒数和微秒数
 * 
 * @return uint64_t 
 */
uint64_t GetCuurentTimeMs();
uint64_t GetCuurentTimeUs();


/**
 * @brief 转换传入的值的字节序
 * 
 * @tparam T 只接受整数类型
 * @param value 用于转换的值
 * @return constexpr T 
 */
template <std::integral T>
constexpr T ByteSwap(T value) {
    if constexpr(sizeof(uint16_t) == sizeof(value)) {
        return static_cast<T> (bswap_16(static_cast<uint16_t>(value)));
    }else if constexpr(sizeof(uint32_t) == sizeof(value)) {
        return static_cast<T> (bswap_32(static_cast<uint32_t>(value)));
    }else if constexpr(sizeof(uint64_t) == sizeof(value)) {
        return static_cast<T> (bswap_64(static_cast<uint64_t>(value)));
    }
}


/**
 * @brief 主机序转为网络序，检测主机序是否为大端序，如果是则不做任何操作，否则转换字节序
 * 
 * @tparam T 在编译期检测传入的值是否为整数类型，不是则编译时报错
 * @param value 
 * @return constexpr T 
 */
template <std::integral T>
constexpr T HostToNetCast(T value) {
    if constexpr (sizeof(uint8_t) == sizeof(value) || std::endian::native == std::endian::big) {
        return value;
    } else if constexpr (std::endian::native == std::endian::little) {
        return ByteSwap(value);
    }
    
}

/**
 * @brief 获取spdlog的logger实例
 * 
 * @return std::shared_ptr<spdlog::logger> 
 */
std::shared_ptr<spdlog::logger> GetLoggerInstance();

/**
 * @brief 提取栈信息并转换成字符串
 * 
 * @param size 栈信息的最大长度
 * @param skip 跳过的栈帧的层数
 * @param prefix 每个栈帧的前缀
 * @return std::string 
 * 
 * @details 
 */
std::string BackTraceToString(int size, int skip = 2, const std::string& prefix = "");

/**
 * @brief 提取栈信息，并转换为人类可读的信息
 * 
 * @param size 栈信息的最大长度
 * @param skip 跳过的栈帧的层数
 * @param bt 存储函数调用栈中的信息
 */
void BackTrace(int size, int skip = 1, std::vector<std::string>& bt);

class CycleTimerTocken {
public:
    CycleTimerTocken(std::shared_ptr<bool> cancel=nullptr) : cancel(std::move(cancel)) {}

    operator bool();

    bool isCancel() const;
    
    void stop() const;


private:
    std::shared_ptr<bool> cancel;
};

/**
 * @brief 创建一个定时器
 * 
 * @param interval_ms 循环定时器的间隔时间ms
 * @param times 循环次数
 * @param callback 回调函数
 * @param scheduler 协程调度器（调用第三方libgo库）
 * @return CycleTimerTocken 
 * 
 * @details 开启一个协程，并使用scheduler调度器对协程进行调度；每过interval_ms的时间就调用一次callback，共调用times次。
 */
CycleTimerTocken CycleTimer(unsigned interval_ms, int times = -1, std::function<void()> callback, co::Scheduler* scheduler = nullptr);

} // namespace RR

#endif // RR_UTIL_H
