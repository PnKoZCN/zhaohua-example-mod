/**
 * entry.c - 模块入口
 *
 * ★★★ 最重要的一条规则 ★★★
 * constructor 在 dlopen 内部同步执行。如果在这里阻塞（sleep / 死循环 /
 * 等待条件），会冻结调用 dlopen 的线程。如果那个线程是游戏主线程，
 * 整个游戏卡死 → ANR → 闪退。
 *
 * 所以：constructor 只做一件事 —— 开线程，然后立刻返回。
 */

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "example.h"
#include "zh/zh_layer.h"
#include "zh/zlog.h"

/* ══════════════════════════════════════════════════════════
 *  全局状态
 * ══════════════════════════════════════════════════════════ */

mod_config_t g_cfg;

static volatile int g_running = 0;
static pthread_t    g_thread;
static int          g_threadStarted = 0;

/* 默认配置 */
void mod_config_defaults(void)
{
    memset(&g_cfg, 0, sizeof g_cfg);
    g_cfg.enableTick     = 1;
    g_cfg.enableHp       = 1;
    g_cfg.enableLog      = 0;
    g_cfg.enableMulti    = 0;
    g_cfg.hpThreshold    = 10;
    g_cfg.hpTarget       = 20;
    g_cfg.logFuncIdx     = 0;
    g_cfg.statIntervalMs = 5000;
}

/* ══════════════════════════════════════════════════════════
 *  初始化 / 清理
 * ══════════════════════════════════════════════════════════ */

int mod_init(void)
{
    mod_config_defaults();
    mod_config_load("/sdcard/Download/example.cfg");

    /* 引擎初始化（幂等 —— 若别的模块已初始化，返回 ZH_E_ALREADY） */
    int rc = zh_layer_init_default();
    if (rc != ZH_OK && !zh_layer_ready()) {
        LOG_E(LOG_INIT, "引擎初始化失败 rc=%d (%s)", rc, ii_strerror(rc));
        LOG_I(LOG_INIT, "  %s", ii_strhint(rc));
        return -1;
    }

    /* 标记所有者 —— 卸载时只清自己的 */
    zh_set_owner(MOD_NAME);

    char st[160];
    zh_layer_status(st, sizeof st);
    LOG_I(LOG_INIT, "═══ %s v%s ═══", MOD_NAME, MOD_VERSION);
    LOG_I(LOG_INIT, "引擎: %s", st);

    /* 按开关注册功能 */
    if (g_cfg.enableTick)  feature_tick_setup();
    if (g_cfg.enableHp)    feature_hp_setup();
    if (g_cfg.enableLog)   feature_log_setup();
    if (g_cfg.enableMulti) feature_multi_setup();

    LOG_I(LOG_INIT, "已安装 %d 个站点", ii_count());
    return 0;
}

void mod_shutdown(void)
{
    /* 逆序清理 */
    feature_multi_teardown();
    feature_log_teardown();
    feature_hp_teardown();
    feature_tick_teardown();

    /* 兜底：按 owner 清理漏网的 */
    int n = zh_unhook_owner(MOD_NAME);
    LOG_I(LOG_INIT, "卸载完成（owner 清理 %d 个）", n);

    /* 极少数情况：owner 也没覆盖到，全清 */
    if (ii_count() > 0) {
        LOG_W(LOG_INIT, "仍有 %d 个站点残留，强制全清", ii_count());
        zh_unhook_all();
    }
}

/* ══════════════════════════════════════════════════════════
 *  工作线程
 * ══════════════════════════════════════════════════════════ */

static void* worker(void* arg)
{
    (void)arg;

    if (mod_init() != 0) {
        LOG_E(LOG_INIT, "%s", "初始化失败，工作线程退出");
        return NULL;
    }

    while (g_running) {
        usleep(g_cfg.statIntervalMs * 1000);

        /* ★ 周期性状态报告：全在工作线程里做，不在回调里 */
        LOG_I(LOG_INIT, "存活: 站点=%d 崩溃隔离=%ld",
              ii_count(), zh_guard_caught());

        if (g_cfg.enableTick) {
            LOG_I(LOG_INIT, "  ticks +%lu", feature_tick_report());
        }
        if (g_cfg.enableHp) {
            LOG_I(LOG_INIT, "  hp: 补血 %lu 次，当前 %.1f",
                  feature_hp_fix_count(), (double)feature_hp_last());
        }
        if (g_cfg.enableLog) {
            feature_log_report(g_cfg.statIntervalMs);
        }
        if (g_cfg.enableMulti) {
            feature_multi_report();
        }
    }

    mod_shutdown();
    return NULL;
}

/* ══════════════════════════════════════════════════════════
 *  constructor / destructor
 * ══════════════════════════════════════════════════════════ */

__attribute__((constructor))
static void mod_constructor(void)
{
    g_running = 1;

    int rc = pthread_create(&g_thread, NULL, worker, NULL);

    if (rc != 0) {
        /* 起线程失败 —— 不阻塞，直接返回。Mod 不生效但不影响游戏 */
        return;
    }

    pthread_detach(g_thread);
    g_threadStarted = 1;

    /* ★ 立刻返回，绝不在 constructor 里阻塞 */
}

__attribute__((destructor))
static void mod_destructor(void)
{
    g_running = 0;

    if (g_threadStarted) {
        /* 给工作线程一点时间走完 mod_shutdown */
        for (int i = 0; i < 100; ++i) {
            usleep(10000);   /* 最多 1 秒 */
            if (ii_count() == 0) break;
        }
    }

    /* 无论工作线程如何，最后兜底清理 */
    zh_unhook_owner(MOD_NAME);
    zh_guard_off();
}
