#ifndef HASH_TABLE_H
#define HASH_TBALE_H

#include <stdint.h>

// 哈希表中的节点，它是一个链表节点
typedef struct HashNode
{
    uint32_t key;          // 组合键 (space_id, page_no)
    int value;             // 值 (frame_index)
    struct HashNode *next; // 指向同一个桶中的下一个节点（拉链）
} HashNode;

// 哈希表结构
typedef struct
{
    int size;           // 哈希表的大小（桶的数量）
    HashNode **buckets; // 指向哈希桶数组的指针（数组里存的是HashNode指针）
} HashTable;

/**
 * @brief 创建一个新的哈希表
 * @param size 哈希表的大小
 * @return 创建的哈希表指针，失败则返回NULL
 */
HashTable *hash_table_create(int size);

/**
 * @brief 向哈希表中插入或更新一个键值对
 * @param table 哈希表指针
 * @param key 键
 * @param value 值
 */
void hash_table_put(HashTable *table, uint32_t key, int value);

/**
 * @brief 从哈希表中查找一个键对应的值
 * @param table 哈希表指针
 * @param key 键
 * @return 找到则返回值，否则返回-1
 */
int hash_table_get(HashTable *table, uint32_t key);

/**
 * @brief 从哈希表中移除一个键
 * @param table 哈希表指针
 * @param key 键
 */
void hash_table_remove(HashTable *table, uint32_t key);

/**
 * @brief 销毁哈希表，释放所有内存
 * @param table 哈希表指针
 */
void hash_table_destroy(HashTable *table);

#endif