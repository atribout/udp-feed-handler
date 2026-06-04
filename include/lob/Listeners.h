#include <cstdint>
#include <vector>
#include <print>
#include "Order.h"

struct ConsoleListener 
{
    void onOrderAdded(uint64_t id, int32_t price, uint32_t qty, Side side) 
    {
        std::println("[ORDER] New {} {} @ {} (ID: {})", 
                     (side == Side::Buy ? "Buy" : "Sell"), qty, price, id);
    }

    void onOrderCancelled(uint64_t id) 
    {
        std::println("[CANCEL] Order {} removed", id);
    }

    void onOrderExecuted(uint64_t id, uint32_t qty) {
        std::println("[EXEC] Order {} passively executed for {} lots", id, qty);
    }

    void onOrderRejected(uint64_t id, RejectReason reason)
    {
        std::println("[REJ] Order {} rejected (Reason: {})", id, static_cast<int>(reason));
    }

    void onTrade(uint64_t aggId, uint64_t passId, int32_t price, uint32_t qty) 
    {
        std::println(">>> TRADE EXECUTE: {}@{} (Aggressor: {}, Passive: {})", 
                     qty, price, aggId, passId);
    }

    // --- PUBLIC FLOW ---
    void onOrderBookUpdate(int32_t price, uint32_t volume, Side side)
    {
        std::println("[MKT DATA] Price Level {} ({}) is now {}", 
                     price, (side == Side::Buy ? "Bid" : "Ask"), volume);
    }
};

struct EmptyListener
{
    void onOrderAdded(uint64_t, int32_t, uint32_t, Side) {}

    void onOrderCancelled(uint64_t) {}

    void onOrderExecuted(uint64_t, uint32_t) {}

    void onOrderRejected(uint64_t, RejectReason) {}

    void onTrade(uint64_t, uint64_t, int32_t, uint32_t) {}

    void onOrderBookUpdate(int32_t, uint32_t, Side) {}
};

struct VectorListener {
    struct TradeInfo 
    { 
        uint64_t aggId;
        uint64_t passId;
        int32_t price;
        uint32_t qty; 
    };

    std::vector<TradeInfo> trades;
    std::vector<uint64_t> cancelledIds;
    std::vector<uint64_t> rejectedIds;
    std::vector<RejectReason> rejectReasons;

    void onTrade(uint64_t aggId, uint64_t passId, int32_t price, uint32_t qty) 
    {
        trades.push_back({aggId, passId, price, qty});
    }

    void onOrderCancelled(uint64_t id) 
    {
        cancelledIds.push_back(id);
    }

    void onOrderExecuted(uint64_t id, uint32_t qty) {}

    void onOrderRejected(uint64_t id, RejectReason reason) 
    {
        rejectedIds.push_back(id);
        rejectReasons.push_back(reason);
    }
    
    void onOrderAdded(uint64_t, int32_t, uint32_t, Side) {}
    void onOrderBookUpdate(int32_t, uint32_t, Side) {}

    void clear() 
    {
        trades.clear();
        cancelledIds.clear();
        rejectedIds.clear();
        rejectReasons.clear();
    }
};