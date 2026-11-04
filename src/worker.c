/**
 * worker.c - 主逻辑线程
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "example.h"
#include "zh/zh_layer.h"
#include "zh/zlog.h"

/* ══════════════════════════════════════════════════════════
 *  配置文件
 * ══════════════════════════════════════════════════════════ */

/**
 * 极简配置格式（每行 key=value，# 开头是注释）：
 *
 *   tick=1
 *   hp=1
 *   log=0
 *   multi=0
 *   hp_threshold=10
 *   hp_target=20
 *   stat_interval_ms=5000
 */
void mod_config_load(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) return;      /* 没有配置文件就用默认值，不报错 */

    char line[256];
    while (fgets(line, sizeof line, f)) {
        char* p = line;

        /* 去掉前导空白 */
        while (*p == ' ' || *p == '\t') ++p;

        /* 注释 / 空行 */
        if (*p == '#' || *p == '\n' || *p == '\0') continue;

        /* 去掉行尾换行 */
        size_t n = strlen(p);
        while (n > 0 && (p[n-1] == '\n' || p[n-1] == '\r')) p[--n] = '\0';

        char* eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        const char* key = p;
        const char* val = eq + 1;

        /* 去前后空白 */
        while (*val == ' ' || *val == '\t') ++val;
        char* end = (char*)key + strlen(key);
        while (end > key && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';

        int b = (val[0] == '1' || val[0] == 'y' || val[0] == 'Y' ||
                 val[0] == 't' || val[0] == 'T');
        int iv = atoi(val);

        if      (strcmp(key, "tick") == 0)             g_cfg.enableTick = b;
        else if (strcmp(key, "hp") == 0)               g_cfg.enableHp = b;
        else if (strcmp(key, "log") == 0)              g_cfg.enableLog = b;
        else if (strcmp(key, "multi") == 0)            g_cfg.enableMulti = b;
        else if (strcmp(key, "hp_threshold") == 0)     g_cfg.hpThreshold = iv;
        else if (strcmp(key, "hp_target") == 0)        g_cfg.hpTarget = iv;
        else if (strcmp(key, "log_func_idx") == 0)     g_cfg.logFuncIdx = iv;
        else if (strcmp(key, "stat_interval_ms") == 0) g_cfg.statIntervalMs = iv;
    }

    fclose(f);
    LOG_I(LOG_INIT, "已加载配置 %s", path);
}

/* ══════════════════════════════════════════════════════════
 *  工具
 * ══════════════════════════════════════════════════════════ */

int util_hook_first(const char* const* names, void* cb, void* user,
                    const char** outName)
{
    for (int i = 0; names[i]; ++i) {
        if (!zh_find(names[i])) continue;

        int rc = zh_hook(names[i], (ii_callback)cb, user);
        if (rc == 0) {
            if (outName) *outName = names[i];
            LOG_I(LOG_HOOK, "✓ %s", names[i]);
            return 0;
        }

        if (rc == -2) {
            /* 已有站点 —— 追加回调 */
            if (ii_add_callback(zh_find(names[i]), (ii_callback)cb, user, 0) == 0) {
                if (outName) *outName = names[i];
                LOG_I(LOG_HOOK, "✓ %s（追加）", names[i]);
                return 0;
            }
        }

        LOG_W(LOG_HOOK, "✗ %s rc=%d", names[i], rc);
    }
    return -1;
}

int util_safe_read_u64(void* base, size_t off, uint64_t* out)
{
    if (!base) return -1;
    if ((uintptr_t)base < 0x10000) return -1;
    if (out) *out = *(volatile uint64_t*)((char*)base + off);
    return 0;
}

void util_counter_inc(volatile unsigned long* p)
{
    __atomic_add_fetch(p, 1, __ATOMIC_RELAXED);
}

unsigned long util_counter_get(volatile unsigned long* p)
{
    return __atomic_load_n(p, __ATOMIC_RELAXED);
}

unsigned long util_counter_take(volatile unsigned long* p)
{
    return __atomic_exchange_n(p, 0, __ATOMIC_RELAXED);
}
