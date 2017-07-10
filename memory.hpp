#pragma once
#include <cstddef>

struct Memory {
    char *bytes_first;
    size_t bytes_n;
};

struct MemoryArena {
    struct Memory memory;
    size_t free_i;
    size_t max_requested_i;
};

Memory alloc(size_t requested_bytes_n, MemoryArena *arena_);

Memory system_alloc(size_t requested_bytes_n);
void system_free(Memory memory);

Memory alloc(size_t requested_bytes_n, struct SystemAllocator *);
