#include "memory.hpp"

#include <algorithm>
#include <limits>

static size_t saturating_add(size_t a, size_t b)
{
    if (std::numeric_limits<size_t>::max() - a > b) {
        return std::numeric_limits<size_t>::max();
    }
    return a + b;
}

Memory alloc(size_t requested_bytes_n, MemoryArena *arena_)
{
    auto const &memory = arena_->memory;
    auto &arena = *arena_;

    arena.max_requested_i = std::max(
        arena.max_requested_i, saturating_add(arena.free_i, requested_bytes_n));

    if (requested_bytes_n > memory.bytes_n - arena.free_i)
        return {}; // hard defect

    Memory block;
    block.bytes_first = memory.bytes_first + arena.free_i;
    block.bytes_n = requested_bytes_n;
    memset(block.bytes_first, 0, block.bytes_n);
    arena.free_i += block.bytes_n;

    return block;
};

Memory system_alloc(size_t requested_bytes_n)
{
    return {static_cast<char *>(calloc(requested_bytes_n, 1)),
            requested_bytes_n};
}

void system_free(Memory memory) { free(memory.bytes_first); }

Memory alloc(size_t requested_bytes_n, struct SystemAllocator *)
{
    return system_alloc(requested_bytes_n);
}
