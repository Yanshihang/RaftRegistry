#include <iostream>
#include <string>

// 写一个嵌套类
class A {
private:
    class B {
    public:
        void print() {
            std::cout << "B::print()" << std::endl;
        }
    };

    static int a;
};

int main() {
    std::cout << sizeof(int);
    return 0;
}