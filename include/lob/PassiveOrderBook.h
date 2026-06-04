#pragma once
#include <vector>
#include "Order.h"
#include "OrderPool.h"

template<typename T>
concept TradeListenerConcept = requires(T t, uint64_t id, int32_t p, uint32_t q, Side s, RejectReason r) {
    { t.onTrade(id, id, p, q) };
    { t.onOrderAdded(id, p, q, s) };
    { t.onOrderCancelled(id) };
    { t.onOrderExecuted(id, q) };
    { t.onOrderRejected(id, r) };
    { t.onOrderBookUpdate(p, q, s) };
};


template<TradeListenerConcept ListenerT>
class PassiveOrderBook {
private:

    struct Level {
        int32_t head = -1;
        int32_t tail = -1;
        uint32_t totalVolume = 0;
    };

    OrderPool pool;

    // We assume prices between 0 and 100,000.
    static constexpr size_t MAX_PRICE = 100000;
    std::vector<Level> bids;
    std::vector<Level> asks;

    std::vector<int32_t> orderIndexLookup;

    int32_t minAskPrice = MAX_PRICE;
    int32_t maxBidPrice = 0;

    ListenerT& listener;

public:

    PassiveOrderBook(ListenerT& l):
          listener(l), 
          pool(1000000),
          bids(MAX_PRICE + 1),
          asks(MAX_PRICE + 1),
          orderIndexLookup(10000000, -1)
    {};

    void onAddOrder(uint64_t id, int32_t price, uint32_t quantity, Side side) {
        if (id >= orderIndexLookup.size()) [[unlikely]] return;

        int32_t idx = pool.allocate(id, price, quantity, side);

        if (idx == -1) [[unlikely]] {
            listener.onOrderRejected(id, RejectReason::SystemFull); 
            return;
        }
        
        std::vector<Level>& bookSide = (side == Side::Buy) ? bids: asks;
        Level& level = bookSide[price];

        if (level.head == -1) {
            level.head = idx;
            level.tail = idx;
        }
        else {
            pool.get(level.tail).next = idx;
            pool.get(idx).prev = level.tail;
            level.tail = idx;
        }

        level.totalVolume += quantity;
        orderIndexLookup[id] = idx;

        if(side == Side::Buy) {
            if(price > maxBidPrice) {
                maxBidPrice = price;
            }
        }
        else {
            if (price < minAskPrice) {
                minAskPrice = price;
            }
        }

        listener.onOrderAdded(id, price, quantity, side);
        listener.onOrderBookUpdate(price, level.totalVolume, side);
    }

    void onCancelOrder(uint64_t orderId) {
        if (orderId >= orderIndexLookup.size()) [[unlikely]] return;

        int32_t idx = orderIndexLookup[orderId];

        if (idx == -1) [[unlikely]] {
            listener.onOrderRejected(orderId, RejectReason::OrderNotFound);
            return;
        }

        Order& order = pool.get(idx);
        std::vector<Level>& bookSide = (order.side == Side::Buy) ? bids: asks;
        Level& level = bookSide[order.price];

        level.totalVolume -= order.quantity;

        listener.onOrderCancelled(order.id);

        listener.onOrderBookUpdate(order.price, level.totalVolume, order.side);
        
        removeOrder(order, level, idx);
    }

    void onOrderExecuted(uint64_t orderId, uint32_t executedQty) {
        if (orderId >= orderIndexLookup.size()) [[unlikely]] return;

        int32_t idx = orderIndexLookup[orderId];

        if (idx == -1) [[unlikely]] {
            listener.onOrderRejected(orderId, RejectReason::OrderNotFound);
            return;
        }

        Order& order = pool.get(idx);

        uint32_t actualExecuted = std::min(order.quantity, executedQty);

        std::vector<Level>& bookSide = (order.side == Side::Buy) ? bids: asks;
        Level& level = bookSide[order.price];

        order.quantity -= actualExecuted;
        level.totalVolume -= actualExecuted;

        listener.onOrderExecuted(orderId, actualExecuted);

        listener.onOrderBookUpdate(order.price, level.totalVolume, order.side);

        if (order.quantity == 0) {
            removeOrder(order, level, idx);
        }
    }

    void printBook() {
        std::cout << "--- ASKS ---" << std::endl;
        for (int32_t p = MAX_PRICE; p >= 0; --p) {
            if (asks[p].totalVolume > 0) {
                std::cout << p << "\t|" << asks[p].totalVolume << std::endl;
            }
        }

        std::cout << "--- BIDS ---"<< std::endl;
        for (int32_t p = MAX_PRICE; p>=0; --p) {
            if (bids[p].totalVolume > 0) {
                std::cout << p << "\t|" << bids[p].totalVolume << std::endl;
            }
        }
        std::cout << "------------" << std::endl;
    }

private:
    inline void removeOrder(Order& order, Level& level, int32_t idx) {

        if (order.prev != -1) {
            pool.get(order.prev).next = order.next;
        }
        else {
            level.head = order.next;
        }
        
        if (order.next != -1) {
            pool.get(order.next).prev = order.prev;
        }
        else {
            level.tail = order.prev;
        }

        orderIndexLookup[order.id] = -1;

        if (order.side == Side::Buy) {
            if (order.price == maxBidPrice && level.totalVolume == 0) {
                while (maxBidPrice > 0 && bids[maxBidPrice].totalVolume == 0) {
                    --maxBidPrice;
                }
            }
        }
        else {
            if (order.price == minAskPrice && level.totalVolume == 0) {
                while (minAskPrice < MAX_PRICE && asks[minAskPrice].totalVolume == 0) {
                    ++minAskPrice;
                }
            }
        }

        pool.deallocate(idx);
    }
};