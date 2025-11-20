#!/bin/bash

# ================= 配置区域 =================
# 请根据你的 VPS 实际情况修改这里
# 1. 公网网卡名称 (使用 `ip link` 查看，通常是 eth0, ens3, ens33 等)
ETH_DEV="eth0"

# 2. 你的 Web Server 监听端口
PORT="8080"

# 3. TUN 设备编号 (对应 tun144)
TUN_NUM="144"

# 4. Minnow 运行的内部 IP (对应 scripts/tun.sh 中的默认配置)
MINNOW_IP="169.254.${TUN_NUM}.9"
# ===========================================

# 检查是否以 root 权限运行
if [ "$EUID" -ne 0 ]; then
  echo "请使用 sudo 运行此脚本"
  exit 1
fi

start() {
    echo "[*] 正在启动环境..."
    
    # 1. 启动 TUN 设备
    if ip link show "tun${TUN_NUM}" &> /dev/null; then
        echo "    - tun${TUN_NUM} 已存在，跳过创建"
    else
        ./scripts/tun.sh start "${TUN_NUM}"
        echo "    - tun${TUN_NUM} 启动成功"
    fi

    # 2. 添加 iptables 端口转发规则 (DNAT)
    # 检查规则是否已存在，避免重复添加
    check_rule=$(iptables -t nat -C PREROUTING -i "${ETH_DEV}" -p tcp --dport "${PORT}" -j DNAT --to-destination "${MINNOW_IP}:${PORT}" 2>&1)
    if [ $? -eq 0 ]; then
        echo "    - 端口转发规则已存在，跳过"
    else
        iptables -t nat -A PREROUTING -i "${ETH_DEV}" -p tcp --dport "${PORT}" -j DNAT --to-destination "${MINNOW_IP}:${PORT}"
        echo "    - 端口转发规则已添加: 外部 ${ETH_DEV}:${PORT} -> 内部 ${MINNOW_IP}:${PORT}"
    fi

    echo "[+] 环境启动完毕！现在可以运行 ./build/apps/web_server -l ${MINNOW_IP} ${PORT}"
}

stop() {
    echo "[*] 正在清理环境..."

    # 1. 删除 iptables 端口转发规则
    # 循环删除直到没有该规则（防止添加了多次）
    while iptables -t nat -D PREROUTING -i "${ETH_DEV}" -p tcp --dport "${PORT}" -j DNAT --to-destination "${MINNOW_IP}:${PORT}" 2>/dev/null; do
        echo "    - 已删除一条端口转发规则"
    done
    echo "    - 端口转发规则清理完毕"

    # 2. 停止 TUN 设备
    ./scripts/tun.sh stop "${TUN_NUM}"
    echo "    - tun${TUN_NUM} 已停止"

    echo "[-] 环境已清理干净"
}

status() {
    echo "=== 接口状态 ==="
    ip addr show "tun${TUN_NUM}" 2>/dev/null || echo "tun${TUN_NUM} 未运行"
    
    echo -e "\n=== 相关的 iptables 规则 ==="
    iptables -t nat -L PREROUTING -n -v | grep "${PORT}" || echo "无相关转发规则"
}

usage() {
    echo "用法: $0 {start|stop|restart|status}"
    echo "  start   : 启动 TUN 设备并添加 iptables 转发规则"
    echo "  stop    : 删除规则并关闭 TUN 设备"
    echo "  restart : 先 stop 后 start"
    echo "  status  : 查看当前状态"
    exit 1
}

# 主逻辑
case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart)
        stop
        start
        ;;
    status)
        status
        ;;
    *)
        usage
        ;;
esac