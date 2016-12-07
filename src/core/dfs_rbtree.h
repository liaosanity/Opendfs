#ifndef DFS_RBTREE_H
#define DFS_RBTREE_H

#include "dfs_types.h"

#define RBTREE_COLOR_RED   1
#define RBTREE_COLOR_BLACK 0

typedef struct rbtree_node_s rbtree_node_t;

struct rbtree_node_s 
{
    rbtree_key     key;
    rbtree_node_t *left;
    rbtree_node_t *right;
    rbtree_node_t *parent;
    uchar_t        color;
    uchar_t        data;
};

typedef void (*rbtree_insert_pt) (rbtree_node_t *root,
    rbtree_node_t *node, rbtree_node_t *sentinel);

typedef struct rbtree_s 
{
    rbtree_node_t    *root;
    rbtree_node_t    *sentinel;
    rbtree_insert_pt  insert;
} rbtree_t;

void rbtree_init(_xvolatile rbtree_t *tree, rbtree_node_t *s, rbtree_insert_pt i);
void rbtree_sentinel_init(rbtree_node_t *s);
void rbtree_insert(_xvolatile rbtree_t *tree, rbtree_node_t *node);
void rbtree_delete(_xvolatile rbtree_t *tree, rbtree_node_t *node);
void rbtree_insert_value(rbtree_node_t *root,
    rbtree_node_t *node, rbtree_node_t *sentinel);
void rbtree_insert_timer_value(rbtree_node_t *root,
    rbtree_node_t *node, rbtree_node_t *sentinel);
rbtree_node_t *rbtree_min(rbtree_node_t *node, rbtree_node_t *sentinel);

#define rbtree_red(node)          ((node)->color = RBTREE_COLOR_RED)
#define rbtree_black(node)        ((node)->color = RBTREE_COLOR_BLACK)
#define rbtree_is_red(node)       ((node)->color == RBTREE_COLOR_RED)
#define rbtree_is_black(node)     ((node)->color == RBTREE_COLOR_BLACK)
#define rbtree_copy_color(n1, n2) (n1->color = n2->color)

#endif

