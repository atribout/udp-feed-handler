#include <cstdlib>
#include <emmintrin.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <atomic>
#include <cstring>
#include <print>
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
        std::println(stderr, "Error pinning thread to core {}", core_id);
    } else {
        std::println("Thread pinned to Core {}", core_id);
    }
}

void consumer_thread()
{
    pin_to_core(5);
    TSCClock::get().printCalibration();
    std::println("Engine started (waiting for data)...");

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

                std::println("--- STATS REPORT ---");
                std::println("Lat p50   : {} ns", p50);
                std::println("Lat p99   : {} ns", p99);
                std::println("Lat Max   : {} ns", maxLat);
                std::println("Queue Max : {} / {}", maxQueueDepth, BUFFER_SIZE);
                std::println("Packet Loss : {}", gapCount);

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

template<typename T>
concept PacketReceiverConcept = requires(T t, size_t& len) {
    { t.receive(len) } -> std::same_as<const char*>;
};

class PcapReceiver {
    std::ifstream file;
public:
    explicit PcapReceiver(const std::string& filename) : file(filename, std::ios::binary) {
        if (!file.is_open()) {
            std::println(stderr, "[PCAP] Failed to open");
        }
    }
    
    inline const char* receive(size_t& len) {
        return nullptr;
    }
};

class UdpMulticastReceiver {
private:
    const int sockfd;
    static constexpr int BATCH_SIZE = 32;
    static constexpr int BUF_LEN = 1024;

    struct mmsghdr msgs[BATCH_SIZE];
    struct iovec iovecs[BATCH_SIZE];
    char buffers[BATCH_SIZE][BUF_LEN];

    int current_batch_size = 0;
    int current_msg_idx = 0;

public:
    explicit UdpMulticastReceiver(uint16_t port) : sockfd(socket(AF_INET, SOCK_DGRAM, 0)) {
        
        if(sockfd < 0)  {
            std::println(stderr, "Socket creation failed");
            exit(EXIT_FAILURE);
        }

        struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        struct sockaddr_in servaddr;
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = INADDR_ANY;
        servaddr.sin_port = htons(port);

        if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        {
            std::println(stderr, "Bind failed");
            exit(EXIT_FAILURE);
        }  

        for (int i = 0; i < BATCH_SIZE; i++) {
            iovecs[i].iov_base = buffers[i];
            iovecs[i].iov_len = BUF_LEN;
            
            memset(&msgs[i], 0, sizeof(msgs[i]));
            msgs[i].msg_hdr.msg_iov = &iovecs[i];
            msgs[i].msg_hdr.msg_iovlen = 1;
        }
    }

    ~UdpMulticastReceiver() {
        if (sockfd > 0) close(sockfd);
    }

    UdpMulticastReceiver(const UdpMulticastReceiver&) = delete;
    UdpMulticastReceiver& operator=(const UdpMulticastReceiver&) = delete;
    UdpMulticastReceiver(UdpMulticastReceiver&&) = delete;
    UdpMulticastReceiver& operator=(UdpMulticastReceiver&&) = delete;
    
    inline const char* receive(size_t& out_len) {
        if (current_msg_idx >= current_batch_size) {
            current_batch_size = recvmmsg(sockfd, msgs, BATCH_SIZE, MSG_DONTWAIT, NULL);
            if (current_batch_size <= 0) {
                current_batch_size = 0;
                out_len = 0;
                return nullptr;
            }
            current_msg_idx = 0;
        }

        out_len = msgs[current_msg_idx].msg_len;
        return buffers[current_msg_idx++];
    }
};

template<PacketReceiverConcept ReceiverT>
class NetworkProducer {
private:
    ReceiverT& receiver;
public:
    explicit NetworkProducer(ReceiverT& recv) : receiver(recv) {}

    void run() {
        pin_to_core(4);
        std::println("Network thread listening...");

        while (running) {
            size_t len = 0;
            const char* packet_ptr = receiver.receive(len);

            if (!packet_ptr) {
                _mm_pause();
                continue;
            }
            
            if (len < sizeof(PacketHeader)) continue;
                const PacketHeader* header = reinterpret_cast<const PacketHeader*>(packet_ptr);
                
                QueueItem* slot = ringBuffer.claim();
                if(!slot) continue;

                slot->type = header->type;
                slot->seqNum = header->seqNum;

                if(header->type == MsgType::AddOrder && len >= sizeof(AddOrderMsg)) {
                    const AddOrderMsg* msg = reinterpret_cast<const AddOrderMsg*>(packet_ptr);
                    slot->id = msg->id;
                    slot->price = msg->price;
                    slot->quantity = msg->quantity;
                    slot->side = msg->side;
                    ringBuffer.publish();
                }
                else if(header->type == MsgType::CancelOrder && len >= sizeof(CancelOrderMsg))  {
                    const CancelOrderMsg* msg = reinterpret_cast<const CancelOrderMsg*>(packet_ptr);
                    slot->id = msg->id;
                    ringBuffer.publish();
                }
                else  {
                    _mm_pause();
                }
        }   
    }
};

int main(int argc, char* argv[])  {
    std::string mode = "live";
    if (argc > 1) {
        mode = argv[1];
    }

    std::thread consumer(consumer_thread);

    if (mode == "pcap") {
        std::println("=== Starting in REPLAY mode (PCAP) ===");
        PcapReceiver pcapRecv("nasdaq_sample.pcap");
        NetworkProducer<PcapReceiver> producer(pcapRecv);
        producer.run();
    }
    else {
        std::println("=== Starting in LIVE mode (UDP Multicast) ===");
        UdpMulticastReceiver udpRecv(1234);
        NetworkProducer<UdpMulticastReceiver> producer(udpRecv);
        producer.run();
    }
        
    consumer.join();
    return 0;
}
