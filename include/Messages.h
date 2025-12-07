#pragma once
#include <cstdint>

#pragma pack(push, 1)

enum class MsgType : uint8_t 
{
    AddOrder = 'A',
    CancelOrder = 'C'
};

struct AddOrderMsg 
{
    MsgType type;
    uint64_t id;
    int32_t price;
    uint32_t quantity;
    char side;
};

struct CancelOrderMsg 
{
    MsgType type;
    uint64_t id;
};

#pragma pack(pop)

struct alignas(32) QueueItem
{
    uint64_t id;
    int32_t price;     // Ignored if type == CancelOrder
    uint32_t quantity; // Ignored if type == CancelOrder
    MsgType type;
    char side;         // Ignored if type == CancelOrder
};
