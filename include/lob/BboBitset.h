#pragma once
#include <cstdint>
#include <bit>

class BboBitset {
private:
    static constexpr int MAX_PRICE = 100000;
    static constexpr int L0_SIZE = (MAX_PRICE / 64) + 1;
    static constexpr int L1_SIZE = (L0_SIZE / 64) + 1;

    uint64_t root = 0;
    uint64_t l1[L1_SIZE] = {0};
    uint64_t l0[L0_SIZE] = {0};

    constexpr int highestBit(uint64_t mask) const noexcept {
        return std::bit_width(mask) - 1;
    }

    constexpr int lowestBit(uint64_t mask) const noexcept {
        return std::countr_zero(mask);
    }

public:
    void setPrice(int price) {
        int i0 = price / 64;
        int b0 = price % 64;
        
        if ((l0[i0] & (1ULL << b0)) == 0) {
            l0[i0] |= (1ULL << b0);
            
            if (l0[i0] == (1ULL << b0)) {
                int i1 = i0 / 64;
                int b1 = i0 % 64;
                
                if ((l1[i1] & (1ULL << b1)) == 0) {
                    l1[i1] |= (1ULL << b1);
                    
                    if (l1[i1] == (1ULL << b1)) {
                        root |= (1ULL << i1);
                    }
                }
            }
        }
    }

    void clearPrice(int price) {
        int i0 = price / 64;
        int b0 = price % 64;
        
        l0[i0] &= ~(1ULL << b0);
        
        if (l0[i0] == 0) {
            int i1 = i0 / 64;
            int b1 = i0 % 64;
            l1[i1] &= ~(1ULL << b1);
            
            if (l1[i1] == 0) {
                root &= ~(1ULL << i1);
            }
        }
    }

    int getBestBid() const {
        if (root == 0) [[unlikely]] return 0;
        
        int r_bit = highestBit(root);
        int l1_bit = highestBit(l1[r_bit]);
        int i0 = (r_bit * 64) + l1_bit;
        int l0_bit = highestBit(l0[i0]);
        
        return (i0 * 64) + l0_bit;
    }

    int getBestAsk() const {
        if (root == 0) [[unlikely]] return MAX_PRICE;
        
        int r_bit = lowestBit(root);
        int l1_bit = lowestBit(l1[r_bit]);
        int i0 = (r_bit * 64) + l1_bit;
        int l0_bit = lowestBit(l0[i0]);
        
        return (i0 * 64) + l0_bit;
    }
};