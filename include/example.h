/**
 * example_mod - 公共头文件
 */

#ifndef EXAMPLE_H
#define EXAMPLE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════
 *  模块信息
 * ══════════════════════════════════════════════════════════ */

#define MOD_NAME     "example_mod"
#define MOD_VERSION  "1.0.0"

/* ══════════════════════════════════════════════════════════
 *  配置
 * ══════════════════════════════════════════════════════════ */

typedef struct {
    int enableTick;      /* tick 计数 */
    int enableHp;        /* 血量下限 */
    int enableLog;       /* 调用统计 */
    int enableMulti;     /* 多实例演示 */

    int hpThreshold;     /* 血量低于此值补血 */
    int hpTarget;        /* 补到多少 */

    int logFuncIdx;      /* 保留字段（当前未使用） */

    int statIntervalMs;  /* 统计打印间隔 */
} mod_config_t;

extern mod_config_t g_cfg;

/* ══════════════════════════════════════════════════════════
 *  生命周期
 * ══════════════════════════════════════════════════════════ */

int  mod_init(void);
void mod_shutdown(void);

void mod_config_load(const char* path);
void mod_config_defaults(void);

/* ══════════════════════════════════════════════════════════
 *  功能注册
 * ══════════════════════════════════════════════════════════ */

void feature_tick_setup(void);
void feature_tick_teardown(void);
unsigned long feature_tick_report(void);

void feature_hp_setup(void);
void feature_hp_teardown(void);
unsigned long feature_hp_fix_count(void);
float feature_hp_last(void);

void feature_log_setup(void);
void feature_log_teardown(void);
void feature_log_report(unsigned intervalMs);

void feature_multi_setup(void);
void feature_multi_teardown(void);
int  feature_multi_track(void* instance);
void feature_multi_untrack(void* instance);
void feature_multi_report(void);

/* ══════════════════════════════════════════════════════════
 *  工具
 * ══════════════════════════════════════════════════════════ */

/* 尝试多个候选函数名，装第一个成功的 */
int  util_hook_first(const char* const* names, void* cb, void* user,
                     const char** outName);

/* 安全读写（地址合法性启发式检查） */
int  util_safe_read_u64(void* base, size_t off, uint64_t* out);
int  util_read_u32(void* base, size_t off, uint32_t* out);
int  util_read_f32(void* base, size_t off, float* out);
int  util_write_f32(void* base, size_t off, float v);
int  util_looks_like_ptr(uint64_t v);

/* 时间 */
int64_t util_now_ms(void);

/* 原子计数器 */
void          util_counter_inc(volatile unsigned long* p);
unsigned long util_counter_get(volatile unsigned long* p);
unsigned long util_counter_take(volatile unsigned long* p);

#ifdef __cplusplus
}
#endif

#endif /* EXAMPLE_H */
