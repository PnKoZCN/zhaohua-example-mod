#!/system/bin/sh
# ══════════════════════════════════════════════════════════
#  scripts/logs.sh —— 实时看探针日志
#
#  用法：
#    ./scripts/logs.sh            # tail -f
#    ./scripts/logs.sh cat        # 全部打印
#    ./scripts/logs.sh err        # 只看 ERROR
#    ./scripts/logs.sh mx         # 只看退出前
# ══════════════════════════════════════════════════════════
set -e

LOG="/data/data/com.netease.x19/files/zhprobe/zhaohua.log"
MODE="${1:-follow}"

if ! su -c "test -f $LOG" 2>/dev/null; then
    echo "日志文件不存在: $LOG" >&2
    echo "Mod 可能还没成功初始化。" >&2
    exit 1
fi

case "$MODE" in
    cat)
        su -c "cat $LOG"
        ;;
    err)
        su -c "grep -E '\\[E\\]|\\[SLOW\\]|FATAL' $LOG"
        ;;
    mx)
        su -c "cat $LOG" | tail -100
        ;;
    *)
        su -c "tail -f $LOG"
        ;;
esac
