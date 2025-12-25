#!/bin/bash
# 快速编译和测试脚本

set -e  # 遇到错误立即退出

echo "========================================"
echo "  NVIDIA 虚拟内存分配测试 - 快速启动"
echo "========================================"
echo ""

# 检查是否在正确的目录
if [ ! -f "test_virtual_alloc.c" ]; then
    echo "错误: 找不到 test_virtual_alloc.c"
    echo "请确保在 /workspace 目录下运行此脚本"
    exit 1
fi

# 检查头文件
if [ ! -f "src/common/sdk/nvidia/inc/nvos.h" ]; then
    echo "错误: 找不到必需的头文件"
    echo "请确保 NVIDIA 驱动源码完整"
    exit 1
fi

# 编译
echo ">>> 步骤 1: 编译测试程序"
gcc -o test_virtual_alloc test_virtual_alloc.c \
    -std=c99 \
    -Wall -Wextra \
    -I./src/common/sdk/nvidia/inc \
    -I./src/nvidia/arch/nvalloc/unix/include

if [ $? -eq 0 ]; then
    echo "✅ 编译成功！"
else
    echo "❌ 编译失败，请检查错误信息"
    exit 1
fi

echo ""
echo ">>> 步骤 2: 设置执行权限"
chmod +x test_virtual_alloc
chmod +x monitor_memory.py
echo "✅ 权限设置完成"

echo ""
echo ">>> 步骤 3: 检查 NVIDIA 驱动"
if [ ! -c "/dev/nvidiactl" ]; then
    echo "⚠️  警告: 找不到 /dev/nvidiactl"
    echo "    请确保 NVIDIA 驱动已安装并加载"
else
    echo "✅ NVIDIA 设备节点存在"
fi

echo ""
echo "========================================"
echo "  准备就绪！"
echo "========================================"
echo ""
echo "现在请打开两个终端："
echo ""
echo "【终端 1 - 监控显存】"
echo "  python3 monitor_memory.py"
echo "  或"
echo "  watch -n 1 nvidia-smi"
echo ""
echo "【终端 2 - 运行测试】"
echo "  sudo ./test_virtual_alloc"
echo ""
echo "按回车继续查看详细说明..."
read

echo ""
echo "========================================"
echo "  预期观察结果"
echo "========================================"
echo ""
echo "场景1: 只有 VIRTUAL 标志"
echo "  - nvidia-smi 显存变化: +0~2MB（页表）"
echo "  - 说明: 数据显存未分配，只有少量页表内存"
echo ""
echo "场景2: VIRTUAL + LAZY 标志"
echo "  - nvidia-smi 显存变化: +0MB"
echo "  - 说明: 连页表内存也不分配，完全零占用"
echo ""
echo "场景3: 物理显存分配"
echo "  - nvidia-smi 显存变化: +256MB"
echo "  - 说明: 立即分配物理显存，形成对比"
echo ""
echo "========================================"
echo "  关键结论"
echo "========================================"
echo ""
echo "✅ 只要有 NVOS32_ALLOC_FLAGS_VIRTUAL"
echo "   nvidia-smi 就看不到数据显存的分配"
echo ""
echo "✅ NVOS32_ALLOC_FLAGS_LAZY 不是必需的"
echo "   它只是进一步避免页表内存（< 1%）"
echo ""
echo "详细文档请查看:"
echo "  - ANSWER_SUMMARY.md"
echo "  - QUICK_REFERENCE.md"
echo "  - TESTING_GUIDE_zh.md"
echo ""
