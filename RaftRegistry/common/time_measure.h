//
// File created on: 2024/03/11
// Author: Zizhou

#ifndef TIME_MEASURE_H
#define TIME_MEASURE_H

#include <chrono>
#include <string>
#include <iostream>

namespace RR {
class TimeMeasure {
public:
TimeMeasure() : m_begin(std::chrono::high_resolution_clock::now()) {};
~TimeMeasure() {
    std::cout << "Time elapsed:" << toString(elapsed_seconds()) << "s " << toString(elapsed_milli()) << "ms " << toString(elapsed_micro()) << "us " << toString(elapsed_nano()) << "ns" << std::endl;
}

void reset() {
    m_begin = std::chrono::high_resolution_clock::now();
}

int64_t elapsed_milli() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_begin).count();
}

int64_t elapsed_micro() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_begin).count();
}

int64_t elapsed_nano() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - m_begin).count();
}

int64_t elapsed_seconds() const {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - m_begin).count();
}

int64_t elapsed_minutes() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_begin).count();
}

int64_t elapsed_hours() const {
    return std::chrono::duration_cast<std::chrono::hours>(std::chrono::high_resolution_clock::now() - m_begin).count();
}

private:
    // 保存高性能时钟的开始时间点
    std::chrono::time_point<std::chrono::high_resolution_clock> m_begin;

    /**
     * @brief 每三位数字加一个逗号，方便阅读
     * 
     * @param duration_time 时间段
     * @return std::string 易于阅读的字符串
     */
    std::string toString(int64_t duration_time) const {
        auto time_string = std::to_string(duration_time);
        auto old_len = time_string.size();

        // 计算新字符串的长度
        // 判断逻辑：如果原字符串长度大于3，那么新字符串长度为原字符串长度除以3 + 原字符长度，否则新字符串长度等于原字符串长度
        auto new_len = (old_len / 3 && old_len >3 ? old_len /3 : 0) + old_len;
        time_string.resize(new_len);

        auto front_tail = old_len - 1;
        int count = 0;
        for (int back_tail = new_len-1;back_tail >= 0;--back_tail) {
            if (count !=0 && count % 3 ==0) {
                time_string[back_tail] = ','; // 插入逗号
                ++count;
            }else {
                time_string[back_tail] = time_string[front_tail];
                ++count;
                --front_tail;
            }

        }
        return time_string;
    }
};
}

#endif