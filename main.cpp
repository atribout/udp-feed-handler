#include <iostream>
#include <thread>
#include <atomic>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "include/Messages.h"
#include "include/RingBuffer.h"

constexpr size_t BUFFER_SIZE = 1024;

RingBuffer<AddOrderMsg, BUFFER_SIZE> ringBuffer;

std::atomic<bool> running{true};

void consumer_thread()
{
    std::cout << "Consumer thread started (LOB Simulation)\n";

    AddOrderMsg msg;
    uint64_t count = 0;

    while(running)
    {
        if (ringBuffer.pop(msg))
        {
            if (count % 10 == 0)
            {
                std::cout << "[LOB Consumer] Processing Order ID: " << msg.id 
                    << " Price: " << msg.price << "\n";
            }
            count++;
        }
        else
        {
            std::this_thread::yield();
        }
    }
}

int main()
{
    std::thread consumer(consumer_thread);

    int sockfd;
    struct sockaddr_in servaddr, cliaddr;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed");
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = 1; tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(1234);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("Bind failed");
        return -1;
    }

    std::cout << "Feed handler listening on port 1234..." << std::endl;

    char buffer[1024];
    socklen_t len = sizeof(cliaddr);

    while (running)
    {
        int n = recvfrom(sockfd, (char *)buffer, 1024, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);

        if (n > 0)
        {
            MsgType type = static_cast<MsgType>(buffer[0]);

            if(type == MsgType::AddOrder)
            {
                AddOrderMsg* msgPtr = reinterpret_cast<AddOrderMsg*>(buffer);

                if(!ringBuffer.push(*msgPtr))
                {
                    std::cerr << "Ring buffer FULL! Dropping packet.\n";
                }
            }
        }
    }

    close(sockfd);
    consumer.join();
    return 0;
}