#include "udp_client.hpp"

int main(int argc, char* argv[]) {
    auto udp = std::make_shared<UdpClient>("172.16.100.247", 7777);

    udp->start();
    auto count = 0;
    // std::this_thread::sleep_for(std::chrono::seconds(1));
    while(count++ < 10){
        udp->send(std::to_string(count));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // std::this_thread::sleep_for(std::chrono::seconds(2));
    udp->stop();

    return 0;
}