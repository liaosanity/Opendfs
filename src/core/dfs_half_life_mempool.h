#ifndef DFS_HALF_LIFE_MEM_POOL_H
#define DFS_HALF_LIFE_MEM_POOL_H

/*
 * not thread safe typedef struct hl_mempool_s hl_mempool_t;
 */
typedef struct hl_mempool_s hl_mempool_t;

hl_mempool_t* hl_mempool_create(int max_reserv_size, 
	int min_reserv_size, int elememt_size);
void *hl_mempool_get(hl_mempool_t* pool);
void  hl_mempool_free(hl_mempool_t* pool, void* ptr);
int   hl_mempool_get_free_size(hl_mempool_t* pool);
int   hl_mempool_reserve(hl_mempool_t* pool, int size);
void  hl_mempool_destroy(hl_mempool_t* pool);

#endif

