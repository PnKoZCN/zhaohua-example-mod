/**
 * util.c - 常用工具
 */

#include <string.h>
#include <stdint.h>
#include <time.h>

#include "example.h"
#include "zh/zh_layer.h"
#include "zh/zlog.h"

/* ══════════════════════════════════════════════════════════
 *  尝试多个候选函数名
 * ══════════════════════════════════════════════════════════ */

/**
 * 实现放在 worker.c（因为要调 ii_add_callback）。
 * 这里只提供额外的变体。
 */

/* ══════════════════════════════════════════════════════════
 *  安全内存读
 * ══════════════════════════════════════════════════════════ */

int util_read_u32(void* base, size_t off, uint32_t* out)
{
    if (!base || (uintptr_t)base < 0x10000) return -1;
    if (out) *out = *(volatile uint32_t*)((char*)base + off);
    return 0;
}

int util_read_f32(void* base, size_t off, float* out)
{
    if (!base || (uintptr_t)base < 0x10000) return -1;
    if (out) *out = *(volatile float*)((char*)base + off);
    return 0;
}

int util_write_f32(void* base, size_t off, float v)
{
    if (!base || (uintptr_t)base < 0x10000) return -1;
    *(volatile float*)((char*)base + off) = v;
    return 0;
}

/* ══════════════════════════════════════════════════════════
 *  时间
 * ══════════════════════════════════════════════════════════ */

static inline int64_t now_ms_impl(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int64_t util_now_ms(void)
{
    return now_ms_impl();
}

/* ══════════════════════════════════════════════════════════
 *  字符串
 * ══════════════════════════════════════════════════════════ */

/**
 * 判断指针看起来像合法的可读地址（启发式，不保证）。
 */
int util_looks_like_ptr(uint64_t v)
{
    if (v == 0) return 0;
    if (v < 0x10000ULL) return 0;
    if (v > 0x800000000000ULL) return 0;
    if (v & 0x7) return 0;      /* 通常指针对齐 */
    return 1;
}
