#pragma once
#include <cstdint>
#include <x86intrin.h>

inline uint64_t rdtsc()
{
    unsigned int dummy;
    return __rdtscp(&dummy);
}