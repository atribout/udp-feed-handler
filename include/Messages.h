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

struct cancelOrderMsg 
{
    MsgType type;
    uint64_t id;
};

#pragma pack(pop)
