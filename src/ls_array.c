#include <memory.h>
#include "ls_array.h"

ls_array_t* ls_create_array(ls_mem_pool_t* pool, size_t size, size_t n)
{
    ls_array_t* array = ls_palloc(pool, sizeof(ls_array_t));
    if(array == NULL) {
        return NULL;
    }
    void* head = ls_palloc(pool, n * size);

    if(head == NULL) {
        return NULL;
    }

    array->head = head;
    array->n_alocted = n;
    array->n_elmnts = 0;
    array->mem_pool = pool;
    array->size = size;

    return array; 
}

void* ls_array_push(ls_array_t* array)
{
    /* Check if the array is full */
    if(array->n_elmnts == array->n_alocted) {
        size_t new_capacity = array->n_alocted * 2;

        void* new_head = ls_palloc(array->mem_pool, new_capacity * array->size);
        if (new_head == NULL) {
            return NULL;
        }

        /* Copy old data */
        memcpy(new_head, array->head, array->n_elmnts * array->size);

        /* Update array */
        array->head = new_head;
        array->n_alocted = new_capacity;
    }
    /* Just return the next position if array is not full */
    return (u_char*)array->head + (array->size * array->n_elmnts++);
}