#pragma once
#include <cstdint>
#include <bit>
#include <array>
#include <cstddef>

class BboBitset {
private:
    static constexpr int32_t MAX_PRICE{100000};

    static constexpr size_t L0_SIZE = (MAX_PRICE / 64) + 1;
    static constexpr size_t L1_SIZE = (L0_SIZE / 64) + 1;

    uint64_t root{0ULL};

    std::array<uint64_t, L1_SIZE> l1{};
    std::array<uint64_t, L0_SIZE> l0{};

    constexpr int32_t highestBit(uint64_t mask) const noexcept {
        return static_cast<int32_t>(std::bit_width(mask)) - 1;
    }

    constexpr int32_t lowestBit(uint64_t mask) const noexcept {
        return static_cast<int32_t>(std::countr_zero(mask));
    }

public:
    constexpr void setPrice(int32_t price) noexcept {
        auto u_price = static_cast<uint32_t>(price);

        auto i0 = u_price / 64;
        auto b0 = u_price & 63;

        uint64_t bit0 = UINT64_C(1) << b0;

        if (!(l0[i0] & bit0)) {
            l0[i0] |= bit0;

            if (l0[i0] == bit0) {
                auto i1 = i0 / 64;
                auto b1 = i0 & 63;

                uint64_t bit1 = UINT64_C(1) << b1;
                if (!(l1[i1] & bit1)) {
                    l1[i1] |= bit1;
                    if (l1[i1] == bit1) {
                        root |= (UINT64_C(1) << i1);
                    }
                }
            }
        }
    }

    constexpr void clearPrice(int32_t price) noexcept {
        auto u_price = static_cast<uint32_t>(price);

        auto i0 = u_price / 64;
        auto b0 = u_price & 63;
        auto bit0 = UINT64_C(1) << b0;
        
        l0[i0] &= ~bit0;

        if (l0[i0] == 0) {
            auto i1 = i0 / 64;
            auto b1 = i0 & 63;
            auto bit1 = UINT64_C(1) << b1;

            l1[i1] &= ~bit1;
            if (l1[i1] == 0) {
                root &= ~(UINT64_C(1) << i1);
            }
        }
    }

    constexpr int32_t getBestBid() const noexcept {
        if(!root) [[unlikely]] return 0;

        auto i1 = highestBit(root);
        auto i0 = i1 * 64 + highestBit(l1[i1]);

        return i0 * 64 + highestBit(l0[i0]);
    }

    constexpr int32_t getBestAsk() const noexcept {
        if(!root) [[unlikely]] return MAX_PRICE;

        auto i1 = lowestBit(root);
        auto i0 = i1 * 64 + lowestBit(l1[i1]);

        return i0 * 64 + lowestBit(l0[i0]);
    }
};