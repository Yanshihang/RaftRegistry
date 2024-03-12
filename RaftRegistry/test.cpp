#include <iostream>
#include <variant>

using namespace std;

decltype(auto) func(auto&& f) {
    cout << "func" << endl;
    return f();
}

auto func2() {
    cout << "func2" << endl;
    return 2;
}

class A {
public:
    int a;
    int b;
    A(int a, int b) : a(a), b(b) {}
};

int main() {
    int a = 1;
    int b = 2;
    A aObject(a, b);

    return 0;
}