#pragma once

#include "memory.hpp"

template <typename T, typename Allocator = SystemAllocator>
Memory
alloc_many_and_assign(T **pointer_to_first,
                      size_t n,
                      Allocator *a = static_cast<SystemAllocator *>(nullptr))
{
    Memory block = alloc((sizeof **pointer_to_first) * n, a);
    *pointer_to_first = reinterpret_cast<T *>(block.bytes_first);
    return block;
}




