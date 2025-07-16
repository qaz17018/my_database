#include <stdlib.h>
#include <string.h>
#include "hash_table.h"

// 创建哈希表
HashTable *hash_table_create(int size)
{
    HashTable *table = (HashTable *)malloc(sizeof(HashTable));
    if (!table)
        return NULL;

    table->size = size;
    table->buckets = (HashNode **)calloc(table->size, sizeof(HashNode *));
    if (!table->buckets)
    {
        free(table);
        return NULL;
    }
    return table;
}

static uint32_t hash(uint32_t key)
{
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = (key >> 16) ^ key;
    return key;
}

// 插入或更新
void hash_table_put(HashTable *table, uint32_t key, int value)
{
    uint32_t index = hash(key) % table->size;
    HashNode *node = table->buckets[index];

    // 检查键是否已存在，如果存在则更新
    while (node)
    {
        if (node->key == key)
        {
            node->value = value;
            return;
        }
        node = node->next;
    }

    // 如果键不存在，则在链表头部插入新节点
    HashNode *new_node = (HashNode *)malloc(sizeof(HashNode));
    new_node->key = key;
    new_node->value = value;
    new_node->next = table->buckets[index];
    table->buckets[index] = new_node;
}

int hash_table_get(HashTable *table, uint32_t key)
{
    uint32_t index = hash(key) % table->size;
    HashNode *node = table->buckets[index];
    while (node)
    {
        if (node->key == key)
        {
            return node->value;
        }
        node = node->next;
    }
    return -1; // 未找到
}

void hash_table_remove(HashTable *table, uint32_t key)
{
    uint32_t index = hash(key) % table->size;
    HashNode *node = table->buckets[index];
    HashNode *prev = NULL;

    while (node)
    {
        if (node->key == key)
        {
            if (prev)
            {
                prev->next = node->next;
            }
            else
            {
                table->buckets[index] = node->next;
            }
            free(node);
            return;
        }
        prev = node;
        node = node->next;
    }
}

// 销毁
void hash_table_destroy(HashTable *table)
{
    for (int i = 0; i < table->size; i++)
    {
        HashNode *node = table->buckets[i];
        while (node)
        {
            HashNode *tmp = node;
            node = node->next;
            free(tmp);
        }
    }
    free(table->buckets);
    free(table);
}