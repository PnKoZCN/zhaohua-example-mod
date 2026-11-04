# 详细构建说明

---

## 一、环境要求

| 项 | 版本 | 检查 |
|---|---|---|
| Android NDK | r26+ （推荐 r27c） | `ls $ANDROID_NDK/build/cmake/android.toolchain.cmake` |
| CMake | 3.20+ | `cmake --version` |
| 目标 ABI | arm64-v8a | — |
| minSdk | 30 | — |

**为什么必须用 NDK**：musl gcc 产出的 `.so` 无法被 Android bionic 的 `dlopen` 加载。

---

## 二、获得 Nexus 引擎

### 方式 1：克隆

```bash
git clone https://github.com/PnKoZCN/zhaohua-nexus.git zh_engine
```

**放在本工程的同级目录：**

```
parent/
├── zh_engine/         ← 引擎源码
└── zhaohua-example-mod/   ← 本工程
```

### 方式 2：用预编译

```bash
# 下载 libzh_engine.so + 头文件
mkdir -p zh_engine
cp -r /path/to/prebuilt/include zh_engine/
cp /path/to/prebuilt/libzh_engine.so zh_engine/
```

**然后在 `CMakeLists.txt` 里设 `ZH_ENGINE_DIR`。**

---

## 三、构建

### 3.1 快速

```bash
export ANDROID_NDK=/opt/android-ndk-r27c
./build.sh
```

### 3.2 手工

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-30 \
  -DCMAKE_BUILD_TYPE=Release \
  -DZH_ENGINE_DIR=../zh_engine \
  -DZH_LINK_MODE=shared

cmake --build build -j8
```

### 3.3 选项

| 选项 | 默认 | 说明 |
|---|---|---|
| `ZH_ENGINE_DIR` | `../zh_engine` | 引擎源码路径 |
| `ZH_LINK_MODE` | `shared` | `shared` / `static` |
| `BUILD_FEATURE_TICK` | ON | tick 计数示例 |
| `BUILD_FEATURE_HP` | ON | 血量示例 |
| `BUILD_FEATURE_LOG` | ON | 调用统计示例 |
| `BUILD_FEATURE_MULTI` | ON | 多实例示例 |

---

## 四、链接模式

### 4.1 `shared`（推荐）

**只包含头文件，符号在运行时从 `libzh_engine.so` 解析。**

**优点**：
- 你的 `.so` 只有 ~40 KB
- 多个 Mod 共享一个引擎
- 引擎升级不用重编 Mod

**要求**：注入顺序 —— 先引擎，后 Mod。

### 4.2 `static`

**引擎源码直接编进你的 `.so`。**

**优点**：
- 单文件，注入简单
- 无依赖

**缺点**：
- `.so` 约 900 KB
- 多个 Mod 会重复引擎代码（互相冲突）

**只适合"只有一个 Mod"的场景。**

---

## 五、注入

### 5.1 需要一个注入通道

| 通道 | 需要 root | 说明 |
|---|---|---|
| KernelSU + `zhinf` | 是 | 本项目采用 |
| Zygisk 模块 | 否（需模块） | 需要写模块 |
| Magisk + `nsenter` | 是 | 通用 |
| ptrace 注入器 | 是/可调试版 | 自己实现 |

### 5.2 KernelSU 方式（本项目验证）

**1. 编译注入器**

注入器是个 tiny ELF 程序：`ptrace` 挂起 → `mmap` 分配 → 写 `.so` 路径 → 远程调 `dlopen`。

**2. 推到设备**

```bash
adb push zhinf /data/local/tmp/zhtest/
su -c "chmod 755 /data/local/tmp/zhtest/zhinf"
```

**3. 查地址**

```bash
PID=$(pidof com.netease.x19)
su -c "grep ' libc.so' /proc/$PID/maps"
```

**输出**：

```
7e8b0f2000-7e8b1a8000 r-xp 00000000 fd:04 12345 /apex/com.android.runtime/lib64/bionic/libc.so
```

**基址 = `0x7e8b0f2000`**

**`mmap` / `dlopen` 的偏移从 `libc.so` 的 ELF 符号表读：**

```bash
aarch64-linux-gnu-readelf -sW libc.so | grep -E ' (mmap|dlopen)$'
```

**绝对地址 = 基址 + 偏移。**

**4. 注入**

```bash
su -c "/data/local/tmp/zhtest/zhinf $PID libexample_mod.so 0x7e8b587640 0x7e8b0fb01c"
```

**参数**：`<pid> <libname> <mmap_addr> <dlopen_addr>`

`zhinf` 自己找 `libexample_mod.so` 在 `/data/local/tmp/zhtest/` 下。

### 5.3 验证

```bash
su -c "grep libexample_mod /proc/$PID/maps"
```

**有输出 = 加载成功。**

```bash
./scripts/logs.sh
```

**看到 `═══ example_mod v1.0.0 ═══` = 初始化成功。**

---

## 六、常见问题

### Q：`dlopen` 返回 NULL

**原因**：
- 库名不对（要完整文件名 `libexample_mod.so`）
- 库路径不在 `DLOPEN` 能搜到的地方（必须绝对路径或 `/data/local/tmp/zhtest/` 下）
- 库依赖缺失（`shared` 模式下引擎没先加载）

**排查**：

```bash
su -c "cat /proc/$PID/maps | grep zh"
```

### Q：加载了但没日志

**检查**：

1. `constructor` 真的跑了吗（加 `write(2, "..." , n)` 直接写 stderr）
2. `constructor` 里有没有阻塞（`constructor` 卡住 → 日志线程起不来）
3. 日志路径可写吗

**最粗暴的验证**：

```c
__attribute__((constructor))
static void ctor(void) {
    write(2, "[example_mod] ctor entered\n", 27);
    ...
}
```

**stderr 会进 logcat。**

### Q：游戏卡死

**99% 是 `constructor` 阻塞**。

```bash
adb logcat | grep -i "ANR\|Input dispatching"
```

**看到 `Waited 5000ms for MotionEvent` → 就是它。**

### Q：`zh_hook` 返回 -1

**函数名不对。**

```c
// 枚举看看有哪些
for (int i = 0; i < zh_tbl_count(ZH_TBL_RETFUNC); i++)
    LOG_I(LOG_HOOK, "%s", zh_tbl_retfunc(i));
```

### Q：多个 Mod 冲突

**每个 Mod 必须**：

1. `zh_set_owner("唯一名")`
2. `destructor` 里 `zh_unhook_owner("唯一名")`

**引擎会通过 `poolreg.bin` 共享区协调。**

---

## 七、调试

### 7.1 logcat

```bash
adb logcat -s DEBUG:V
```

### 7.2 探针日志

```bash
./scripts/logs.sh
```

### 7.3 看映射

```bash
su -c "grep -E 'zh_engine|example_mod' /proc/$(pidof com.netease.x19)/maps"
```

### 7.4 反汇编你的库

```bash
$ANDROID_NDK/toolchains/llvm/prebuilt/*/bin/llvm-objdump -d out/libexample_mod.so
```

### 7.5 导出符号检查

```bash
$ANDROID_NDK/toolchains/llvm/prebuilt/*/bin/llvm-nm -D --defined-only out/libexample_mod.so
```

**应该只看到 `mod_constructor` / `mod_destructor`（其余都被 `-fvisibility=hidden` 隐藏）。**

---

## 八、发布

```bash
./build.sh static
strip out/libexample_mod.so
```

**`strip` 能减 60% 体积。**

**但保留 `constructor`/`destructor`（它们不是导出符号，不会被 strip）。**

---

## 九、参考

- [Nexus API 手册](https://github.com/PnKoZCN/zhaohua-nexus/tree/main/docs/api)
- [构建注入详解](https://github.com/PnKoZCN/zhaohua-nexus/blob/main/docs/api/04-Getting-Started-Build-Inject.md)
- [常见陷阱](https://github.com/PnKoZCN/zhaohua-nexus/blob/main/docs/api/52-Guide-Pitfalls.md)
