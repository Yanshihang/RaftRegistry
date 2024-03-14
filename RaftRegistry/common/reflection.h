//
// File created on: 2024/03/12
// Author: Zizhou

#ifndef RR_REFLECTION_H
#define RR_REFLECTION_H

#include <variant>

namespace RR {

/**
 * @brief 模板变量声明，用于获取类的成员数量
 * @details 如果某些类对这个模板变量进行了特化，那么就可以通过MemberCount模板变量获取到相应类的成员数量
 *          没有对这个模板变量进行特化的类，通过MemberCount模板变量只会返回0
 */
template <typename T>
constexpr std::size_t member = 0;

/**
 * @brief 万能类型转换，可以将UniversalType转换为任意类型
 */
struct UniversalType {
    template <typename T>
    operator T();
};

/**
 * @brief MemberCount的实现函数，核心逻辑
 * 
 * @tparam T 
 * @tparam Args 
 * @return consteval 
 */
template <typename T, typename... Args>
consteval std::size_t MemberCountImple() {
    // 使用requires关键字判断某个类型的构造函数是否还能接收参数
    // 如果能接收参数，则证明还有成员，那么就递归调用MemberCountImple函数
    if constexpr (requires
    {
        T {
            {Args{}}...,
            {UniversalType{}}
        }
    }) {
        return MemberCountImple<T, Args..., UniversalType>();
    }else { // 如果不能接收参数，则证明已经到达了最后一个成员
        return sizeof...(Args);
    }
}

/**
 * @brief 计算某个类型的成员数量
 * 
 * @tparam T 
 * @return consteval 
 */
template <typename T>
consteval std::size_t MemberCount() {
    if constexpr (member<T> > 0) {
        // 如果某个类对member模板变量进行了特化，那么就返回特化的值
        return member<T>;
    }else {// 否则，调用MemberCountImple函数进行计算
        return MemberCountImple<T>();
    }
}

constexpr static std::size_t MaxMember = 64;

/**
 * @brief 对object的所有成员进行访问（即调用visitor）
 * 
 * @param object 被检测的对象
 * @param visitor visitor是需要从外部传入的函数或者可调用对象。
 *                它被设计为一个通用的函数，可以接收对象的成员作为参数，并对其执行某些操作。
 * @return constexpr decltype(auto) 
 */
constexpr decltype(auto) VisitMembers(auto&& object, auto&& visitor) {
    // 使用type别名获取object的类型，不包括const、volatile和引用
    using type = std::remove_cvref_t<decltype(object)>;
    // count用于存储object的成员数量
    constexpr auto count = MemberCount<type>();

    // 判断type是否是一个空类
    if constexpr (count == 0 && std::is_class_v<type> && !std::is_same_v<type, std::monostate>) {
        // 这个断言会被恒定触发
        // 因为只要type是一个类，即便是空类，sizeof(type)也等于1
        static_assert(!sizeof(type), "object is empty");
    }

    static_assert(count <= MaxMember, "exceed the max or members");

    if constexpr(count ==0) {
        return visitor();
    }else if constexpr(count == 1) {
        // 结构化绑定，将object的前一个数据成员绑定到m1上
        auto&& [m1] = object;
        return visitor(m1);
    }else if constexpr(count == 2) {
        auto&& [m1,m2] = object;
        return visitor(m1,m2);
    }else if constexpr (Count == 3) {
        auto &&[m1, m2, m3] = object;
        return visitor(m1, m2, m3);
    }
    else if constexpr (Count == 4) {
        auto &&[m1, m2, m3, m4] = object;
        return visitor(m1, m2, m3, m4);
    }
    else if constexpr (Count == 5) {
        auto &&[m1, m2, m3, m4, m5] = object;
        return visitor(m1, m2, m3, m4, m5);
    }
    else if constexpr (Count == 6) {
        auto &&[m1, m2, m3, m4, m5, m6] = object;
        return visitor(m1, m2, m3, m4, m5, m6);
    }
    else if constexpr (Count == 7) {
        auto &&[m1, m2, m3, m4, m5, m6, m7] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7);
    }
    else if constexpr (Count == 8) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8);
    }
    else if constexpr (Count == 9) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9);
    }
    else if constexpr (Count == 10) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10);
    }
    else if constexpr (Count == 11) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11);
    }
    else if constexpr (Count == 12) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12);
    }
    else if constexpr (Count == 13) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13);
    }
    else if constexpr (Count == 14) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14] =
        object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14);
    }
    else if constexpr (Count == 15) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15] =
        object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15);
    }
    else if constexpr (Count == 16) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16);
    }
    else if constexpr (Count == 17) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17);
    }
    else if constexpr (Count == 18) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18);
    }
    else if constexpr (Count == 19) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19);
    }
    else if constexpr (Count == 20) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20);
    }
    else if constexpr (Count == 21) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21);
    }
    else if constexpr (Count == 22) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22);
    }
    else if constexpr (Count == 23) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23);
    }
    else if constexpr (Count == 24) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24);
    }
    else if constexpr (Count == 25) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25);
    }
    else if constexpr (Count == 26) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26);
    }
    else if constexpr (Count == 27) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27] =
        object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27);
    }
    else if constexpr (Count == 28) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28] =
        object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28);
    }
    else if constexpr (Count == 29) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29);
    }
    else if constexpr (Count == 30) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30);
    }
    else if constexpr (Count == 31) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31);
    }
    else if constexpr (Count == 32) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32);
    }
    else if constexpr (Count == 33) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33);
    }
    else if constexpr (Count == 34) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34);
    }
    else if constexpr (Count == 35) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35);
    }
    else if constexpr (Count == 36) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36);
    }
    else if constexpr (Count == 37) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37);
    }
    else if constexpr (Count == 38) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38);
    }
    else if constexpr (Count == 39) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39);
    }
    else if constexpr (Count == 40) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40] =
        object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40);
    }
    else if constexpr (Count == 41) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41] =
        object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41);
    }
    else if constexpr (Count == 42) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42);
    }
    else if constexpr (Count == 43) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43);
    }
    else if constexpr (Count == 44) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44);
    }
    else if constexpr (Count == 45) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45);
    }
    else if constexpr (Count == 46) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46);
    }
    else if constexpr (Count == 47) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47);
    }
    else if constexpr (Count == 48) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48);
    }
    else if constexpr (Count == 49) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49);
    }
    else if constexpr (Count == 50) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50);
    }
    else if constexpr (Count == 51) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51);
    }
    else if constexpr (Count == 52) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51, m52);
    }
    else if constexpr (Count == 53) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53] =
        object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51, m52, m53);
    }
    else if constexpr (Count == 54) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53, m54] =
        object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51, m52, m53, m54);
    }
    else if constexpr (Count == 55) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53, m54,
        m55] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51, m52, m53, m54, m55);
    }
    else if constexpr (Count == 56) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53, m54,
        m55, m56] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51, m52, m53, m54, m55, m56);
    }
    else if constexpr (Count == 57) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53, m54,
        m55, m56, m57] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51, m52, m53, m54, m55, m56, m57);
    }
    else if constexpr (Count == 58) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53, m54,
        m55, m56, m57, m58] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51, m52, m53, m54, m55, m56, m57, m58);
    }
    else if constexpr (Count == 59) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53, m54,
        m55, m56, m57, m58, m59] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51, m52, m53, m54, m55, m56, m57, m58, m59);
    }
    else if constexpr (Count == 60) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53, m54,
        m55, m56, m57, m58, m59, m60] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51, m52, m53, m54, m55, m56, m57, m58, m59, m60);
    }
    else if constexpr (Count == 61) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53, m54,
        m55, m56, m57, m58, m59, m60, m61] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61);
    }
    else if constexpr (Count == 62) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53, m54,
        m55, m56, m57, m58, m59, m60, m61, m62] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62);
    }
    else if constexpr (Count == 63) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53, m54,
        m55, m56, m57, m58, m59, m60, m61, m62, m63] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62,
                       m63);
    }
    else if constexpr (Count == 64) {
        auto &&[m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15,
        m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28,
        m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41,
        m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53, m54,
        m55, m56, m57, m58, m59, m60, m61, m62, m63, m64] = object;
        return visitor(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14,
                       m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26,
                       m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38,
                       m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                       m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62,
                       m63, m64);
    }
}

}

#endif // RR_REFLECTION_H