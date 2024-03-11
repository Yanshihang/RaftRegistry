#include <iostream>
#include <string>
#include <fstream>
#include <functional>
#include <chrono>

using namespace std;

int func1(int a) {
    return a;
}

// 写一个带有静态数据成员和静态成员函数和普通函数、普通数据成员的类
class A {
public:
    static int a;
    static void func1(int a) {
        return;
    }
    void func2(int a) {

    }
    int b;
};

int main() {
    auto old = std::chrono::high_resolution_clock::now();
    int i = 0;
    while(i<1000000) {
        ++i;
    }
    std::cout << (std::chrono::high_resolution_clock::now()-old).count() << std::endl;
    // 将上面得到的时间转换为毫秒
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-old).count() << std::endl;
    // 转换为nano
    std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()-old).count();

    return 0;
}