#include <iostream>
#include <variant>
#include <memory>
#include <sys/socket.h> // Include the header file for socket
#include <netinet/in.h> // Include the header file for sockaddr_in
#include <sys/ioctl.h> // Include the header file for ioctl
#include <optional>

#include <sys/types.h> // 对于某些系统来说包含了基本的数据类型
#include <net/if.h> // 包含ifconf和ifreq结构体
#include <arpa/inet.h> // Include the header file for inet_ntoa
#include "libgo/libgo.h"

#include "libgo/coroutine.h"

using namespace std;


int main() {
    std::optional<int> a = 1;

    cout << *a << endl;

    return 0;
}

