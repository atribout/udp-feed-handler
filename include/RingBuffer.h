#pragma once
#include <atomic>
#include <vector>
#include <cstddef>
#include <new>

#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_destructive_interference_size;
#else
    constexpr size_t hardware_destructive_interference_size = 64;
#endif

template<typename T, size_t Size>
class RingBuffer {

    static_assert((Size & (Size -1)) == 0, "Size must be a power of 2");

private:
    static constexpr size_t mask = Size - 1;

    T buffer[Size];

    alignas(hardware_destructive_interference_size)
    std::atomic<size_t> head = {0};

    alignas(hardware_destructive_interference_size)
    std::atomic<size_t> tail = {0};

public:
    RingBuffer() {}

    bool push(const T& item)
    {
        const auto current_head = head.load(std::memory_order_relaxed);
        const auto current_tail = tail.load(std::memory_order_acquire);

        if(current_head - current_tail >= Size) 
        {
            return false;
        }

        buffer[current_head & mask] = item;

        head.store(current_head + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item)
    {
        const auto current_tail = tail.load(std::memory_order_relaxed);
        const auto current_head = head.load(std::memory_order_acquire);

        if(current_tail ==  current_head)
        {
            return false;
        }

        item = buffer[current_tail & mask];

        tail.store(current_tail + 1, std::memory_order_release);
        return true;
    }
};