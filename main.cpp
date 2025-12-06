#include <iostream>
#include <thread>
#include <atomic>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "OrderBook.h"
#include "Order.h"
#include "Listeners.h"
#include "include/Messages.h"
#include "include/RingBuffer.h"

constexpr size_t BUFFER_SIZE = 1024;
RingBuffer<AddOrderMsg, BUFFER_SIZE> ringBuffer;
std::atomic<bool> running{true};

void consumer_thread()
{
    std::cout << "Engine started (waiting for data)...\n";

    EmptyListener listener;
    OrderBook<EmptyListener> lob(listener);

    AddOrderMsg msg;

    uint64_t totalLatency = 0;
    uint64_t maxLatency = 0;
    uint64_t count = 0;

    while (running) {
        if (ringBuffer.pop(msg)) {
            
            auto start = std::chrono::high_resolution_clock::now();

            // --- CRITICAL ZONE ---
            Side side = (msg.side == 'B') ? Side::Buy : Side::Sell;
            Order newOrder(msg.id, msg.price, msg.quantity, side);
            lob.submitOrder(newOrder);
            // -----------------------------------------

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            
            totalLatency += duration;
            if (duration > maxLatency) maxLatency = duration;
            count++;

            if (count % 100000 == 0) {
                std::cout << "[LATENCY] Average per order: " 
                          << (totalLatency / 100000) << " nanoseconds (" 
                          << (totalLatency / 100000 / 1000.0) << " µs)\n";
                std::cout << "[LATENCY] Avg: " << (totalLatency / 100000) << " ns | "
                    << "Max (Jitter): " << maxLatency << " ns\n";
                totalLatency = 0;
                maxLatency = 0;
            }
        } else {
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

    struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
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

    std::cout << "Newtork thread listening on port 1234..." << std::endl;

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