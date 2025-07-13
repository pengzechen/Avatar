

#ifndef SPINLOCK_H
#define SPINLOCK_H

typedef struct
{
    volatile int lock;
} spinlock_t;

static inline void spinlock_init(spinlock_t *lock) { lock->lock = 0; }
extern void spin_lock(spinlock_t *lock);
extern int spin_trylock(spinlock_t *lock);
extern void spin_unlock(spinlock_t *lock);

#endif // SPINLOCK_H