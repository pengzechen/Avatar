/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file spinlock.h
 * @brief Implementation of spinlock.h
 * @author Avatar Project Team
 * @date 2024
 */



#ifndef SPINLOCK_H
#define SPINLOCK_H

typedef struct
{
    volatile int32_t lock;
} spinlock_t;

static inline void
spinlock_init(spinlock_t *lock)
{
    lock->lock = 0;
}
extern void
spin_lock(spinlock_t *lock);
extern int32_t
spin_trylock(spinlock_t *lock);
extern void
spin_unlock(spinlock_t *lock);

#endif  // SPINLOCK_H