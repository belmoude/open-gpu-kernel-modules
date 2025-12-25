# 编译错误修复说明

## 问题

编译时遇到错误：
```
test_virtual_alloc.c:313:5: error: a label can only be part of a statement and a declaration is not a statement
```

## 原因

在 C89/C90 标准中，goto 标签（label）后面不能直接跟变量声明。必须先有一个语句。

## 解决方案

### 方案 1: 使用复合语句块（已修复）

在 label 后添加 `{}` 块：

```c
cleanup_device:
    {  // 添加复合语句块
        NVOS00_PARAMETERS freeDeviceParams = {0};
        // ...
    }
```

### 方案 2: 使用 C99 标准编译

```bash
gcc -o test_virtual_alloc test_virtual_alloc.c \
    -std=c99 \
    -I./src/common/sdk/nvidia/inc \
    -I./src/nvidia/arch/nvalloc/unix/include
```

## 现在可以编译了

代码已修复，现在可以正常编译：

```bash
# 使用 Makefile（已包含 -std=c99）
make -f Makefile.test

# 或手动编译
gcc -o test_virtual_alloc test_virtual_alloc.c \
    -std=c99 \
    -I./src/common/sdk/nvidia/inc \
    -I./src/nvidia/arch/nvalloc/unix/include
```

## 验证

编译成功后应该看到：
```
编译测试程序...
编译完成: test_virtual_alloc

运行方式:
  1. 终端1: python3 monitor_memory.py
  2. 终端2: sudo ./test_virtual_alloc
```

## 其他编译选项

如果遇到其他问题，可以尝试：

### 添加调试信息
```bash
gcc -g -o test_virtual_alloc test_virtual_alloc.c \
    -std=c99 \
    -I./src/common/sdk/nvidia/inc \
    -I./src/nvidia/arch/nvalloc/unix/include
```

### 更详细的警告
```bash
gcc -Wall -Wextra -o test_virtual_alloc test_virtual_alloc.c \
    -std=c99 \
    -I./src/common/sdk/nvidia/inc \
    -I./src/nvidia/arch/nvalloc/unix/include
```

### 使用优化
```bash
gcc -O2 -o test_virtual_alloc test_virtual_alloc.c \
    -std=c99 \
    -I./src/common/sdk/nvidia/inc \
    -I./src/nvidia/arch/nvalloc/unix/include
```

## 常见编译问题

### 问题 1: 找不到头文件
```
fatal error: nvos.h: No such file or directory
```

**解决**: 确保在正确的目录执行编译命令
```bash
cd /workspace  # 或你的代码仓库根目录
ls src/common/sdk/nvidia/inc/nvos.h  # 验证头文件存在
```

### 问题 2: 权限问题
```
Permission denied
```

**解决**: 添加执行权限
```bash
chmod +x test_virtual_alloc
```

### 问题 3: 旧版本 GCC
如果你的 GCC 版本很旧（< 4.5），可能不支持某些特性。

**解决**: 升级 GCC 或使用更简单的语法
```bash
gcc --version  # 查看版本
```

推荐 GCC 4.9 或更高版本。
