#!/bin/bash
# 修复版快速启动脚本

set -e

echo "======================================"
echo "  NVIDIA 虚拟内存测试 - 修复版快速启动"
echo "======================================"
echo

# 1. 编译修复版测试程序
echo ">>> 步骤 1: 编译修复版测试程序..."
if gcc -o test_virtual_alloc_fixed test_virtual_alloc_fixed.c -std=c99 -Wall; then
    echo "✅ 编译成功！"
else
    echo "❌ 编译失败！"
    exit 1
fi
echo

# 2. 检查权限
echo ">>> 步骤 2: 检查 /dev/nvidiactl 权限..."
if [ -c /dev/nvidiactl ]; then
    ls -l /dev/nvidiactl
    echo "✅ 设备文件存在"
else
    echo "❌ /dev/nvidiactl 不存在！"
    exit 1
fi
echo

# 3. 说明
echo "======================================"
echo "  准备完成！"
echo "======================================"
echo
echo "运行测试程序："
echo "  sudo ./test_virtual_alloc_fixed"
echo
echo "📖 详细文档："
echo "  - ioctl 修复说明: IOCTL_FIX_GUIDE_zh.md"
echo "  - 测试指南: TESTING_GUIDE_zh.md"
echo "  - 快速参考: QUICK_REFERENCE.md"
echo
echo "❓ 如果仍然失败，请检查："
echo "  1. 是否使用 sudo 运行"
echo "  2. NVIDIA 驱动是否已加载（运行 nvidia-smi）"
echo "  3. 驱动版本是否支持（建议 >= 470.x）"
echo

# 4. 测试 nvidia-smi
echo ">>> 步骤 3: 测试 nvidia-smi..."
if nvidia-smi &>/dev/null; then
    echo "✅ nvidia-smi 可用"
    nvidia-smi --query-gpu=name,driver_version --format=csv,noheader
else
    echo "⚠️  nvidia-smi 不可用，但测试可以继续"
fi
echo

echo "✅ 所有准备工作完成！"
echo "现在可以运行: sudo ./test_virtual_alloc_fixed"
