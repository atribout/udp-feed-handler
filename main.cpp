#include <iostream>
#include <thread>
#include <atomic>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <immintrin.h>
#include <sched.h>
#include <pthread.h>

#include "OrderBook.h"
#include "Order.h"
#include "Listeners.h"
#include "include/Messages.h"
#include "include/RingBuffer.h"
#include "include/TSCClock.h"

constexpr size_t BUFFER_SIZE = 4096;
RingBuffer<QueueItem, BUFFER_SIZE> ringBuffer;
std::atomic<bool> running{true};

void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "Error pinning thread to core " << core_id << "\n";
    } else {
        std::cout << "Thread pinned to Core " << core_id << "\n";
    }
}

void consumer_thread()
{
    pin_to_core(5);
    TSCClock::get().printCalibration();
    std::cout << "Engine started (waiting for data)...\n";

    EmptyListener listener;
    OrderBook<EmptyListener> lob(listener);

    // We store the latencies to print the percentiles later
    std::vector<uint64_t> samples;
    samples.reserve(100000);

    uint64_t maxQueueDepth = 0;

    // For packet loss
    uint64_t lastSeqNum = 0;
    uint64_t gapCount = 0;

    unsigned int dummy;
    uint64_t start_cycles, end_cycles;

    while (running) 
    {
        size_t currentDepth = ringBuffer.getSize();
        if (currentDepth > maxQueueDepth) maxQueueDepth = currentDepth;

        QueueItem* item = ringBuffer.peek();
        if (item) 
        {
            if(item->seqNum <= lastSeqNum)
            {
                lastSeqNum = item->seqNum;
            }
            else if(lastSeqNum != 0 && item->seqNum != lastSeqNum + 1)
            {
                gapCount += (item->seqNum -lastSeqNum -1);
            }
            lastSeqNum = item->seqNum;

            start_cycles = __rdtscp(&dummy);
            // --- CRITICAL ZONE ---
            if(item->type == MsgType::AddOrder)
            {
                Side side = (item->side == 'B') ? Side::Buy : Side::Sell;
                Order newOrder(item->id, item->price, item->quantity, side);
                lob.submitOrder(newOrder);
            }
            else if (item->type == MsgType::CancelOrder)
            {
                lob.cancelOrder(item->id);
            }
            // -----------------------------------------

            end_cycles = __rdtscp(&dummy);

            ringBuffer.advance();

            uint64_t cycles = end_cycles - start_cycles;
            samples.push_back(cycles);

            if (samples.size() > 100000) {

                std::sort(samples.begin(), samples.end());

                double p50 = TSCClock::get().toNanos(samples[50000]);
                double p99 = TSCClock::get().toNanos(samples[99000]);
                double maxLat = TSCClock::get().toNanos(samples.back());

                std::cout << "--- STATS REPORT ---\n"
                          << "Lat p50   : " << p50 << " ns\n"
                          << "Lat p99   : " << p99 << " ns\n"
                          << "Lat Max   : " << maxLat << " ns\n"
                          << "Queue Max : " << maxQueueDepth << " / " << BUFFER_SIZE << "\n"
                          << "Packet Loss : " << gapCount << "\n";

                samples.clear();
                maxQueueDepth = 0;
            }
        } 
        else 
        {
            _mm_pause();
        }
    }
}

constexpr int BATCH_SIZE = 32;
constexpr int BUF_LEN = 1024;

int main()
{
    pin_to_core(4);
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

    struct mmsghdr msgs[BATCH_SIZE];
    struct iovec iovecs[BATCH_SIZE];
    char buffers[BATCH_SIZE][BUF_LEN];

    for (int i = 0; i < BATCH_SIZE; i++) {
        iovecs[i].iov_base = buffers[i];
        iovecs[i].iov_len = BUF_LEN;
        
        memset(&msgs[i], 0, sizeof(msgs[i]));
        msgs[i].msg_hdr.msg_iov = &iovecs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
    }

    std::cout << "Network thread listening with recvmmsg (Batch " << BATCH_SIZE << ")..." << std::endl;

    while (running)
    {
        int retval = recvmmsg(sockfd, msgs, BATCH_SIZE, MSG_DONTWAIT, NULL);

        if (retval > 0)
        {
            for (int i = 0; i < retval; i++)
            {
                int len = msgs[i].msg_len;
                char* packet_ptr = buffers[i];

                if (len < sizeof(PacketHeader)) continue;

                PacketHeader* header = reinterpret_cast<PacketHeader*>(packet_ptr);

                QueueItem* slot = ringBuffer.claim();
                if(!slot) continue;

                slot->type = header->type;
                slot->seqNum = header->seqNum;

                if(header->type == MsgType::AddOrder && len >= sizeof(AddOrderMsg))
                {
                    AddOrderMsg* msg = reinterpret_cast<AddOrderMsg*>(packet_ptr);
                    slot->id = msg->id;
                    slot->price = msg->price;
                    slot->quantity = msg->quantity;
                    slot->side = msg->side;
                    ringBuffer.publish();
                }
                else if(header->type == MsgType::CancelOrder && len >= sizeof(CancelOrderMsg))
                {
                    CancelOrderMsg* msg = reinterpret_cast<CancelOrderMsg*>(packet_ptr);
                    slot->id = msg->id;
                    ringBuffer.publish();
                }
                else
                {
                    _mm_pause();
                }
            }
        }
    }

    close(sockfd);
    consumer.join();
    return 0;
}
