#include <stdio.h>
#include <string.h>
#include "../../src/include/buffer_pool.h"

int main()
{
    buf_init();

    uint32_t space_user = 1;
    uint32_t space_order = 2;

    void *user_page = buf_get_page(space_user, 0, PAGE_TYPE_LEAF);
    strcpy((char *)user_page, "Hello from user table!");
    buf_mark_dirty(space_user, 0);

    void *order_page = buf_get_page(space_order, 0, PAGE_TYPE_LEAF);
    strcpy((char *)order_page, "Hello from order table!");
    buf_mark_dirty(space_order, 0);

    buf_flush_all();

    buf_init();

    char *reload_user = buf_get_page(space_user, 0, PAGE_TYPE_LEAF);
    char *reload_order = buf_get_page(space_order, 0, PAGE_TYPE_LEAF);

    printf("Reload user: %s\n", reload_user);
    printf("Reload order: %s\n", reload_order);

    return 0;
}