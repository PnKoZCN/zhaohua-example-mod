# zhaohua-example-mod

**ZhaoHua Nexus** 示例 Mod —— 从零到可运行的完整参考工程。

对标 [FabricMC/fabric-example-mod](https://github.com/FabricMC/fabric-example-mod)：一个最小但完整的可编译、可注入、可调试的 Mod 工程，供你 fork 后改成自己的。

---

## 这是什么

一个 **原生 ARM64 共享库**，`dlopen` 注入网易 Minecraft 基岩版进程，通过 [ZhaoHua Nexus](https://github.com/PnKoZCN/zhaohua-nexus) 框架 hook 游戏函数。

不用改 APK，不用 root（运行期），用 C 写 Mod。

---

## 目录结构

```
zhaohua-example-mod/
├── CMakeLists.txt          构建脚本（NDK 交叉编译）
├── build.sh                一键构建 + 打包
├── deploy.sh               一键推到设备 + 注入
├── include/
│   └── example.h           你自己的头文件
├── src/
│   ├── entry.c             constructor/destructor 入口
│   ├── worker.c            工作线程（主逻辑）
│   ├── features/
│   │   ├── feature_tick.c         示例 1：tick 计数
│   │   ├── feature_hp.c           示例 2：血量下限
│   │   ├── feature_log.c          示例 3：调用统计
│   │   └── feature_multi.c        示例 4：多实例
│   └── util/
│       └── util.c          常用工具
├── config/
│   └── example.cfg         运行期配置
├── scripts/
│   ├── inject.sh           注入脚本
│   └── logs.sh             看日志
├── docs/
│   └── BUILD.md            详细构建说明
└── README.md
```

---

## 30 秒开始

```bash
# 1. 克隆
git clone https://github.com/PnKoZCN/zhaohua-example-mod.git
cd zhaohua-example-mod

# 2. 准备 NDK
export ANDROID_NDK=/opt/android-ndk-r27c

# 3. 构建
./build.sh

# 4. 部署（设备需已授权 root/shizuku 通道）
./deploy.sh
```

**产物**：`out/libexample_mod.so`（约 40 KB，不含 Nexus 引擎）。

---

## 它做什么

**四个示例功能，各自独立，可单独开关：**

| 功能 | 文件 | 说明 |
|---|---|---|
| `tick` | `feature_tick.c` | 统计每秒 tick 数，验证 hook 存活 |
| `hp` | `feature_hp.c` | 血量低于 10 时补到 20（改返回值） |
| `log` | `feature_log.c` | 统计某函数调用次数（只观察） |
| `multi` | `feature_multi.c` | 按实例（this 指针）分发 |

**在 `example.cfg` 里开关。**

---

## 依赖 Nexus 引擎

**本工程只产出你自己的 Mod 逻辑，hook 能力来自 `libzh_engine.so`。**

**两种链接方式：**

### 方式 A：动态依赖（推荐）

`CMakeLists.txt` 里只写头文件路径，运行时由引擎先加载：

```cmake
target_include_directories(example_mod PRIVATE ${ZH_ENGINE_INCLUDE})
# 不 link，符号在运行时从 libzh_engine.so 解析
```

**注入顺序**：先 `libzh_engine.so`，再 `libexample_mod.so`。

### 方式 B：静态打包

把引擎源码直接编进你的 `.so`（体积大但单文件）：

```cmake
add_subdirectory(${ZH_ENGINE_DIR} zh_engine STATIC)
target_link_libraries(example_mod PRIVATE zh_engine)
```

---

## 最小可用 Mod

**如果你只想要一个能跑的骨架：**

```c
// src/entry.c
#include <pthread.h>
#include <unistd.h>
#include "zh/zh_layer.h"
#include "zh/zlog.h"

static volatile int g_running = 1;

static void on_tick(ii_ctx* c, void* user) {
    (void)c; (void)user;
    // 你的逻辑（O(1)，不要阻塞）
}

static void* worker(void* arg) {
    (void)arg;

    if (zh_layer_init_default() != ZH_OK && !zh_layer_ready()) {
        return NULL;
    }

    zh_set_owner("example_mod");

    if (zh_hook("LocalPlayer_normalTick", on_tick, NULL) != 0) {
        LOG_E(LOG_HOOK, "hook 失败");
        return NULL;
    }

    LOG_I(LOG_HOOK, "example_mod 已启动");

    while (g_running) sleep(1);
    return NULL;
}

__attribute__((constructor))
static void mod_init(void) {
    pthread_t t;
    if (pthread_create(&t, NULL, worker, NULL) == 0) pthread_detach(t);
}

__attribute__((destructor))
static void mod_fini(void) {
    g_running = 0;
    zh_unhook_owner("example_mod");
}
```

**这就是全部必需结构。**

---

## 构建要求

| 项 | 要求 |
|---|---|
| NDK | r27c（或 r26+） |
| CMake | 3.20+ |
| 目标 ABI | `arm64-v8a` |
| minSdk | 30 |
| 引擎 | ZhaoHua Nexus v1.0 |

**详细说明见 [docs/BUILD.md](docs/BUILD.md)。**

---

## 注入

**需要一个把 `.so` 塞进游戏进程的通道。** 常见选择：

| 通道 | 说明 |
|---|---|
| KernelSU + `zhinf` | 本项目验证用的方式 |
| Zygisk 模块 | 需要模块开发 |
| Magisk + `nsenter` | 需要 root |
| 自建 ptrace 注入器 | 需要 root 或 debuggable |

**`scripts/inject.sh` 是参考实现：**

```bash
./scripts/inject.sh <libname.so>
```

**它做的事：**

1. 找游戏 PID
2. 找 `libzh_engine.so` 的 `mmap` / `dlopen` 地址
3. 调用注入器

**详细步骤见 [Nexus 构建注入文档](https://github.com/PnKoZCN/zhaohua-nexus/blob/main/docs/api/04-Getting-Started-Build-Inject.md)。**

---

## 看日志

```bash
./scripts/logs.sh
```

**等价于：**

```bash
adb shell su -c "tail -f /data/data/com.netease.x19/files/zhprobe/zhaohua.log"
```

---

## 三个铁律

**违反任意一条都会导致游戏 ANR 或闪退。**

### 1. 回调里不能阻塞

```c
// ✗ 错
static void on_tick(ii_ctx* c, void* user) {
    usleep(10000);          // 冻结游戏
    do_heavy_work();         // 同理
    FILE* f = fopen(...);    // 同理
}

// ✓ 对
static void on_tick(ii_ctx* c, void* user) {
    __atomic_store_n(&g_flag, 1, __ATOMIC_RELAXED);   // 只设标志
}
```

**引擎超 50 ms 会打 `[SLOW]` 告警，5 秒 ANR。**

### 2. `constructor` 里不能 sleep

```c
// ✗ 错 —— 会冻结注入线程
__attribute__((constructor))
static void init(void) {
    zh_layer_init_default();
    while (1) sleep(5);
}

// ✓ 对
__attribute__((constructor))
static void init(void) {
    pthread_t t;
    if (pthread_create(&t, NULL, worker, NULL) == 0) pthread_detach(t);
    // 立刻返回
}
```

### 3. 库卸载前必须清理

```c
// ✓ 必须有
__attribute__((destructor))
static void fini(void) {
    zh_unhook_owner("example_mod");
}
```

**否则 `dlclose` 后站点跳到野内存。**

---

## 许可

MIT。仅供学习研究。

---

## 相关链接

- **框架源码**：[PnKoZCN/zhaohua-nexus](https://github.com/PnKoZCN/zhaohua-nexus)
- **API 手册**：[zhaohua-nexus/docs/api](https://github.com/PnKoZCN/zhaohua-nexus/tree/main/docs/api)
- **Fabric Mixin 文档**：[SpongePowered/Mixin wiki](https://github.com/SpongePowered/Mixin/wiki)
