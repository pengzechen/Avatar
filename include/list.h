#ifndef LIST_H
#define LIST_H

#include <aj_types.h>

typedef struct _list_node_t {
    struct _list_node_t* pre;
    struct _list_node_t* next;

} list_node_t;

typedef struct _list_t {
    list_node_t* first;
    list_node_t* last;
    int          count;
    
}list_t;


static inline void list_node_init(list_node_t* node) {
    node->pre = node->next = (list_node_t*)0;
}

static inline list_node_t* list_node_pre(list_node_t* node) {
    return node->pre;
}

static inline list_node_t* list_node_next(list_node_t* node) {
    return node->next;
}


static inline int list_is_empty (list_t* list) {
    return list->count == 0;
}

static inline int list_count (list_t* list) {
    return list->count;
}

static inline list_node_t* list_first (list_t* list) {
    return list->first;
}

static inline list_node_t* list_last (list_t* list) {
    return list->last;
}


#define offset_in_parent(parent_type, node_name) \
    (   (uint64_t)&(  ((parent_type*)0)->node_name  )     )

#define parent_addr(node, parent_type, node_name) \
    (   (uint64_t)node - offset_in_parent(parent_type, node_name)     )

#define list_node_parent(node, parent_type, node_name) \
    (   (parent_type*)(node ? parent_addr(node, parent_type, node_name) :0)     )


void list_init(list_t* list);

void         list_insert_first (list_t* list, list_node_t* node);
void         list_insert_last (list_t* list, list_node_t* node);

list_node_t* list_delete_first (list_t* list);
list_node_t* list_delete (list_t* list, list_node_t* node);

#endif
