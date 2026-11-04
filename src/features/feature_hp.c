/**
 * feature_hp.c - 示例 2：血量下限
 *
 * 演示：
 *   - 返回点 hook（能读返回值）
 *   - 改返回值
 *   - 类型正确（float）
 */

#include <string.h>

#include "example.h"
#include "zh/zh_layer.h"
#include "zh/zlog.h"

static volatile unsigned long g_fixed = 0;
static volatile float          g_lastHp = 0.0f;
static const char* g_usedFunc = NULL;

/* ══════════════════════════════════════════════════════════
 *  回调（返回点）
 *
 *  ★ 返回点处的 x0 是返回值，不是 this
 *  ★ 不能 cancel（会破坏栈帧）
 * ══════════════════════════════════════════════════════════ */

static void on_get_health(ii_ctx* c, void* user)
{
    (void)user;

    float hp = zh_get_ret_float(c);
    __atomic_store_n(&g_lastHp, hp, __ATOMIC_RELAXED);

    /* 血量低于阈值且还没死 → 补到目标值 */
    if (hp > 0.0f && hp < (float)g_cfg.hpThreshold) {
        zh_set_ret_float(c, (float)g_cfg.hpTarget);
        __atomic_add_fetch(&g_fixed, 1, __ATOMIC_RELAXED);
    }
}

/* ══════════════════════════════════════════════════════════
 *  安装 / 卸载
 * ══════════════════════════════════════════════════════════ */

void feature_hp_setup(void)
{
    static const char* const candidates[] = {
        "Actor_getHealth",
        "Actor_getHealthFloat",
        "Actor_getMaxHealth",
        NULL
    };

    for (int i = 0; candidates[i]; ++i) {
        const char* fn = candidates[i];
        unsigned long addr = zh_find(fn);
        if (!addr) continue;

        /* 检查返回类型是不是浮点 —— 类型不对会改坏 */
        const char* tn = ii_ret_type_name(addr);
        int kind = ii_ret_kind(addr);

        if (kind != 4 && kind != 5) {
            LOG_W(LOG_HOOK, "hp: %s 返回类型 %s (kind=%d)，不是浮点，跳过",
                  fn, tn ? tn : "?", kind);
            continue;
        }

        /* 挂所有返回点 —— 否则某些路径漏掉 */
        int rc = ii_install_at_return(fn, -1, on_get_health, NULL);
        if (rc == 0) {
            g_usedFunc = fn;
            LOG_I(LOG_HOOK, "hp: 已挂 %s 全部 %d 个返回点（%s）",
                  fn, ii_retpoint_count(fn), tn);
            return;
        }

        LOG_W(LOG_HOOK, "hp: %s 返回点安装失败 rc=%d (%s)",
              fn, rc, ii_strerror(rc));
    }

    LOG_E(LOG_HOOK, "hp: 没有可用的血量函数");
}

void feature_hp_teardown(void)
{
    if (!g_usedFunc) return;

    zh_unhook(g_usedFunc);
    g_usedFunc = NULL;

    LOG_I(LOG_HOOK, "hp: 已卸载（补血 %lu 次，最后血量 %.1f）",
          __atomic_load_n(&g_fixed, __ATOMIC_RELAXED),
          (double)__atomic_load_n(&g_lastHp, __ATOMIC_RELAXED));
}

/* ══════════════════════════════════════════════════════════
 *  供 worker 查询
 * ══════════════════════════════════════════════════════════ */

unsigned long feature_hp_fix_count(void)
{
    return __atomic_load_n(&g_fixed, __ATOMIC_RELAXED);
}

float feature_hp_last(void)
{
    return __atomic_load_n(&g_lastHp, __ATOMIC_RELAXED);
}
