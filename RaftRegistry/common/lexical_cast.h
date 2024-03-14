//
// File created on: 2024/03/11
// Author: Zizhou

#ifndef RR_LEXICAL_CAST_H
#define RR_LEXICAL_CAST_H

#include <cstddef>
#include <string>
#include <cstring>
#include <stdexcept>

namespace RR {
namespace DETAIL {

template <typename To, typename From>
struct Caster {
};

// 将字符串转换为整数

/**
 * @brief 将字符串类型转换为int
 * 
 * @tparam From 字符串类型
 */
template <typename From>
struct Caster<int, From> {
    static int Cast(const From& from) {
        return std::stoi(from);
    }
};

/**
 * @brief 将字符串类型转换为unsigned int
 * 
 * @tparam From 字符串类型
 */
template <typename From>
struct Caster<unsigned int, From> {
    static unsigned int Cast(const From& from) {
        return static_cast<unsigned int>(std::stoul(from)); // 由于没有std::stoui，所以需要先转换为unsigned long，然后通过static_cast转换为unsigned int
    }
};

/**
 * @brief 将字符串类型转换为long
 * 
 * @tparam From 字符串类型
 */
template <typename From>
struct Caster<long, From> {
    static long Cast(const From& from) {
        return std::stol(from);
    }
};

/**
 * @brief 将字符串类型转换为unsigned long
 * 
 * @tparam From 字符串类型
 */
template <typename From>
struct Caster<unsigned long, From> {
    static unsigned long Cast(const From& from) {
        return std::stoul(from);
    }
};

/**
 * @brief 将字符串类型转换为long long
 * 
 * @tparam From 字符串类型
 */
template <typename From>
struct Caster<long long, From> {
    static long long Cast(const From& from) {
        return std::stoll(from);
    }
};

/**
 * @brief 将字符串类型转换为unsigned long long
 * 
 * @tparam From 字符串类型
 */

template <typename From>
struct Caster<unsigned long long, From> {
    static unsigned long long Cast(const From& from) {
        return std::stoull(from);
    }
};

/**
 * @brief 将字符串转换为float
 * 
 * @tparam From 字符串类型
 */
template <typename From>
struct Caster<float, From> {
    static float Cast(const From& from) {
        return std::stof(from);
    }
};

/**
 * @brief 将字符串转换为double
 * 
 * @tparam From 字符串类型
 */
template <typename From>
struct Caster<double, From> {
    static double Cast(const From& from) {
        return std::stod(from);
    }
};

// 将整数、字符串、字符数组转换为bool

/**
 * @brief 将整数转换为bool
 * 
 * @tparam From 可以传入任何类型，但是只有当From是整数类型时，才会被实例化
 * 
 * @details 当然这个模板偏特化可以接收任何类型（即便不是整数类型），
 *          但是只有当From是整数类型时，才会被实例化。
 *          这是因为std::is_integral<From>::value是一个bool值，当From是整数类型时，它的值是true，否则是false。
 *          std::enable_if的第一个模板参数是false时，不会实例化这个模板。    
 */
template <typename From>
struct Caster<bool, From> {
    static typename std::enable_if<std::is_integral<From>::value,bool>::type Cast(From from) {
        // !运算符会将其操作数转换为布尔值，然后取反。因此，!from会将from转换为布尔值，如果from是非零的，!from就会得到false，如果from是零，!from就会得到true。
        return !!from;
    }
};


/**
 * @brief 判断能否将字符串转换为bool，Convert函数的辅助函数
 * 
 * @param from 要判断的字符串
 * @param len 要判断的字符串的长度
 * @param s "true"或"false"
 * @return true 能转换为bool
 * @return false 不能转换为bool
 */
bool CheckBool(const char* from, const size_t len, const char* s);

/**
 * @brief 判断是否能将字符串转换为bool，如能则转换，否则抛出异常
 * 
 * @param from 要转换的字符串
 * @return true 如果字符串是"true"则返回true
 * @return false 如果字符串是"false"则返回false
 */
bool Convert(const char* from);

template<>
struct Caster<bool, std::string> {
    static bool Cast(const std::string& from) {
        return Convert(from.c_str());
    }
};

template<>
struct Caster<bool, const char*> {
    static bool Cast(const char* from) {
        return Convert(from);
    }
};

// 虽然const char*足以覆盖const char*和char*两种情况
// 但是仍提供char*的特化是为了使得接口的使用和意图更加清晰

template<>
struct Caster<bool, char*> {
    static bool Cast(const char* from) {
        return Convert(from);
    }
};

// 前面已经定义了针对char*的模板特化，为什么这里还需要定义char[N]的特化呢？
// 这是因为char[N]和char*是不同的类型，char[N]是一个数组类型，而char*是一个指针类型。
// char[N] 保留了数组的大小信息，这意味着编译时就知道了数组的确切大小。而 char* 仅仅是一个指针，它指向某个位置，但不包含大小信息，需要判断'\0'来确定字符串的长度。
// 明确区分 char[N] 和 char* 允许 LexicalCastFunc 函数准确处理固定大小数组的情况，并利用数组大小的编译时信息。这种区分可以提高类型安全性，确保在编译时就能捕获到一些潜在的错误，同时也允许对固定大小数组的特定优化。
// 在函数的形参中如何对数组进行引用，即const char (&from) [N]，则from会保留数组的大小信息，这样就可以在编译时知道数组的确切大小。因为此时from是一个引用，所以from不会退化为指针。

template<unsigned N>
struct Caster<bool, const char [N]> {
    static bool Cast(const char (&from) [N]) {
        return Convert(from);
    }
};

template<unsigned N>
struct Caster<bool, char[N]> {
    static bool Cast(const char (&from) [N]) {
        return Convert(from);
    }
};

// 将整型转换为字符串
template <typename From>
struct Caster<std::string, From> {
    static std::string Cast(From from) {
        return std::to_string(from);
    }
};


} // namespace DETAIL

// 下面两个函数模板是对外的接口
// 这两个函数模板的作用是判断To和From是否是相同类型，如果是，则直接返回From，否则调用Caster<To, From>::Cast(from)进行转换

/**
 * @brief 判断To和From是否是相同类型，如果是，则直接返回From，否则调用Caster<To, From>::Cast(from)进行转换
 * 
 * @tparam To 转换后的目标类型
 * @tparam From 转换前的类型
 * @param from 用于转换的值
 * @return std::enable_if<!std::is_same<To, From>::value, To>::type 
 */
template <typename To, typename From>
std::enable_if<!std::is_same<To, From>::value, To>::type LexicalCastFunc(const From& from) {
    return DETAIL::Caster<To, From>::Cast(from);
}

template <typename To, typename From>
std::enable_if<std::is_same<To, From>::value, To>::type LexicalCastFunc(const From& from) {
    return from;
}
} // namespace RR

#endif // RR_LEXICAL_CAST_H
