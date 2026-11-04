#!/system/bin/sh
# ══════════════════════════════════════════════════════════
#  build.sh —— 构建 example_mod
#
#  用法：
#    ./build.sh                       # 默认（动态依赖引擎）
#    ./build.sh static                # 静态打包引擎
#    ZH_ENGINE_DIR=/path ./build.sh   # 指定引擎路径
# ══════════════════════════════════════════════════════════
set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
cd "$SCRIPT_DIR"

# ── NDK ────────────────────────────────────────────────
if [ -z "$ANDROID_NDK" ]; then
    for cand in /opt/android-ndk-r27c /opt/android-ndk-r26d \
                "$HOME/android-ndk-r27c" "$ANDROID_NDK_ROOT"; do
        if [ -d "$cand" ]; then
            ANDROID_NDK="$cand"
            break
        fi
    done
fi

if [ -z "$ANDROID_NDK" ] || [ ! -d "$ANDROID_NDK" ]; then
    echo "错误：找不到 Android NDK" >&2
    echo "请设置 ANDROID_NDK=/path/to/ndk" >&2
    exit 1
fi

echo "NDK: $ANDROID_NDK"

# ── 引擎路径 ────────────────────────────────────────────
: "${ZH_ENGINE_DIR:=$SCRIPT_DIR/../zh_engine}"

if [ ! -f "$ZH_ENGINE_DIR/include/zh/zh_layer.h" ]; then
    echo "错误：找不到 Nexus 引擎" >&2
    echo "  期望: $ZH_ENGINE_DIR/include/zh/zh_layer.h" >&2
    echo "  设置: ZH_ENGINE_DIR=/path/to/zh_engine ./build.sh" >&2
    exit 1
fi

echo "引擎: $ZH_ENGINE_DIR"

# ── 链接模式 ────────────────────────────────────────────
MODE="shared"
if [ "$1" = "static" ]; then
    MODE="static"
fi
echo "链接模式: $MODE"

# ── 构建 ────────────────────────────────────────────────
BUILD_DIR="$SCRIPT_DIR/build"
rm -rf "$BUILD_DIR"

cmake -B "$BUILD_DIR" \
      -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-30 \
      -DCMAKE_BUILD_TYPE=Release \
      -DZH_ENGINE_DIR="$ZH_ENGINE_DIR" \
      -DZH_LINK_MODE="$MODE"

cmake --build "$BUILD_DIR" -j8

# ── 产物 ────────────────────────────────────────────────
SO=$(find "$BUILD_DIR" -name 'libexample_mod.so' | head -1)

if [ -z "$SO" ]; then
    echo "错误：没有产出 .so" >&2
    exit 1
fi

mkdir -p "$SCRIPT_DIR/out"
cp "$SO" "$SCRIPT_DIR/out/libexample_mod.so"

echo ""
echo "══════════════════════════════════════"
echo " 构建成功"
echo "══════════════════════════════════════"
ls -l "$SCRIPT_DIR/out/libexample_mod.so"
echo ""
echo "下一步："
echo "  ./deploy.sh          推到设备并注入"
echo "  ./scripts/logs.sh    看日志"
