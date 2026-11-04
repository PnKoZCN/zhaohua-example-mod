/**
 * feature_multi.c - 示例 4：多实例（按 this 指针分发）
 *
 * 演示：
 *   - ii_multi_add：给特定实例绑回调
 *   - 与通用回调共存
 *   - 实例释放时清理
 *
 * 这是 Nexus 相对传统 inline hook 的核心优势：
 * 同一个函数地址，不同实例可以有不同行为。
 */

#include <string.h>

#include "example.h"
#include "zh/zh_layer.h"
#include "zh/zlog.h"

#define MAX_TRACKED 32

typedef struct {
    void* ptr;                  /* 实例指针 */
    volatile unsigned long ticks;
    int   alive;
} tracked_t;

static tracked_t g_tracked[MAX_TRACKED];
static int g_nTracked = 0;
static unsigned long g_addr = 0;
static volatile unsigned long g_genericTicks = 0;

/* ══════════════════════════════════════════════════════════
 *  回调
 * ══════════════════════════════════════════════════════════ */

/* 特定实例：key 匹配上才执行 */
static void on_specific(ii_ctx* c, void* user)
{
    (void)c;
    tracked_t* t = (tracked_t*)user;
    if (!t || !t->alive) return;
    __atomic_add_fetch(&t->ticks, 1, __ATOMIC_RELAXED);
}

/* 通用：所有实例都执行 */
static void on_generic(ii_ctx* c, void* user)
{
    (void)c; (void)user;
    __atomic_add_fetch(&g_genericTicks, 1, __ATOMIC_RELAXED);
}

/* ══════════════════════════════════════════════════════════
 *  跟踪管理（在工作线程或回调里调用都可以）
 * ══════════════════════════════════════════════════════════ */

/**
 * 开始跟踪一个实例。
 *
 * 例：当玩家的 Level 对象确定后，把玩家指针传进来，
 *     之后 on_specific 只对那个玩家触发。
 */
int feature_multi_track(void* instance)
{
    if (!instance || g_nTracked >= MAX_TRACKED || !g_addr) return -1;

    /* 已经在跟踪？ */
    for (int i = 0; i < g_nTracked; ++i) {
        if (g_tracked[i].ptr == instance && g_tracked[i].alive) return 0;
    }

    tracked_t* t = &g_tracked[g_nTracked++];
    t->ptr = instance;
    t->ticks = 0;
    t->alive = 1;

    int rc = ii_multi_add(g_addr, instance, on_specific, t, 0);
    if (rc != 0) {
        t->alive = 0;
        --g_nTracked;
        LOG_W(LOG_MULTI, "multi: 注册 %p 失败 rc=%d", instance, rc);
        return -1;
    }

    LOG_I(LOG_MULTI, "multi: 开始跟踪 %p (槽 %d)", instance, g_nTracked - 1);
    return 0;
}

/**
 * 停止跟踪（对象即将被释放时调用）。
 *
 * ★ 重要：对象 delete 后 key 地址可能被复用，
 *   你的回调会跑到新对象上。所以析构时要移除。
 */
void feature_multi_untrack(void* instance)
{
    if (!instance || !g_addr) return;

    for (int i = 0; i < g_nTracked; ++i) {
        if (g_tracked[i].ptr != instance) continue;

        ii_multi_remove(g_addr, instance, on_specific, &g_tracked[i]);
        g_tracked[i].alive = 0;
        LOG_I(LOG_MULTI, "multi: 停止跟踪 %p", instance);
        return;
    }
}

/* ══════════════════════════════════════════════════════════
 *  安装 / 卸载
 * ══════════════════════════════════════════════════════════ */

void feature_multi_setup(void)
{
    const char* fn = "Actor_normalTick";
    if (!zh_find(fn)) fn = "LocalPlayer_normalTick";
    if (!zh_find(fn)) {
        LOG_E(LOG_MULTI, "multi: 找不到可用的 tick 函数");
        return;
    }

    g_addr = zh_find(fn);

    /* 先建站点：通用回调 */
    int rc = zh_hook(fn, on_generic, NULL);
    if (rc != 0 && rc != -2) {
        LOG_E(LOG_MULTI, "multi: 建站点失败 rc=%d (%s)", rc, ii_strerror(rc));
        return;
    }

    LOG_I(LOG_MULTI, "multi: 站点就绪 @0x%lx (%s)，可跟踪 %d 个实例",
          g_addr, fn, MAX_TRACKED);
}

void feature_multi_teardown(void)
{
    for (int i = 0; i < g_nTracked; ++i) {
        if (!g_tracked[i].alive) continue;
        ii_multi_clear(g_addr, g_tracked[i].ptr);
        g_tracked[i].alive = 0;
    }
    g_nTracked = 0;

    if (g_addr) {
        zh_unhook("Actor_normalTick");
        zh_unhook("LocalPlayer_normalTick");
        g_addr = 0;
    }

    LOG_I(LOG_MULTI, "multi: 已卸载");
}

/* ══════════════════════════════════════════════════════════
 *  供 worker 查询
 * ══════════════════════════════════════════════════════════ */

void feature_multi_report(void)
{
    LOG_I(LOG_MULTI, "── 多实例状态 ──");
    LOG_I(LOG_MULTI, "  通用回调: %lu 次",
          __atomic_load_n(&g_genericTicks, __ATOMIC_RELAXED));

    for (int i = 0; i < g_nTracked; ++i) {
        if (!g_tracked[i].alive) continue;
        LOG_I(LOG_MULTI, "  [%d] %p: %lu 次",
              i, g_tracked[i].ptr,
              __atomic_load_n(&g_tracked[i].ticks, __ATOMIC_RELAXED));
    }
}
