#include <iostream>
#include <string>
#include <fstream>

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
    std::ofstream ofs("test.txt");
    ofs.write("hello", 5);
    ofs.close();

    std::ifstream ifs("test.txt");
    char buf[50];
    ifs.read(buf, 50);
    if (ifs.fail()) {
        for (int i=0;i < 5;++i) {
            std::cout << buf[i] << std::endl;
        }
        std::cout << "FAIL" << std::endl;
    }

    return 0;
}