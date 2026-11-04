#!/system/bin/sh
# ══════════════════════════════════════════════════════════
#  scripts/inject.sh —— 注入 .so 到游戏进程
#
#  用法：
#    ./scripts/inject.sh [库名]
#
#  默认库名：libexample_mod.so
#
#  前提：
#    1. 游戏已在运行
#    2. 设备已授权 root（KernelSU / Magisk）
#    3. Nexus 引擎已就位（见 detect 部分）
# ══════════════════════════════════════════════════════════
set -e

LIB_NAME="${1:-libexample_mod.so}"
PKG="com.netease.x19"

# ── 1. 找 PID ───────────────────────────────────────────
PID=$(pidof "$PKG" 2>/dev/null || echo "")

if [ -z "$PID" ]; then
    echo "错误：游戏未运行（$PKG）" >&2
    exit 1
fi

echo "游戏 PID: $PID"

# ── 2. 确认库已推送 ─────────────────────────────────────
LIB_PATH="/data/local/tmp/zhtest/$LIB_NAME"

if [ ! -f "$LIB_PATH" ]; then
    echo "错误：$LIB_PATH 不存在" >&2
    echo "先执行: ./deploy.sh" >&2
    exit 1
fi

echo "库文件: $LIB_PATH"

# ── 3. 找注入器 ─────────────────────────────────────────
INJECTOR="/data/local/tmp/zhtest/zhinf"

if [ ! -x "$INJECTOR" ]; then
    echo "错误：注入器不存在: $INJECTOR" >&2
    exit 1
fi

# ── 4. 找引擎地址 ───────────────────────────────────────
# 引擎（libzh_engine.so）必须先从外部 dlopen 进进程，
# 或者你的 Mod 静态打包了引擎。
#
# 这里假设引擎已加载（用 zhinf 先注入一次引擎）。
#
# 获取 mmap / dlopen 地址（每次启动会变，必须现查）。

LIBM="/data/app/~~*/com.netease.x19-*/lib/arm64/libminecraftpe.so"

ADDRS=$(su -c "
    cat /proc/$PID/maps 2>/dev/null | grep -m1 'libc.so' | head -1
" || true)

if [ -z "$ADDRS" ]; then
    echo "提示：无法自动获取 dlopen 地址"
    echo "请手工查询（见 docs/BUILD.md 的「注入」章节）"
    MMAP_ADDR="${MMAP_ADDR:-}"
    DLOPEN_ADDR="${DLOPEN_ADDR:-}"
else
    echo "libc 映射: $ADDRS"
fi

# ── 5. 注入 ─────────────────────────────────────────────
if [ -z "$MMAP_ADDR" ] || [ -z "$DLOPEN_ADDR" ]; then
    echo ""
    echo "需要手工提供地址："
    echo "  MMAP_ADDR=0x... DLOPEN_ADDR=0x... ./scripts/inject.sh"
    echo ""
    echo "获取方法（root）："
    echo "  su -c \"grep ' libc.so' /proc/$PID/maps\""
    echo "  从输出里算出 mmap 和 dlopen 偏移（基址 + 偏移）"
    exit 1
fi

echo "注入中: $LIB_NAME"
echo "  mmap   = $MMAP_ADDR"
echo "  dlopen = $DLOPEN_ADDR"

su -c "$INJECTOR $PID $LIB_NAME $MMAP_ADDR $DLOPEN_ADDR"

echo ""
echo "已注入。看日志："
echo "  ./scripts/logs.sh"
