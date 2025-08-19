/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file spinlock.S
 * @brief Assembly implementation of spinlock.S
 * @author Avatar Project Team
 * @date 2024
 */



/*
忙等待：获取锁的处理器会不断地检查锁的状态，直到锁被释放。
低开销：由于不涉及上下文切换，spinlock 比一般的锁（如互斥锁）开销更低。
短时间锁定：适用于锁定时间较短的场景。如果锁定时间较长，自旋等待会浪费 CPU 资源。
 */
.global spin_lock
.global spin_unlock
.global spin_trylock


spin_lock:
    mov w1, #1                // w1 = 1 (表示锁定)
1:  ldaxr w2, [x0]            // 原子加载锁的状态到 w2，带有 Acquire 语义
    cbnz w2, 1b               // 如果锁已被持有，继续自旋
    stlxr w2, w1, [x0]        // 尝试原子存储 1 到锁变量，带有 Release 语义
    cbnz w2, 1b               // 如果存储失败（锁被其他处理器获取），继续自旋
    dmb ish                   // 内存屏障，确保锁定操作完成
    ret

/*
 * when spinlock already taken, r1 will read 1 which is same with the
 * 'strex' failure return value, no extra convertion needed.
 */
spin_trylock:
    mov w1, #1                // w1 = 1 (表示锁定)
    ldaxr w2, [x0]            // 原子加载锁的状态到 w2，带有 Acquire 语义
    cbnz w2, 2f               // 如果锁已被持有，跳到标签2
    stlxr w2, w1, [x0]        // 尝试原子存储 1 到锁变量，带有 Release 语义
    cbnz w2, 2f               // 如果存储失败（锁被其他处理器获取），跳到标签2
    dmb ish                   // 内存屏障，确保锁定操作完成
    mov w0, #0                // 返回 0 表示获取锁成功
    ret
2:  mov w0, #1                // 返回 1 表示获取锁失败
    ret


spin_unlock:
    dmb ish                   // 内存屏障，确保之前的内存操作完成
    mov w1, #0
    stlr w1, [x0]             // 原子存储 0 到锁变量，带有 Release 语义
    ret