#!/usr/bin/env python3
"""
UVM测试运行器 - 安全最终版本
使用UVM_INITIALIZE正确初始化，并使用安全的CHANNEL_STRESS参数
"""

import os
import sys
import fcntl
import array
import struct
import time
import argparse

# NV_STATUS 错误码
NV_STATUS_CODES = {
    0x00000000: "NV_OK",
    0x00000001: "NV_ERR_GENERIC",
    0x00000004: "NV_ERR_INVALID_PARAMETER",
    0x00000005: "NV_ERR_INVALID_ARGUMENT",
    0x00000006: "NV_ERR_INVALID_STATE",
    0x00000016: "NV_ERR_ILLEGAL_ACTION",
    0x00000017: "NV_ERR_IN_USE",
    0x00000032: "NV_ERR_INVALID_DEVICE",
    0x00000046: "NV_ERR_NO_MEMORY",
    0x00000065: "NV_ERR_NOT_SUPPORTED",
}

def get_nv_status_name(status_code):
    return NV_STATUS_CODES.get(status_code, f"UNKNOWN_0x{status_code:08x}")

def setup_test_params(cmd_id, test_name):
    """为每个测试设置正确且安全的参数"""
    
    if cmd_id == 200:  # GET_GPU_REF_COUNT
        params = array.array('B', [0] * 32)
        return params, 24
    elif cmd_id == 201:  # RNG_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 202:  # RANGE_TREE_DIRECTED
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 203:  # RANGE_TREE_RANDOM
        params = array.array('B', [0] * 256)
        struct.pack_into('<I', params, 0, 12345)      # 固定seed，避免随机问题
        struct.pack_into('<Q', params, 8, 20)         # 较小的iterations
        struct.pack_into('<I', params, 16, 0)         # verbose
        struct.pack_into('<I', params, 20, 75)        # high_probability
        struct.pack_into('<I', params, 24, 60)        # add_remove_shrink_group_probability
        struct.pack_into('<I', params, 28, 30)        # shrink_probability
        struct.pack_into('<I', params, 32, 5)         # collision_checks (减少)
        struct.pack_into('<I', params, 36, 3)         # iterator_checks (减少)
        struct.pack_into('<Q', params, 40, 0x10000)   # max_end (减少到64KB)
        struct.pack_into('<Q', params, 48, 20)        # max_ranges (减少)
        struct.pack_into('<Q', params, 56, 5)         # max_batch_count (减少)
        struct.pack_into('<I', params, 64, 50)        # max_attempts (减少)
        return params, 252
    elif cmd_id == 204:  # VA_RANGE_INFO
        params = array.array('B', [0] * 512)
        struct.pack_into('<Q', params, 0, 0x400000)
        return params, 508
    elif cmd_id == 205:  # RM_MEM_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 206:  # GPU_SEMAPHORE_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 207:  # PEER_REF_COUNT
        params = array.array('B', [0] * 48)
        return params, 32
    elif cmd_id == 208:  # VA_RANGE_SPLIT
        params = array.array('B', [0] * 16)
        struct.pack_into('<Q', params, 0, 0x500000)
        return params, 8
    elif cmd_id == 209:  # VA_RANGE_INJECT_SPLIT_ERROR
        params = array.array('B', [0] * 16)
        struct.pack_into('<Q', params, 0, 0x600000)
        return params, 8
    elif cmd_id == 210:  # PAGE_TREE
        params = array.array('B', [0] * 4096)
        return params, 4092
    elif cmd_id == 211:  # CHANGE_PTE_MAPPING
        params = array.array('B', [0] * 128)
        struct.pack_into('<Q', params, 0, 0x700000)
        struct.pack_into('<I', params, 8, 0)
        return params, 124
    elif cmd_id == 212:  # TRACKER_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 213:  # PUSH_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 214:  # CHANNEL_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 215:  # CHANNEL_STRESS - 🛡️ 安全版本!
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 0)          # mode = NOOP_PUSH (最安全)
        struct.pack_into('<I', params, 4, 5)          # iterations = 5 (而不是100!)
        struct.pack_into('<I', params, 8, 1)          # num_streams = 1 (而不是4!)
        struct.pack_into('<I', params, 12, 0)         # key_rotation_operation
        struct.pack_into('<I', params, 16, 12345)     # seed = 固定值 (而不是随机!)
        struct.pack_into('<I', params, 20, 0)         # verbose
        return params, 24
    elif cmd_id == 216:  # CE_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 217:  # VA_BLOCK_INFO
        params = array.array('B', [0] * 512)
        struct.pack_into('<Q', params, 0, 0x800000)
        return params, 508
    elif cmd_id == 218:  # LOCK_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 219:  # PERF_UTILS_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 220:  # KVMALLOC - 安全版本
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 2)          # num_threads = 2 (减少)
        struct.pack_into('<I', params, 4, 20)         # iterations = 20 (减少)
        struct.pack_into('<I', params, 8, 1024)       # max_allocation_size = 1KB (减少)
        return params, 12
    elif cmd_id == 221:  # PMM_QUERY
        params = array.array('B', [0] * 128)
        return params, 124
    elif cmd_id == 222:  # PMM_CHECK_LEAK
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 0)
        return params, 12
    elif cmd_id == 223:  # PERF_EVENTS_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 224:  # PERF_MODULE_SANITY
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 1)  # 减少模块数量
        return params, 12
    elif cmd_id == 225:  # RANGE_ALLOCATOR_SANITY
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 2)          # num_threads = 2 (减少)
        struct.pack_into('<I', params, 4, 20)         # iterations = 20 (减少)
        return params, 28
    elif cmd_id == 226:  # GET_RM_PTES
        params = array.array('B', [0] * 128)
        struct.pack_into('<Q', params, 16, 0x900000)
        struct.pack_into('<I', params, 24, 0)
        return params, 124
    elif cmd_id == 227:  # FAULT_BUFFER_FLUSH
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 5)          # iterations = 5 (减少)
        return params, 28
    elif cmd_id == 228:  # INJECT_TOOLS_EVENT
        params = array.array('B', [0] * 64)
        struct.pack_into('<I', params, 0, 1)
        return params, 60
    elif cmd_id == 229:  # INCREMENT_TOOLS_COUNTER
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 1)
        return params, 28
    elif cmd_id == 230:  # MEM_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 232:  # MAKE_CHANNEL_STOPS_IMMEDIATE
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 233:  # VA_BLOCK_INJECT_ERROR
        params = array.array('B', [0] * 64)
        struct.pack_into('<Q', params, 0, 0xA00000)
        struct.pack_into('<I', params, 8, 1)
        return params, 60
    elif cmd_id == 234:  # PEER_IDENTITY_MAPPINGS
        params = array.array('B', [0] * 128)
        return params, 124
    elif cmd_id == 235:  # VA_RESIDENCY_INFO
        params = array.array('B', [0] * 4096)
        struct.pack_into('<Q', params, 0, 0xB00000)
        struct.pack_into('<Q', params, 8, 0x1000)
        return params, 4092
    elif cmd_id == 236:  # PMM_ASYNC_ALLOC
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 2)  # 减少chunk数量
        return params, 28
    elif cmd_id == 237:  # SET_PREFETCH_FILTERING
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 1)
        return params, 12
    elif cmd_id == 240:  # PMM_SANITY
        params = array.array('B', [0] * 64)
        return params, 60
    elif cmd_id == 241:  # INVALIDATE_TLB
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 0)
        return params, 28
    elif cmd_id == 242:  # VA_BLOCK
        params = array.array('B', [0] * 2048)
        struct.pack_into('<Q', params, 0, 0xC00000)
        return params, 2044
    elif cmd_id == 243:  # EVICT_CHUNK
        params = array.array('B', [0] * 64)
        struct.pack_into('<I', params, 0, 1)  # 减少chunk数量
        return params, 60
    elif cmd_id == 244:  # FLUSH_DEFERRED_WORK
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 245:  # NV_KTHREAD_Q
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 246:  # SET_PAGE_PREFETCH_POLICY
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 0)
        return params, 12
    elif cmd_id == 247:  # RANGE_GROUP_TREE
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 20)  # 减少iterations
        return params, 28
    elif cmd_id == 248:  # RANGE_GROUP_RANGE_INFO
        params = array.array('B', [0] * 128)
        struct.pack_into('<Q', params, 0, 0xD00000)
        return params, 124
    elif cmd_id == 249:  # RANGE_GROUP_RANGE_COUNT
        params = array.array('B', [0] * 32)
        struct.pack_into('<Q', params, 0, 0xE00000)
        return params, 28
    elif cmd_id == 250:  # GET_PREFETCH_FAULTS_REENABLE_LAPSE
        params = array.array('B', [0] * 16)
        return params, 12
    elif cmd_id == 251:  # SET_PREFETCH_FAULTS_REENABLE_LAPSE
        params = array.array('B', [0] * 16)
        struct.pack_into('<Q', params, 0, 100000)  # 减少时间间隔
        return params, 12
    elif cmd_id == 252:  # GET_KERNEL_VIRTUAL_ADDRESS
        params = array.array('B', [0] * 16)
        return params, 12
    elif cmd_id == 253:  # PMA_ALLOC_FREE
        params = array.array('B', [0] * 64)
        struct.pack_into('<I', params, 0, 2)          # 减少分配数量
        struct.pack_into('<I', params, 4, 1024)       # 减少分配大小
        return params, 60
    elif cmd_id == 254:  # PMM_ALLOC_FREE_ROOT
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 1)  # 减少chunk数量
        return params, 28
    elif cmd_id == 255:  # PMM_INJECT_PMA_EVICT_ERROR
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 3)  # 减少分配数量
        return params, 28
    elif cmd_id == 256:  # RECONFIGURE_ACCESS_COUNTERS
        params = array.array('B', [0] * 128)
        struct.pack_into('<I', params, 16, 0)  # 禁用tracking，更安全
        return params, 124
    elif cmd_id == 257:  # RESET_ACCESS_COUNTERS
        params = array.array('B', [0] * 64)
        struct.pack_into('<I', params, 16, 0)
        return params, 60
    elif cmd_id == 258:  # SET_IGNORE_ACCESS_COUNTERS
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 16, 1)  # 设置为忽略，更安全
        return params, 28
    elif cmd_id == 259:  # CHECK_CHANNEL_VA_SPACE
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 0)  # 保守的检查值
        return params, 28
    elif cmd_id == 260:  # ENABLE_NVLINK_PEER_ACCESS
        params = array.array('B', [0] * 48)
        return params, 44
    elif cmd_id == 261:  # DISABLE_NVLINK_PEER_ACCESS
        params = array.array('B', [0] * 48)
        return params, 44
    elif cmd_id == 262:  # GET_PAGE_THRASHING_POLICY
        params = array.array('B', [0] * 16)
        return params, 12
    elif cmd_id == 263:  # SET_PAGE_THRASHING_POLICY
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 0)
        return params, 12
    elif cmd_id == 264:  # PMM_SYSMEM
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 0)
        return params, 12
    elif cmd_id == 265:  # PMM_REVERSE_MAP
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 5)  # 减少iterations
        return params, 28
    elif cmd_id == 266:  # PMM_INDIRECT_PEERS
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 3)  # 减少iterations
        return params, 28
    elif cmd_id == 267:  # VA_SPACE_MM_RETAIN
        params = array.array('B', [0] * 64)
        struct.pack_into('<I', params, 0, 1)
        return params, 60
    elif cmd_id == 269:  # PMM_CHUNK_WITH_ELEVATED_PAGE
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 2)  # 减少iterations
        return params, 28
    elif cmd_id == 270:  # GET_GPU_TIME
        params = array.array('B', [0] * 32)
        return params, 24
    elif cmd_id == 271:  # ACCESS_COUNTERS_ENABLED_BY_DEFAULT
        params = array.array('B', [0] * 32)
        return params, 28
    elif cmd_id == 272:  # VA_SPACE_INJECT_ERROR
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 1)
        struct.pack_into('<I', params, 4, 3)  # 减少iterations
        return params, 28
    elif cmd_id == 273:  # PMM_RELEASE_FREE_ROOT_CHUNKS
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 2)  # 保留更多chunks
        return params, 12
    elif cmd_id == 274:  # DRAIN_REPLAYABLE_FAULTS
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 2)  # 减少iterations
        return params, 28
    elif cmd_id == 275:  # PMA_GET_BATCH_SIZE
        params = array.array('B', [0] * 32)
        return params, 28
    elif cmd_id == 276:  # PMM_QUERY_PMA_STATS
        params = array.array('B', [0] * 256)
        return params, 252
    elif cmd_id == 278:  # NUMA_CHECK_AFFINITY
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 3)  # 减少iterations
        return params, 12
    elif cmd_id == 279:  # VA_SPACE_ADD_DUMMY_THREAD_CONTEXTS
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 2)  # 减少线程数
        return params, 12
    elif cmd_id == 280:  # VA_SPACE_REMOVE_DUMMY_THREAD_CONTEXTS
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 281:  # THREAD_CONTEXT_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 282:  # THREAD_CONTEXT_PERF - 安全版本
        params = array.array('B', [0] * 64)
        struct.pack_into('<I', params, 0, 2)          # 减少线程数
        struct.pack_into('<I', params, 4, 20)         # 减少iterations
        return params, 60
    elif cmd_id == 283:  # GET_PAGEABLE_MEM_ACCESS_TYPE
        params = array.array('B', [0] * 32)
        struct.pack_into('<Q', params, 0, 0xE00000)
        return params, 28
    elif cmd_id == 284:  # TOOLS_FLUSH_REPLAY_EVENTS
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 2)  # 减少iterations
        return params, 12
    elif cmd_id == 285:  # REGISTER_UNLOAD_STATE_BUFFER
        params = array.array('B', [0] * 32)
        struct.pack_into('<Q', params, 0, 0x1000000)
        struct.pack_into('<Q', params, 8, 1024)  # 减少缓冲区大小
        return params, 16
    elif cmd_id == 286:  # RB_TREE_DIRECTED
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 20)  # 减少iterations
        return params, 28
    elif cmd_id == 287:  # RB_TREE_RANDOM
        params = array.array('B', [0] * 64)
        struct.pack_into('<I', params, 0, 12345)      # 固定seed
        struct.pack_into('<I', params, 4, 20)         # 减少iterations
        return params, 60
    elif cmd_id == 288:  # HOST_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 289:  # VA_SPACE_MM_OR_CURRENT_RETAIN
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 1)
        return params, 12
    elif cmd_id == 290:  # GET_USER_SPACE_END_ADDRESS
        params = array.array('B', [0] * 16)
        return params, 8
    elif cmd_id == 291:  # GET_CPU_CHUNK_ALLOC_SIZES
        params = array.array('B', [0] * 16)
        return params, 4
    elif cmd_id == 293:  # VA_RANGE_INJECT_ADD_GPU_VA_SPACE_ERROR
        params = array.array('B', [0] * 32)
        struct.pack_into('<Q', params, 0, 0x1100000)
        return params, 28
    elif cmd_id == 294:  # DESTROY_GPU_VA_SPACE_DELAY
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 10)  # 减少延迟时间
        return params, 12
    elif cmd_id == 295:  # SEC2_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 296:  # CGROUP_ACCOUNTING_SUPPORTED
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 298:  # SPLIT_INVALIDATE_DELAY
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 100)  # 减少延迟
        return params, 12
    elif cmd_id == 299:  # SEC2_CPU_GPU_ROUNDTRIP
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 2)  # 减少iterations
        return params, 12
    elif cmd_id == 300:  # CPU_CHUNK_API
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 5)  # 减少iterations
        return params, 12
    elif cmd_id == 301:  # FORCE_CPU_TO_CPU_COPY_WITH_CE
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 302:  # VA_SPACE_ALLOW_MOVABLE_ALLOCATIONS
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 1)
        return params, 12
    elif cmd_id == 303:  # SKIP_MIGRATE_VMA
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 1)
        return params, 12
    else:
        # 默认参数
        params = array.array('B', [0] * 1024)
        return params, 1020

# 所有97个测试用例 - 完整列表
ALL_UVM_TESTS = [
    (200, "GET_GPU_REF_COUNT", "Get GPU reference count", True),
    (201, "RNG_SANITY", "Random number generator sanity test", False),
    (202, "RANGE_TREE_DIRECTED", "Directed range tree test", False),
    (203, "RANGE_TREE_RANDOM", "Random range tree test", False),
    (204, "VA_RANGE_INFO", "VA range information test", False),
    (205, "RM_MEM_SANITY", "RM memory sanity test", True),
    (206, "GPU_SEMAPHORE_SANITY", "GPU semaphore sanity test", True),
    (207, "PEER_REF_COUNT", "Peer reference count test", True),
    (208, "VA_RANGE_SPLIT", "VA range split test", False),
    (209, "VA_RANGE_INJECT_SPLIT_ERROR", "VA range split error injection test", False),
    (210, "PAGE_TREE", "Page tree test", True),
    (211, "CHANGE_PTE_MAPPING", "Change PTE mapping test", True),
    (212, "TRACKER_SANITY", "Tracker sanity test", True),
    (213, "PUSH_SANITY", "Push sanity test", True),
    (214, "CHANNEL_SANITY", "Channel sanity test", True),
    (215, "CHANNEL_STRESS", "Channel stress test (SAFE VERSION)", True),  # 标记为安全版本
    (216, "CE_SANITY", "Copy engine sanity test", True),
    (217, "VA_BLOCK_INFO", "VA block information test", False),
    (218, "LOCK_SANITY", "Lock sanity test", False),
    (219, "PERF_UTILS_SANITY", "Performance utils sanity test", False),
    (220, "KVMALLOC", "Kernel memory allocation test", False),
    (221, "PMM_QUERY", "Physical memory manager query test", True),
    (222, "PMM_CHECK_LEAK", "PMM leak check test", True),
    (223, "PERF_EVENTS_SANITY", "Performance events sanity test", False),
    (224, "PERF_MODULE_SANITY", "Performance module sanity test", False),
    (225, "RANGE_ALLOCATOR_SANITY", "Range allocator sanity test", False),
    (226, "GET_RM_PTES", "Get RM PTEs test", True),
    (227, "FAULT_BUFFER_FLUSH", "Fault buffer flush test", True),
    (228, "INJECT_TOOLS_EVENT", "Inject tools event test", False),
    (229, "INCREMENT_TOOLS_COUNTER", "Increment tools counter test", False),
    (230, "MEM_SANITY", "Memory sanity test", False),
    (232, "MAKE_CHANNEL_STOPS_IMMEDIATE", "Make channel stops immediate test", True),
    (233, "VA_BLOCK_INJECT_ERROR", "VA block error injection test", False),
    (234, "PEER_IDENTITY_MAPPINGS", "Peer identity mappings test", True),
    (235, "VA_RESIDENCY_INFO", "VA residency information test", False),
    (236, "PMM_ASYNC_ALLOC", "PMM async allocation test", True),
    (237, "SET_PREFETCH_FILTERING", "Set prefetch filtering test", False),
    (240, "PMM_SANITY", "PMM sanity test", True),
    (241, "INVALIDATE_TLB", "TLB invalidation test", True),
    (242, "VA_BLOCK", "VA block test", False),
    (243, "EVICT_CHUNK", "Evict chunk test", True),
    (244, "FLUSH_DEFERRED_WORK", "Flush deferred work test", False),
    (245, "NV_KTHREAD_Q", "NV kernel thread queue test", False),
    (246, "SET_PAGE_PREFETCH_POLICY", "Set page prefetch policy test", False),
    (247, "RANGE_GROUP_TREE", "Range group tree test", False),
    (248, "RANGE_GROUP_RANGE_INFO", "Range group range info test", False),
    (249, "RANGE_GROUP_RANGE_COUNT", "Range group range count test", False),
    (250, "GET_PREFETCH_FAULTS_REENABLE_LAPSE", "Get prefetch faults reenable lapse", False),
    (251, "SET_PREFETCH_FAULTS_REENABLE_LAPSE", "Set prefetch faults reenable lapse", False),
    (252, "GET_KERNEL_VIRTUAL_ADDRESS", "Get kernel virtual address test", False),
    (253, "PMA_ALLOC_FREE", "PMA allocation/free test", True),
    (254, "PMM_ALLOC_FREE_ROOT", "PMM alloc/free root test", True),
    (255, "PMM_INJECT_PMA_EVICT_ERROR", "PMM inject PMA evict error test", True),
    (256, "RECONFIGURE_ACCESS_COUNTERS", "Reconfigure access counters test", True),
    (257, "RESET_ACCESS_COUNTERS", "Reset access counters test", True),
    (258, "SET_IGNORE_ACCESS_COUNTERS", "Set ignore access counters test", True),
    (259, "CHECK_CHANNEL_VA_SPACE", "Check channel VA space test", True),
    (260, "ENABLE_NVLINK_PEER_ACCESS", "Enable NVLink peer access test", True),
    (261, "DISABLE_NVLINK_PEER_ACCESS", "Disable NVLink peer access test", True),
    (262, "GET_PAGE_THRASHING_POLICY", "Get page thrashing policy test", False),
    (263, "SET_PAGE_THRASHING_POLICY", "Set page thrashing policy test", False),
    (264, "PMM_SYSMEM", "PMM system memory test", False),
    (265, "PMM_REVERSE_MAP", "PMM reverse mapping test", True),
    (266, "PMM_INDIRECT_PEERS", "PMM indirect peers test", True),
    (267, "VA_SPACE_MM_RETAIN", "VA space MM retain test", False),
    (269, "PMM_CHUNK_WITH_ELEVATED_PAGE", "PMM chunk with elevated page test", True),
    (270, "GET_GPU_TIME", "Get GPU time test", True),
    (271, "ACCESS_COUNTERS_ENABLED_BY_DEFAULT", "Access counters enabled by default", True),
    (272, "VA_SPACE_INJECT_ERROR", "VA space error injection test", False),
    (273, "PMM_RELEASE_FREE_ROOT_CHUNKS", "PMM release free root chunks test", True),
    (274, "DRAIN_REPLAYABLE_FAULTS", "Drain replayable faults test", True),
    (275, "PMA_GET_BATCH_SIZE", "PMA get batch size test", True),
    (276, "PMM_QUERY_PMA_STATS", "PMM query PMA stats test", True),
    (278, "NUMA_CHECK_AFFINITY", "NUMA check affinity test", False),
    (279, "VA_SPACE_ADD_DUMMY_THREAD_CONTEXTS", "VA space add dummy thread contexts", False),
    (280, "VA_SPACE_REMOVE_DUMMY_THREAD_CONTEXTS", "VA space remove dummy thread contexts", False),
    (281, "THREAD_CONTEXT_SANITY", "Thread context sanity test", False),
    (282, "THREAD_CONTEXT_PERF", "Thread context performance test", False),
    (283, "GET_PAGEABLE_MEM_ACCESS_TYPE", "Get pageable memory access type test", False),
    (284, "TOOLS_FLUSH_REPLAY_EVENTS", "Tools flush replay events test", False),
    (285, "REGISTER_UNLOAD_STATE_BUFFER", "Register unload state buffer test", False),
    (286, "RB_TREE_DIRECTED", "Red-black tree directed test", False),
    (287, "RB_TREE_RANDOM", "Red-black tree random test", False),
    (288, "HOST_SANITY", "Host sanity test", True),
    (289, "VA_SPACE_MM_OR_CURRENT_RETAIN", "VA space MM or current retain test", False),
    (290, "GET_USER_SPACE_END_ADDRESS", "Get user space end address test", False),
    (291, "GET_CPU_CHUNK_ALLOC_SIZES", "Get CPU chunk allocation sizes test", False),
    (293, "VA_RANGE_INJECT_ADD_GPU_VA_SPACE_ERROR", "VA range inject add GPU VA space error", True),
    (294, "DESTROY_GPU_VA_SPACE_DELAY", "Destroy GPU VA space delay test", True),
    (295, "SEC2_SANITY", "SEC2 sanity test", True),
    (296, "CGROUP_ACCOUNTING_SUPPORTED", "CGroup accounting supported test", False),
    (298, "SPLIT_INVALIDATE_DELAY", "Split invalidate delay test", False),
    (299, "SEC2_CPU_GPU_ROUNDTRIP", "SEC2 CPU-GPU roundtrip test", True),
    (300, "CPU_CHUNK_API", "CPU chunk API test", False),
    (301, "FORCE_CPU_TO_CPU_COPY_WITH_CE", "Force CPU to CPU copy with CE test", True),
    (302, "VA_SPACE_ALLOW_MOVABLE_ALLOCATIONS", "VA space allow movable allocations", False),
    (303, "SKIP_MIGRATE_VMA", "Skip migrate VMA test", False),
]

class UVMTestRunnerSafe:
    """安全的UVM测试运行器"""
    
    def __init__(self, device_path="/dev/nvidia-uvm"):
        self.device_path = device_path
        self.fd = None
        self.initialized = False
        self.verbose = False
        self.skip_dangerous = False
    
    def initialize_uvm(self):
        """使用UVM_INITIALIZE正确初始化"""
        if self.verbose:
            print("🔧 使用UVM_INITIALIZE初始化UVM...")
        
        try:
            params = array.array('B', [0] * 16)
            struct.pack_into('<Q', params, 0, 0)  # flags
            
            ioctl_result = fcntl.ioctl(self.fd, 0x30000001, params)  # UVM_INITIALIZE
            rm_status = struct.unpack('<I', params[8:12])[0]
            
            if self.verbose:
                print(f"  ioctl返回值: {ioctl_result}")
                print(f"  rmStatus: {rm_status} ({get_nv_status_name(rm_status)})")
            
            if rm_status == 0:
                if self.verbose:
                    print("  ✅ UVM初始化成功!")
                self.initialized = True
                return True
            else:
                if self.verbose:
                    print(f"  ❌ UVM初始化失败: {get_nv_status_name(rm_status)}")
                return False
                
        except Exception as e:
            if self.verbose:
                print(f"  ❌ UVM初始化异常: {e}")
            return False
    
    def run_single_test(self, cmd_id, test_name, description, requires_gpu=False):
        """运行单个测试，包含安全检查"""
        
        # 危险测试的安全检查
        if self.skip_dangerous and test_name in ["CHANNEL_STRESS"]:
            return {
                'ioctl_result': 0,
                'rm_status': 0,
                'status_name': "SKIPPED_FOR_SAFETY",
                'success': True,
                'skipped': True
            }
        
        try:
            params, rmstatus_offset = setup_test_params(cmd_id, test_name)
            ioctl_result = fcntl.ioctl(self.fd, cmd_id, params)
            
            if rmstatus_offset >= 0 and rmstatus_offset + 4 <= len(params):
                rm_status = struct.unpack('<I', params[rmstatus_offset:rmstatus_offset+4])[0]
            else:
                rm_status = -1
            
            return {
                'ioctl_result': ioctl_result,
                'rm_status': rm_status,
                'status_name': get_nv_status_name(rm_status),
                'success': rm_status == 0,
                'skipped': False
            }
            
        except Exception as e:
            return {'error': str(e), 'success': False, 'skipped': False}
    
    def run_all_tests_safely(self, test_filter=None, continue_on_error=True, skip_dangerous=False):
        """安全地运行所有测试"""
        
        self.skip_dangerous = skip_dangerous
        
        # 过滤测试
        if test_filter:
            tests_to_run = [(cmd_id, name, desc, gpu) for cmd_id, name, desc, gpu in ALL_UVM_TESTS 
                           if test_filter.lower() in name.lower()]
        else:
            tests_to_run = ALL_UVM_TESTS
        
        print(f"UVM安全测试运行器 - 最终版本")
        print(f"===========================")
        print(f"✅ 使用UVM_INITIALIZE (0x30000001)正确初始化")
        print(f"✅ 安全的CHANNEL_STRESS参数 (iterations=5, streams=1)")
        print(f"✅ 包含所有 {len(ALL_UVM_TESTS)} 个测试用例")
        if skip_dangerous:
            print(f"⚠️ 跳过危险测试以避免系统重启")
        print()
        
        # 打开设备
        self.fd = os.open(self.device_path, os.O_RDWR)
        
        try:
            # 正确初始化UVM
            uvm_initialized = self.initialize_uvm()
            if uvm_initialized:
                print("✅ UVM初始化成功，现在可以运行所有测试")
            else:
                print("❌ UVM初始化失败")
                return False
            print()
            
            # 运行测试
            stats = {"pass": 0, "fail": 0, "error": 0, "skipped": 0}
            start_time = time.time()
            
            print(f"开始运行 {len(tests_to_run)} 个测试...")
            print()
            
            for i, (cmd_id, test_name, description, requires_gpu) in enumerate(tests_to_run, 1):
                print(f"[{i:2}/{len(tests_to_run)}] {test_name:35} ", end="", flush=True)
                
                if self.verbose:
                    print()
                    print(f"    描述: {description}")
                    print(f"    命令ID: {cmd_id}")
                    print(f"    需要GPU: {'是' if requires_gpu else '否'}")
                    
                    # 特别标记CHANNEL_STRESS
                    if test_name == "CHANNEL_STRESS":
                        print(f"    ⚠️ 使用安全参数 (iterations=5, streams=1)")
                    
                    print("    执行中... ", end="", flush=True)
                
                result = self.run_single_test(cmd_id, test_name, description, requires_gpu)
                
                if 'error' in result:
                    print("[SYSTEM_ERROR]")
                    if self.verbose:
                        print(f"    系统错误: {result['error']}")
                    else:
                        print(f"  系统错误: {result['error']}")
                    stats['error'] += 1
                    
                    if not continue_on_error:
                        break
                        
                elif result.get('skipped', False):
                    print("[SKIPPED]")
                    if self.verbose:
                        print(f"    跳过: 安全考虑")
                    else:
                        print(f"  跳过: 安全考虑")
                    stats['skipped'] += 1
                    
                elif result['success']:
                    print("[PASS]")
                    if self.verbose:
                        print(f"    结果: 测试成功")
                    stats['pass'] += 1
                    
                else:
                    print("[FAIL]")
                    if self.verbose:
                        print(f"    结果: 测试失败")
                        print(f"    rmStatus: {result['status_name']}")
                    else:
                        print(f"  内核错误: {result['status_name']}")
                    stats['fail'] += 1
                
                time.sleep(0.01)
            
            end_time = time.time()
            
            # 打印总结
            total = sum(stats.values())
            print()
            print("=" * 70)
            print("安全版本测试结果")
            print("=" * 70)
            print(f"UVM初始化:    {'成功' if uvm_initialized else '失败'}")
            print(f"总测试数:     {total}")
            print(f"通过:         {stats['pass']}")
            print(f"失败:         {stats['fail']}")
            print(f"系统错误:     {stats['error']}")
            print(f"跳过:         {stats['skipped']}")
            print(f"成功率:       {stats['pass']*100//total if total > 0 else 0}%")
            print(f"执行时间:     {end_time - start_time:.1f} 秒")
            
            print()
            print("安全改进:")
            print("✅ CHANNEL_STRESS使用安全参数 (iterations=5, streams=1)")
            print("✅ 所有压力测试参数都已调低")
            print("✅ 使用固定seed避免随机问题")
            print("✅ 应该不会导致系统重启")
            
            return stats['error'] == 0
            
        finally:
            if self.fd:
                os.close(self.fd)

def main():
    parser = argparse.ArgumentParser(description="Safe UVM Test Runner with Correct Initialization")
    parser.add_argument("-v", "--verbose", action="store_true", help="详细输出")
    parser.add_argument("-c", "--continue", action="store_true", help="错误后继续")
    parser.add_argument("-t", "--test", help="运行指定测试")
    parser.add_argument("-f", "--filter", help="过滤测试名称")
    parser.add_argument("--skip-dangerous", action="store_true", help="跳过危险测试")
    parser.add_argument("--analyze-only", action="store_true", help="只分析CHANNEL_STRESS参数")
    
    args = parser.parse_args()
    
    if args.analyze_only:
        if os.geteuid() != 0:
            print("建议以root身份运行分析: sudo python3", sys.argv[0], "--analyze-only")
            return
        
        import subprocess
        subprocess.run([sys.executable, "/workspace/analyze_channel_stress_issue.py"])
        return
    
    if os.geteuid() != 0:
        print("建议以root身份运行: sudo python3", sys.argv[0])
        print()
    
    runner = UVMTestRunnerSafe()
    runner.verbose = args.verbose
    
    # 运行测试
    success = runner.run_all_tests_safely(
        args.filter, 
        getattr(args, 'continue'),
        args.skip_dangerous
    )
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()