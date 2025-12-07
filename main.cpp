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

constexpr size_t BUFFER_SIZE = 4096;
RingBuffer<QueueItem, BUFFER_SIZE> ringBuffer;
std::atomic<bool> running{true};

void consumer_thread()
{
    std::cout << "Engine started (waiting for data)...\n";

    ConsoleListener listener;
    OrderBook<ConsoleListener> lob(listener);

    QueueItem msg;

    while(running)
    {
        if (ringBuffer.pop(msg))
        {
            if(msg.type == MsgType::AddOrder)
            {
                Side side = (msg.side == 'B') ? Side::Buy : Side::Sell;
                Order newOrder(msg.id, msg.price, msg.quantity, side);
                lob.submitOrder(newOrder);
            }
            else if (msg.type == MsgType::AddOrder)
            {
                lob.cancelOrder(msg.id);
            }
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

            QueueItem item;
            item.type = type;

            if(type == MsgType::AddOrder)
            {
                if (n < sizeof(AddOrderMsg)) continue;

                AddOrderMsg* msg = reinterpret_cast<AddOrderMsg*>(buffer);
                item.id = msg->id;
                item.price = msg->price;
                item.quantity = msg->quantity;
                item.side = msg->side;

                if(!ringBuffer.push(item))
                {
                    std::cerr << "Buffer FULL (Add)\n";
                }
            }
            else if(type == MsgType::CancelOrder)
            {
                if (n < sizeof(CancelOrderMsg)) continue;

                CancelOrderMsg* msg = reinterpret_cast<CancelOrderMsg*>(buffer);
                item.id = msg->id;

                if(!ringBuffer.push(item))
                {
                    std::cerr << "Buffer FULL (Cancel)\n";
                }
            }


        }
    }

    close(sockfd);
    consumer.join();
    return 0;
}