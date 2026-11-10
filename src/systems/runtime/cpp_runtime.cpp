#include <stddef.h>
#include <stdint.h>

#include "fortify/systems/runtime/cpp_runtime.h"

namespace {
constexpr size_t kEarlyHeapSize = 1024 * 1024;
alignas(16) uint8_t g_early_heap[kEarlyHeapSize];
size_t g_early_heap_used = 0;

void *early_alloc(size_t size, size_t alignment) {
    if (alignment == 0) {
        alignment = 1;
    }

    uintptr_t base = reinterpret_cast<uintptr_t>(g_early_heap);
    uintptr_t current = base + g_early_heap_used;
    uintptr_t aligned = (current + (alignment - 1)) & ~(static_cast<uintptr_t>(alignment - 1));
    size_t next_used = static_cast<size_t>(aligned - base) + size;

    if (next_used > kEarlyHeapSize) {
        return nullptr;
    }

    g_early_heap_used = next_used;
    return reinterpret_cast<void *>(aligned);
}
}

void *operator new(size_t size) {
    if (size == 0) {
        size = 1;
    }
    return early_alloc(size, alignof(max_align_t));
}

void *operator new[](size_t size) {
    if (size == 0) {
        size = 1;
    }
    return early_alloc(size, alignof(max_align_t));
}

void operator delete(void *ptr) noexcept {
    (void)ptr;
}

void operator delete[](void *ptr) noexcept {
    (void)ptr;
}

void operator delete(void *ptr, size_t size) noexcept {
    (void)ptr;
    (void)size;
}

void operator delete[](void *ptr, size_t size) noexcept {
    (void)ptr;
    (void)size;
}

extern "C" void __cxa_pure_virtual(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

extern "C" void (*__init_array_start[])(void);
extern "C" void (*__init_array_end[])(void);

void cpp_runtime_init(void) {
    for (size_t i = 0; &__init_array_start[i] < __init_array_end; i++) {
        __init_array_start[i]();
    }
}
