/**
 * feature_log.c - 示例 3：调用统计
 *
 * 演示：
 *   - 只观察，不干预
 *   - 用原子计数器避免日志爆炸
 *   - 统计特定函数的调用频率
 */

#include <string.h>

#include "example.h"
#include "zh/zh_layer.h"
#include "zh/zlog.h"

#define MAX_STATS 8

typedef struct {
    const char* name;
    unsigned long addr;
    volatile unsigned long calls;
    unsigned long lastCalls;
} stat_entry;

static stat_entry g_stats[MAX_STATS];
static int g_nStats = 0;

/* ══════════════════════════════════════════════════════════
 *  回调
 * ══════════════════════════════════════════════════════════ */

static void on_call(ii_ctx* c, void* user)
{
    (void)c;
    stat_entry* e = (stat_entry*)user;
    if (!e) return;
    __atomic_add_fetch(&e->calls, 1, __ATOMIC_RELAXED);
}

/* ══════════════════════════════════════════════════════════
 *  安装 / 卸载
 * ══════════════════════════════════════════════════════════ */

void feature_log_setup(void)
{
    /* 要统计的函数（可按需改） */
    static const char* const targets[] = {
        "LocalPlayer_normalTick",
        "Actor_getHealth",
        "ItemStack_isNull",
        "Level_tick",
        "Actor_getPos",
        NULL
    };

    g_nStats = 0;

    for (int i = 0; targets[i] && g_nStats < MAX_STATS; ++i) {
        const char* fn = targets[i];
        unsigned long addr = zh_find(fn);
        if (!addr) continue;

        stat_entry* e = &g_stats[g_nStats];
        e->name = fn;
        e->addr = addr;
        e->calls = 0;
        e->lastCalls = 0;

        /* 只观察：返回值 NULL 表示走原目标 / 不改行为 */
        int rc = zh_hook(fn, on_call, e);
        if (rc == -2) {
            rc = ii_add_callback(addr, on_call, e, -100);   /* 最低优先级 */
        }

        if (rc == 0) {
            ++g_nStats;
            LOG_I(LOG_HOOK, "log: 统计 %s", fn);
        } else {
            LOG_W(LOG_HOOK, "log: %s 失败 rc=%d", fn, rc);
        }
    }

    if (g_nStats == 0) {
        LOG_E(LOG_HOOK, "log: 没有可统计的函数");
    }
}

void feature_log_teardown(void)
{
    for (int i = 0; i < g_nStats; ++i) {
        zh_unhook(g_stats[i].name);
    }
    g_nStats = 0;
    LOG_I(LOG_HOOK, "log: 已卸载");
}

/* ══════════════════════════════════════════════════════════
 *  供 worker 查询（在工作线程里调用，可以打日志）
 * ══════════════════════════════════════════════════════════ */

void feature_log_report(unsigned intervalMs)
{
    if (g_nStats == 0) return;

    double sec = (double)intervalMs / 1000.0;
    if (sec <= 0.0) sec = 1.0;

    LOG_I(LOG_HOOK, "── 调用统计（最近 %.1f 秒）──", sec);

    for (int i = 0; i < g_nStats; ++i) {
        stat_entry* e = &g_stats[i];
        unsigned long cur = __atomic_load_n(&e->calls, __ATOMIC_RELAXED);
        unsigned long delta = cur - e->lastCalls;
        e->lastCalls = cur;

        LOG_I(LOG_HOOK, "  %-32s %8lu 次 (%.1f/s)",
              e->name, delta, (double)delta / sec);
    }
}
