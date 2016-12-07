#include <string.h>

#include "dfs_mblks.h"

struct mem_mblks * mem_mblks_new_fn(size_t sizeof_type, int64_t count, 
                                              mem_mblks_param_t *param)
{
    struct mem_mblks *mblks = NULL;
    struct mem_data  *ptr = NULL;

    int    idx = 0;
    size_t sizeof_mblks = 0;

    if (!param || !param->mem_alloc) 
	{
        return NULL;
    }

    sizeof_mblks = SIZEOF_PER_MEM_BLOCK(sizeof_type) * count 
		+ sizeof(struct mem_mblks);

    mblks = (struct mem_mblks *)param->mem_alloc(param->priv, sizeof_mblks);
    if (!mblks) 
	{
        return NULL;
    }

    memset(mblks, 0, sizeof_mblks);

    LOCK_INIT(&mblks->lock);
    mblks->cold_count = count;
    mblks->padded_sizeof_type = SIZEOF_PER_MEM_BLOCK(sizeof_type);
    mblks->real_sizeof_type = sizeof_type;
    mblks->param = *param;
    mblks->free_blks = (struct mem_data*)((void *)mblks 
		+ sizeof(struct mem_mblks));
    
    for (ptr = mblks->free_blks, idx = 0; idx < count; idx++) 
	{
        ptr->next = (void *)ptr + mblks->padded_sizeof_type;
        ptr = (struct mem_data *)ptr->next;
    }
	
    return mblks;
}

void * mem_get(struct mem_mblks *mblks)
{
    struct mem_data *pdata = NULL;

    if (!mblks) 
	{
        return NULL;
    }

    LOCK(&mblks->lock);
    {
        if (mblks->cold_count) 
		{
            pdata = mblks->free_blks;
            mblks->free_blks = (struct mem_data *)pdata->next;

            pdata->next = (void *)mblks;

            mblks->hot_count++;
            mblks->cold_count--;
            
        } 
		else 
		{
            UNLOCK(&mblks->lock);
			
            return NULL;
        }
    }
    UNLOCK(&mblks->lock);

    return (void *)pdata->data;
}

void * mem_get0(struct mem_mblks *mblks)
{
	void *ptr = NULL;
	
	if (!mblks) 
	{
		return NULL;
	}
	
	ptr = mem_get(mblks);
    if (!ptr) 
	{
        return NULL;
    }

	memset(ptr, 0, mblks->real_sizeof_type);
	
	return ptr;
}

void mem_put(void *ptr)
{
    struct mem_data *pdata = NULL;
    struct mem_mblks *mblks = NULL;

    if (!ptr) 
	{
        return;
    }

    pdata = (struct mem_data*) ((void *) ptr - MEM_BLOCK_HEAD);
    if (!pdata) 
	{
        return;
    }

    mblks = (struct mem_mblks *)pdata->next;
    if (!mblks) 
	{
        return;
    }

    LOCK(&mblks->lock);
    {
        mblks->hot_count--;
        mblks->cold_count++;

        pdata->next = mblks->free_blks;
        mblks->free_blks = pdata;
    }
    UNLOCK(&mblks->lock);

    return;
}

void mem_mblks_destroy(struct mem_mblks *mblks) 
{
    assert(mblks);

    mem_mblks_param_t *param = &mblks->param;
    if (!param) 
	{
        return;
    }

    LOCK_DESTROY(&mblks->lock);

    param->mem_free(param->priv, mblks);
    
    return;
}

