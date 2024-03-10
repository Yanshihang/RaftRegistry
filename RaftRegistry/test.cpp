#include <iostream>
#include <string>
#include <fstream>
#include <functional>

using namespace std;

int func1(int a) {
    return a;
}

// 写一个带有静态数据成员和静态成员函数和普通函数、普通数据成员的类
class A {
public:
    static int a;
    static void func1(int a) {

    }
    void func2(int a) {

    }
    int b;
};

int main() {

    int a = 90;
    int aa = a;
    int& b = a;
    const int *const d = &a;
    int* e = &a;
    decltype(d) c = &aa;

    decltype(&func1) fu = func1;

    decltype(*e) ee

    return 0;
}