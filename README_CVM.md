# Linux内核机密计算页面状态切换机制

## 概述

这个项目实现了一个基于页面迁移思路的Linux内核机密计算页面状态切换机制，解决了页面加密状态与页表PTE表项C-bit并发访问的安全性问题。

## 问题背景

在机密计算环境中，页面的加密状态（私有/共享）切换与页表PTE表项的C-bit设置并非原子操作，如果与进程对内存页面的并发访问发生冲突，会导致安全性问题。

## 解决方案

借鉴Linux内核页面迁移（page migration）机制的设计思路：
- 在状态切换期间，将所有映射该页的PTE替换成特殊的"状态切换条目"
- 让任何并发访问都转入缺页路径阻塞，等状态切换结束再恢复
- 确保状态切换过程的原子性和安全性

## 架构设计

### 核心组件

1. **状态切换条目（CVM Transition Entry）**
   - 类似于migration entry的特殊PTE条目
   - 用于标识页面正在进行状态切换
   - 包含等待队列和引用计数

2. **缺页处理机制**
   - 当进程访问状态切换中的页面时触发缺页异常
   - 将访问进程阻塞直到状态切换完成
   - 自动重试访问

3. **工作队列系统**
   - 异步执行硬件级别的加密状态切换
   - 支持超时检测和错误恢复
   - 内存热插拔集成

4. **架构适配层**
   - 支持AMD SEV和Intel TDX等不同硬件平台
   - 统一的C-bit操作接口
   - 缓存一致性保证

### 文件结构

```
/workspace/
├── confidential_vm_memory.h       # 主要头文件和数据结构定义
├── confidential_vm_memory.c       # 核心状态切换逻辑实现
├── confidential_vm_fault.c        # 缺页处理机制
├── confidential_vm_workqueue.c    # 工作队列和后台任务
├── arch_cbit.h                   # 架构相关的C-bit操作
├── test_cvm_transition.c          # 测试模块
├── Makefile.cvm                   # 编译配置
└── README_CVM.md                  # 本文档
```

## 核心API

### 状态切换接口

```c
// 开始页面状态切换
int cvm_begin_state_transition(struct vm_area_struct *vma,
                               unsigned long start, unsigned long end,
                               enum cvm_page_state target_state);

// 完成页面状态切换
int cvm_complete_state_transition(struct cvm_state_transition *transition);

// 处理状态切换期间的缺页异常
vm_fault_t cvm_handle_transition_fault(struct vm_fault *vmf);
```

### 页面状态管理

```c
// 设置页面加密状态
int cvm_set_page_state(struct page *page, enum cvm_page_state state);

// 获取页面加密状态
enum cvm_page_state cvm_get_page_state(struct page *page);
```

### 状态切换条目管理

```c
// 分配状态切换条目
struct cvm_transition_entry *cvm_transition_entry_alloc(
    enum cvm_transition_type type, struct page *page);

// 释放状态切换条目
void cvm_transition_entry_free(struct cvm_transition_entry *entry);
```

## 状态机制

### 页面状态

```c
enum cvm_page_state {
    CVM_PAGE_PRIVATE = 0,      // 私有/加密状态
    CVM_PAGE_SHARED = 1,       // 共享/解密状态
    CVM_PAGE_TRANSITIONING = 2 // 状态切换中
};
```

### 切换类型

```c
enum cvm_transition_type {
    CVM_TRANSITION_TO_PRIVATE = 0,  // 切换到私有
    CVM_TRANSITION_TO_SHARED = 1    // 切换到共享
};
```

## 编译和使用

### 编译模块

```bash
# 编译所有模块
make -f Makefile.cvm modules

# 清理编译产物
make -f Makefile.cvm clean
```

### 加载和测试

```bash
# 加载模块
make -f Makefile.cvm load

# 运行基本测试
make -f Makefile.cvm test-basic

# 运行压力测试
make -f Makefile.cvm test-stress

# 查看测试日志
make -f Makefile.cvm log

# 卸载模块
make -f Makefile.cvm unload
```

### 自动化测试

```bash
# 创建测试脚本
make -f Makefile.cvm test-script

# 运行自动化测试
./test_cvm.sh
```

## 硬件平台支持

### AMD SEV

- 使用`_PAGE_ENCRYPTED` C-bit标识加密页面
- 调用`sev_set_memory_encrypted/decrypted`设置硬件状态
- 支持缓存刷新和内存屏障

### Intel TDX

- 使用shared bit标识共享页面
- 调用`tdx_accept_memory/unaccept_memory`设置硬件状态
- 支持TDX特定的内存操作

### 通用平台

- 提供fallback实现用于调试和开发
- 模拟状态切换行为但不执行实际硬件操作

## 核心流程

### 状态切换流程

1. **初始化阶段**
   - 创建状态切换上下文
   - 初始化同步机制

2. **PTE替换阶段**
   - 遍历所有相关的页表条目
   - 原子性地替换为状态切换条目
   - 刷新TLB确保生效

3. **后台切换阶段**
   - 启动工作队列任务
   - 执行硬件级别的状态切换
   - 更新页面状态标记

4. **恢复阶段**
   - 恢复正常的PTE条目
   - 唤醒所有等待的进程
   - 清理资源

### 并发处理流程

1. **缺页触发**
   - 进程访问状态切换中的页面
   - 触发缺页异常

2. **状态检查**
   - 识别为CVM状态切换条目
   - 获取切换上下文信息

3. **等待机制**
   - 将进程加入等待队列
   - 支持可中断和不可中断等待

4. **自动重试**
   - 状态切换完成后自动唤醒
   - 重新尝试内存访问

## 安全特性

### 原子性保证

- 使用页表锁保护PTE修改
- 内存屏障确保操作顺序
- 引用计数防止资源竞争

### 并发安全

- 等待队列机制阻塞并发访问
- 工作队列异步处理避免阻塞
- 超时检测防止无限等待

### 错误处理

- 完整的错误传播机制
- 资源泄漏防护
- 异常情况恢复

## 性能优化

### 内存效率

- 使用kmem_cache管理状态切换条目
- 最小化内存分配开销
- 及时释放临时资源

### 缓存友好

- 局部性优化的数据结构
- 减少不必要的缓存刷新
- 批量处理提高效率

### 可扩展性

- 支持多核并行处理
- 工作队列负载均衡
- 统计信息监控

## 调试和监控

### 调试接口

```c
// 转储状态切换信息
void cvm_dump_transition_state(struct mm_struct *mm);

// 获取活跃切换数量
unsigned long cvm_get_transition_count(void);

// 架构相关调试信息
void arch_dump_encryption_state(struct mm_struct *mm,
                                unsigned long start, unsigned long end);
```

### 内核消息

- 使用`pr_debug`输出详细调试信息
- 使用`pr_info`输出重要状态变化
- 使用`pr_err`输出错误信息

### 统计信息

- 状态切换计数
- 错误统计
- 性能指标

## 测试覆盖

### 基本功能测试

- 页面状态设置和获取
- PTE条目编码解码
- 状态切换条目管理

### 并发测试

- 多线程并发访问
- 状态切换期间的访问阻塞
- 等待和唤醒机制

### 压力测试

- 大量页面状态切换
- 长时间运行稳定性
- 内存使用监控

### 架构兼容性测试

- AMD SEV平台验证
- Intel TDX平台验证
- 通用平台fallback测试

## 扩展和定制

### 添加新架构支持

1. 在`arch_cbit.h`中添加架构检测
2. 实现`pte_mk_encrypted/decrypted`函数
3. 实现`arch_set_page_encryption`函数
4. 添加相应的配置选项

### 性能调优

1. 调整工作队列参数
2. 优化内存分配策略
3. 调整超时阈值
4. 添加性能统计

### 功能增强

1. 添加更多的状态类型
2. 实现批量状态切换
3. 添加用户空间接口
4. 集成内存管理子系统

## 注意事项

### 使用限制

- 需要支持机密计算的硬件平台
- 要求Linux内核版本5.0+
- 需要相应的内核配置选项

### 已知问题

- 大页面支持仍在开发中
- 某些边界情况的处理需要优化
- 性能开销需要进一步测试

### 最佳实践

- 在生产环境使用前进行充分测试
- 监控系统性能和内存使用
- 定期检查内核日志
- 保持模块更新

## 贡献和反馈

这个实现提供了一个完整的机密计算页面状态切换框架，欢迎对代码进行改进和优化。如果发现问题或有建议，请通过适当的渠道反馈。

## 参考资料

- Linux内核页面迁移机制
- AMD SEV技术规范
- Intel TDX技术文档
- Linux内核内存管理子系统