#ifndef __RBTREE_H__
#define __RBTREE_H__

typedef enum
{
    NODE_RED,
    NODE_BLACK,
} node_color_t;

typedef struct rb_node
{
    void *key;
    void *val;
    struct rb_node *par;
    struct rb_node *left;
    struct rb_node *right;
    node_color_t color;
} rbnode_t;

typedef struct
{
    rbnode_t *head;
    int (*comp)(void *, void *);
} rbtree_t;

void rbtree_init(void);
rbtree_t *rbtree_alloc(int (*comp)(void *, void *));
void rbtree_insert(rbtree_t *tree, void *key, void *val);
rbnode_t *rbtree_search(rbtree_t *tree, void *key);
void rbtree_delete(rbtree_t *tree, void *key);

#endif // __RBTREE_H__