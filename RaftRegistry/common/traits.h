//
// File created on: 2024/03/10
// Author: Zizhou

#ifdef RR_TRAITS_H
#define RR_TRAITS_H

namespace RR {

// function_traits模板结构体的作用:
// function_traits是一种在编译时对函数进行解析和操作的工具，
// function_traits可以提取函数的类型信息。这包括函数的返回类型、参数类型、参数个数（arity）、以及一些转换为其他类型的辅助类型。

// function_traits的用法
// 假如我们有个普通函数：
// int add(int a, int b) {
//     return a + b;
// }
// 可以使用function_traits来提取这个函数的类型信息
// function_traits<decltype(add)>::return_type
// 等等


// 前向声明
template <typename T>
struct function_traits;

/**
 * @brief function_traits特化：用于提取普通函数的特性
 */
template <typename ReturnType, typename... Args>
struct function_traits<ReturnType(Args...)> {
    constexpr int typeNum = sizeof...(Args);
    using function_type = ReturnType(Args...);
    using stl_function_type = std::function<function_type>;
    using function_pointer_type = ReturnType(*)(Args...);

    template<size_t N>
    struct args {
        static_assert(N < sizeof...(Args), "index is out of range, index must less than sizeof Args");
        using type = typename std::tuple_element<N,std::tuple<Args...>>::type;
    }

    using tuple_type = std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...>;    
    using bare_tuple_type = std::tuple<std::remove_const_t<std::remove_reference_t<Args>>...>
};

/**
 * @brief function_traits特化：用于提取函数指针的特性
 */
template<typename ReturnType, typename... Args>
struct function_traits<ReturnType(*)(args)> : function_traits<ReturnType(Args...)> {};

/**
 * @brief function_traits特化：用于提取std::function<ReturnType(Args...)>类型的对象的特性
 * @details 仅适用于显式的std::function<ReturnType(Args...)>类型.
 *          为什么不会发生隐式类型转换？
 *          模板元编程涉及的是在编译时对类型信息的操作和推导。
 *          当你向function_traits传递一个类型时，编译器会尝试找到最匹配的模板特化或模板重载。
 *          在这个过程中，编译器不会考虑是否存在能够在运行时将一种类型转换为另一种类型的构造函数或转换函数。它仅基于传递给模板的类型来解析模板。
 */
template<typename ReturnType, typename... Args>
struct function_traits<std::function<ReturnType(Args...)>> : function_traits<ReturnType(Args...)> {};

/**
 * @brief function_traits特化：用于提取函数对象（可调用对象）的特性
 */
templage <typename Callable>
struct function_traits : function_traits<decltype(&Callable::operator())> {};


/**
 * @brief function_traits特化：用于提取成员函数的特性
 * @details 为什么这里能够提取成员函数的特性？因为成员函数才能使用const和volatile修饰符
 *          __VA_ARGS__是一个宏，代表可变参数列表，它可以接受任意数量的参数。
 */
#define FUNCTION_TRAITS(...)\ // ...代表了宏的可变参数列表，这些参数将被替换到宏展开中__VA_ARGS__的位置
template<typename ReturnType, typename ClassType, typename... Args>\
struct function_traits<ReturnType(ClassType::*)(Args...) __VA_ARGS__> : function_traits<ReturnType<Args...> {};\

FUNCTION_TRAITS()
FUNCTION_TRAITS(const)
FUNCTION_TRAITS(volatile)
FUNCTION_TRAITS(const volatile)


/**
 * @brief 将lambda表达式或其他可调用对象转换为std::function类型或函数指针类型。
 * 
 * @tparam Function 
 * @param lambda 
 * @return function_traits<Function>::stl_function_type 
 */
template <typename Function>
typename function_traits<Function>::stl_function_type to_function(const Function& lambda) {
    return static_cast<typename function_traits<Function>::stl_function_type>(lambda);
}

template <typename Function>
typename function_traits<Function>::stl_function_type to_function(Function&& lambda) {
    return static_cast<typename function_traits<Function>::stl_function_type>(std::forward<Function>(lambda));
}

template <typename Function>
typename function_traits<Funtion>::function_pointer_type to_function_pointer(const Function& lambda) {
    return static_cast<typename function_traits<Funtion>::function_pointer_type>(lambda);
}

}

#endif // RR_TRAITS_H