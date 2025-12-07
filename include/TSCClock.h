#pragma once
#include <iostream>
#include <thread>
#include <chrono>
#include "Utils.h"

class TSCClock {
public:
    static TSCClock& get() {
        static TSCClock instance;
        return instance;
    }

    double toNanos(uint64_t cycles) const {
        return cycles * nanosPerCycle_;
    }

    double toSeconds(uint64_t cycles) const {
        return cycles * secondsPerCycle_;
    }

    void printCalibration() const {
        std::cout << "TSC Frequency: " << (1.0 / secondsPerCycle_ / 1e9) << " GHz\n";
        std::cout << "1 Cycle = " << nanosPerCycle_ << " ns\n";
    }

private:
    TSCClock() {
        calibrate();
    }

    void calibrate() {
        std::cout << "Calibrating TSC... (waiting 1 sec)\n";
        
        auto start_time = std::chrono::steady_clock::now();
        uint64_t start_cycles = rdtsc();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        auto end_time = std::chrono::steady_clock::now();
        uint64_t end_cycles = rdtsc();
        
        auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
        uint64_t cycle_diff = end_cycles - start_cycles;

        nanosPerCycle_ = (double)duration_ns / (double)cycle_diff;
        secondsPerCycle_ = nanosPerCycle_ * 1e-9;
    }

    double nanosPerCycle_;
    double secondsPerCycle_;
};