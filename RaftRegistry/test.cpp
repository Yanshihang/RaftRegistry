#include <iostream>
#include <variant>
#include <memory>

using namespace std;

class A{
public:
    virtual void func() {};

    int a = 90;
};

class B: public A{
public:
    void func() {
        // a = 78;
        // cout << a;
    }

    void func2() {
        a = 78;
        cout << a;
    }
};

int main() {
    B b;
    b.func2();

    return 0;
}