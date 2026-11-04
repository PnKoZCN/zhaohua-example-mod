#!/system/bin/sh
# ══════════════════════════════════════════════════════════
#  deploy.sh —— 推到设备并注入
#
#  用法：
#    ./deploy.sh              推送 + 提示注入命令
#    ./deploy.sh push         只推送
#    ./deploy.sh clean        清理设备上的文件
# ══════════════════════════════════════════════════════════
set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
cd "$SCRIPT_DIR"

SO="out/libexample_mod.so"
PKG="com.netease.x19"
REMOTE="/data/local/tmp/zhtest"

# ── clean ───────────────────────────────────────────────
if [ "$1" = "clean" ]; then
    echo "清理 $REMOTE/libexample_mod.so"
    su -c "rm -f $REMOTE/libexample_mod.so"
    exit 0
fi

# ── 检查产物 ────────────────────────────────────────────
if [ ! -f "$SO" ]; then
    echo "错误：$SO 不存在，先运行 ./build.sh" >&2
    exit 1
fi

# ── 推送 ────────────────────────────────────────────────
echo "推送 $SO → $REMOTE/"
su -c "mkdir -p $REMOTE"
adb push "$SO" "$REMOTE/libexample_mod.so"
su -c "chmod 755 $REMOTE/libexample_mod.so"

# ── 配置 ────────────────────────────────────────────────
if [ -f "config/example.cfg" ]; then
    echo "推送配置"
    adb push "config/example.cfg" /sdcard/Download/example.cfg
fi

# ── 提示 ────────────────────────────────────────────────
PID=$(pidof "$PKG" 2>/dev/null || echo "")

echo ""
echo "══════════════════════════════════════"
echo " 推送完成"
echo "══════════════════════════════════════"

if [ -z "$PID" ]; then
    echo "游戏未运行。启动后执行："
    echo "  ./scripts/inject.sh libexample_mod.so"
else
    echo "游戏 PID: $PID"
    echo ""
    echo "注入："
    echo "  ./scripts/inject.sh libexample_mod.so"
fi
echo ""
echo "看日志："
echo "  ./scripts/logs.sh"
