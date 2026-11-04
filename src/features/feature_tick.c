/**
 * feature_tick.c - 示例 1：tick 计数
 *
 * 演示最小可用的 hook：
 *   - LIGHT 模式（最轻量）
 *   - 只做原子计数（不阻塞）
 *   - 在工作线程里定期汇报
 */

#include <string.h>

#include "example.h"
#include "zh/zh_layer.h"
#include "zh/zlog.h"

static volatile unsigned long g_ticks = 0;
static volatile unsigned long g_lastReported = 0;
static const char* g_usedFunc = NULL;

/* ══════════════════════════════════════════════════════════
 *  回调
 *
 *  ★ 这里在游戏线程上执行。只做 O(1) 的事。
 *  ★ 不能 sleep / fopen / malloc / printf
 * ══════════════════════════════════════════════════════════ */

static void on_tick_light(ii_ctx_light* c, void* user)
{
    (void)c; (void)user;
    __atomic_add_fetch(&g_ticks, 1, __ATOMIC_RELAXED);
}

/* FULL 模式版本（LIGHT 装不上时用） */
static void on_tick_full(ii_ctx* c, void* user)
{
    (void)c; (void)user;
    __atomic_add_fetch(&g_ticks, 1, __ATOMIC_RELAXED);
}

/* ══════════════════════════════════════════════════════════
 *  安装 / 卸载
 * ══════════════════════════════════════════════════════════ */

void feature_tick_setup(void)
{
    /*
     * 按优先级试多个候选名 —— 游戏更新后名字可能变。
     */
    static const char* const candidates[] = {
        "LocalPlayer_normalTick",
        "LocalPlayer_tick",
        "Player_tickWorld",
        "Level_tick",
        NULL
    };

    for (int i = 0; candidates[i]; ++i) {
        const char* fn = candidates[i];
        if (!zh_find(fn)) continue;

        /* 先试 LIGHT */
        int rc = zh_hook_light(fn, on_tick_light, NULL);
        if (rc == 0) {
            g_usedFunc = fn;
            LOG_I(LOG_HOOK, "tick: 已挂 %s（LIGHT）", fn);
            return;
        }

        /* LIGHT 装不上就退 FULL（多数情况下都能装上） */
        rc = zh_hook(fn, on_tick_full, NULL);
        if (rc == 0) {
            g_usedFunc = fn;
            LOG_I(LOG_HOOK, "tick: 已挂 %s（FULL）", fn);
            return;
        }

        /* 已有站点 → 追加回调 */
        if (rc == -2) {
            if (ii_add_callback(zh_find(fn), on_tick_full, NULL, 0) == 0) {
                g_usedFunc = fn;
                LOG_I(LOG_HOOK, "tick: 已追加到 %s", fn);
                return;
            }
        }

        LOG_W(LOG_HOOK, "tick: %s 安装失败 rc=%d (%s)",
              fn, rc, ii_strerror(rc));
    }

    LOG_E(LOG_HOOK, "tick: 没有可用的 tick 函数");
}

void feature_tick_teardown(void)
{
    if (!g_usedFunc) return;

    zh_unhook(g_usedFunc);
    g_usedFunc = NULL;

    LOG_I(LOG_HOOK, "tick: 已卸载（累计 %lu 次）",
          __atomic_load_n(&g_ticks, __ATOMIC_RELAXED));
}

/* ══════════════════════════════════════════════════════════
 *  供 worker 查询
 * ══════════════════════════════════════════════════════════ */

unsigned long feature_tick_report(void)
{
    unsigned long cur = __atomic_load_n(&g_ticks, __ATOMIC_RELAXED);
    unsigned long delta = cur - g_lastReported;
    g_lastReported = cur;
    return delta;
}
