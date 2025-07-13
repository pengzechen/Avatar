

#include <queue.h>
#include <aj_types.h>
#include <io.h>

void queue_remove(queue_node_t *que)
{
    que->prev->next = que->next;
    que->next->prev = que->prev;
    que->prev = NULL;
    que->next = NULL;
}

void queue_insert(queue_node_t *que, queue_node_t *prev)
{
    if (que == NULL || prev == NULL)
    {
        print_info("Null !!\n");
        return;
    }
    que->next = prev->next;
    que->next->prev = que;
    que->prev = prev;
    prev->next = que;
}

void queue_insert_prev(queue_node_t *que, queue_node_t *next)
{
    if (que == NULL || next == NULL)
    {
        print_info("Null !\n");
        return;
    }
    que->prev = next->prev;
    que->prev->next = que;
    que->next = next;
    next->prev = que;
}

void queue_list_init(queue_t *ls)
{
    ls->head.prev = NULL;
    ls->head.next = &(ls->tail);
    ls->tail.prev = &(ls->head);
    ls->tail.next = NULL;
}

void queue_list_enque(queue_t *ls, queue_node_t *que)
{

    queue_insert(que, ls->tail.prev);
}

bool queue_list_is_empty(queue_t *ls)
{
    return ls->head.next->next == NULL;
}

queue_node_t *queue_list_top(queue_t *ls)
{
    if (queue_list_is_empty(ls))
    {
        print_info("# pop failed\n");
        return NULL;
    }
    queue_node_t *que = ls->head.next;
    return que;
}

queue_node_t *queue_list_pop(queue_t *ls)
{
    if (queue_list_is_empty(ls))
    {
        print_info("# pop failed\n");
        return NULL;
    }
    queue_node_t *que = ls->head.next;

    queue_remove(que);
    return que;
}

bool queue_list_check(queue_t *ls)
{
    queue_node_t *tmp = &(ls->head);
    queue_node_t *tail = &(ls->tail);
    if (tmp->prev != NULL)
        return false;
    if (tail->next != NULL)
        return false;
    while (tmp->next != NULL)
    {
        if (tmp->next->prev != tmp)
            return false;
        tmp = tmp->next;
        if (tmp->prev->next != tmp)
            return false;
    }
    return tail == tmp;
}

void queue_list_print(queue_t *ls)
{
    queue_node_t *tmp = ls->head.next;
    while (tmp != &(ls->tail))
    {
        if (tmp == tmp->next)
        {
            print_info("error break\n");
            break;
        }
        tmp = tmp->next;
    }
}

bool queue_list_find(queue_t *ls, queue_node_t *que)
{
    queue_node_t *tmp = ls->head.next;
    while (tmp != &(ls->tail))
    {
        if (tmp == que)
            return true;
        tmp = tmp->next;
    }
    return false;
}
