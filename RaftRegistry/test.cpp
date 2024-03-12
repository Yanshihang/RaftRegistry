#include <iostream>
#include <string>
#include <fstream>
#include <functional>
#include <chrono>
#include <vector>

using namespace std;

// template <unsigned N,unsigned M>
// int compare(const char(&p1)[N],const char(&p2)[M]) {
// return strcmp(p1,p2);
// }

// void func(char (&c) [10]) {
//     cout << sizeof(c) << endl;

// }

void func(char c [10])  {
    cout << sizeof(c) << endl; // 输出结果：10
}

int main() {
    char c[10] = "asdfasdfd";
    func(c);

    return 0;
}