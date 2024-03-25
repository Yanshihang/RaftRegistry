#include <iostream>
#include <variant>
#include <memory>
#include <sys/socket.h> // Include the header file for socket
#include <netinet/in.h> // Include the header file for sockaddr_in
#include <sys/ioctl.h> // Include the header file for ioctl

#include <sys/types.h> // 对于某些系统来说包含了基本的数据类型
#include <net/if.h> // 包含ifconf和ifreq结构体
#include <arpa/inet.h> // Include the header file for inet_ntoa
#include "libgo/libgo.h"

#include "libgo/coroutine.h"

// using namespace std;


int main() {
    co::co_chan<int> chan;
    chan << 1;
    std::cout << 1 << std::endl;

    return 0;
}

// int main() {
    

    // auto sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    // struct ifconf ifc;
    // struct ifreq* ifr = nullptr;
    // char buf[512];
    // ifc.ifc_len = 512;
    // ifc.ifc_buf = buf;

    // ioctl(sockfd, SIOCGIFCONF, &ifc);
    // ifr = reinterpret_cast<struct ifreq*>(buf);
    // for (int i = (ifc.ifc_len/sizeof(struct ifreq));i>0;--i) {
    //     if (true) {
    //         cout << ifr->ifr_name << '\t' << ifr->ifr_mtu << '\t' << ifr->ifr_ifindex << '\t' << ifr->ifr_flags << '\t' << std::string(inet_ntoa(reinterpret_cast<struct sockaddr_in*>(&(ifr->ifr_addr))->sin_addr)) << '\t' << reinterpret_cast<struct sockaddr_in*>(&(ifr->ifr_addr))->sin_port << endl;
    //         ifr++;
    //     }
    // }

    // union {
    //     int i;
    //     char x[5];
    // } u;

    // u.i = 90;
    // u.x[0] = 1;
    // u.x[1] = 0;
    // u.x[2] = 0;
    // u.x[3] = 0;
    // u.x[4] = 9;
    // cout << u.i << endl;


// }