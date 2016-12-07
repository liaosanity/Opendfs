#include "dfs_rbtree.h"

/*
 * The red-black tree code is based on the algorithm described in
 * the "Introduction to Algorithms" by Cormen, Leiserson and Rivest.
 */
static _xinline void rbtree_left_rotate(rbtree_node_t **root,
    rbtree_node_t *sentinel, rbtree_node_t *node);
static _xinline void rbtree_right_rotate(rbtree_node_t **root,
    rbtree_node_t *sentinel, rbtree_node_t *node);

void rbtree_init(_xvolatile rbtree_t *tree, rbtree_node_t *s, 
	               rbtree_insert_pt i)
{
    rbtree_sentinel_init(s);
	
    if (tree) 
	{
        tree->root = s;
        tree->sentinel = s;
        tree->insert = i;
    }
}

void rbtree_sentinel_init(rbtree_node_t *s)
{
    if (s) 
	{
	    // sentinel must be black
        rbtree_black(s);
    }
}

void rbtree_insert(_xvolatile rbtree_t *tree, rbtree_node_t *node)
{
    rbtree_node_t **root = NULL;
    rbtree_node_t  *temp = NULL;
    rbtree_node_t  *sentinel = NULL;

    // a binary tree insert
    root = (rbtree_node_t **) &tree->root;
    sentinel = tree->sentinel;
    if (*root == sentinel) 
	{
        node->parent = NULL;
        node->left = sentinel;
        node->right = sentinel;
        rbtree_black(node);
        *root = node;
		
        return;
    }
	
    tree->insert(*root, node, sentinel);
	
    // re-balance tree
    while (node != *root && rbtree_is_red(node->parent)) 
	{
        if (node->parent == node->parent->parent->left) 
		{
            temp = node->parent->parent->right;
            if (rbtree_is_red(temp)) 
			{
                rbtree_black(node->parent);
                rbtree_black(temp);
                rbtree_red(node->parent->parent);
                node = node->parent->parent;
            } 
			else 
			{
                if (node == node->parent->right) 
				{
                    node = node->parent;
                    rbtree_left_rotate(root, sentinel, node);
                }
				
                rbtree_black(node->parent);
                rbtree_red(node->parent->parent);
                rbtree_right_rotate(root, sentinel, node->parent->parent);
            }
        } 
		else 
		{
            temp = node->parent->parent->left;
            if (rbtree_is_red(temp)) 
			{
                rbtree_black(node->parent);
                rbtree_black(temp);
                rbtree_red(node->parent->parent);
                node = node->parent->parent;
            } 
			else 
			{
                if (node == node->parent->left) 
				{
                    node = node->parent;
                    rbtree_right_rotate(root, sentinel, node);
                }
				
                rbtree_black(node->parent);
                rbtree_red(node->parent->parent);
                rbtree_left_rotate(root, sentinel, node->parent->parent);
            }
        }
    }
	
    rbtree_black(*root);
}

void rbtree_insert_value(rbtree_node_t *temp, rbtree_node_t *node, 
	                          rbtree_node_t *sentinel)
{
    rbtree_node_t **p  = NULL;

    for ( ;; ) 
	{
        p = (node->key < temp->key) ? &temp->left : &temp->right;
        if (*p == sentinel) 
		{
            break;
        }
		
        temp = *p;
    }
	
    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
	
    rbtree_red(node);
}

void rbtree_insert_timer_value(rbtree_node_t *root, rbtree_node_t *node, 
	                                  rbtree_node_t *sentinel)
{
    rbtree_node_t **p = NULL;
    rbtree_node_t  *last = NULL;

    if (!root || !node) 
	{
        return;
    }
	
    last = root;
	
    for ( ;; ) 
	{
        /*
         * Timer values
         * 1) are spread in small range, usually several minutes,
         * 2) and overflow each 49 days, if milliseconds are stored in 32 bits.
         * The comparison takes into account that overflow.
         */
 
        if ((rbtree_key_int) node->key < (rbtree_key_int) last->key) 
		{
            p = &last->left;
        } 
		else 
		{
            p =  &last->right;
        }
		
        if (*p == sentinel) 
		{
            break;
        }
 
        last = *p;
    }
	
    *p = node;
    node->parent = last;
    node->left = sentinel;
    node->right = sentinel;
	
    rbtree_red(node);
}

void rbtree_delete(_xvolatile rbtree_t *tree, rbtree_node_t *node)
{
    uint32_t        red = 0;
    rbtree_node_t **root, *sentinel, *subst, *temp, *w;

    // a binary tree delete
    root = (rbtree_node_t **) &tree->root;
    sentinel = tree->sentinel;
	
    if (node->left == sentinel) 
	{
        temp = node->right;
        subst = node;
    } 
	else if (node->right == sentinel) 
	{
        temp = node->left;
        subst = node;
    } 
	else 
	{
        subst = rbtree_min(node->right, sentinel);
        if (subst->left != sentinel) 
		{
            temp = subst->left;
        } 
		else 
		{
            temp = subst->right;
        }
    }
	
    if (subst == *root) 
	{
        *root = temp;
        rbtree_black(temp);
        node->left = NULL;
        node->right = NULL;
        node->parent = NULL;
        node->key = 0;
		
        return;
    }
	
    red = rbtree_is_red(subst);
    if (subst == subst->parent->left) 
	{
        subst->parent->left = temp;
    } 
	else 
	{
        subst->parent->right = temp;
    }
	
    if (subst == node) 
	{
        temp->parent = subst->parent;
    } 
	else 
	{
        if (subst->parent == node) 
		{
            temp->parent = subst;
        } 
		else 
		{
            temp->parent = subst->parent;
        }
		
        subst->left = node->left;
        subst->right = node->right;
        subst->parent = node->parent;
		
        rbtree_copy_color(subst, node);
		
        if (node == *root)
		{
            *root = subst;
        } 
		else 
		{
            if (node == node->parent->left) 
			{
                node->parent->left = subst;
            } 
			else 
			{
                node->parent->right = subst;
            }
        }
		
        if (subst->left != sentinel) 
		{
            subst->left->parent = subst;
        }
		
        if (subst->right != sentinel) 
		{
            subst->right->parent = subst;
        }
    }
  
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->key = 0;
	
    if (red) 
	{
        return;
    }

    while (temp != *root && rbtree_is_black(temp)) 
	{
        if (temp == temp->parent->left) 
		{
            w = temp->parent->right;
            if (rbtree_is_red(w)) 
			{
                rbtree_black(w);
                rbtree_red(temp->parent);
                rbtree_left_rotate(root, sentinel, temp->parent);
                w = temp->parent->right;
            }
 
            if (rbtree_is_black(w->left) && rbtree_is_black(w->right)) 
			{
                rbtree_red(w);
                temp = temp->parent;
            } 
			else 
			{
                if (rbtree_is_black(w->right)) 
				{
                    rbtree_black(w->left);
                    rbtree_red(w);
                    rbtree_right_rotate(root, sentinel, w);
                    w = temp->parent->right;
                }
				
                rbtree_copy_color(w, temp->parent);
                rbtree_black(temp->parent);
                rbtree_black(w->right);
                rbtree_left_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        } 
		else 
		{
            w = temp->parent->left;
            if (rbtree_is_red(w)) 
			{
                rbtree_black(w);
                rbtree_red(temp->parent);
                rbtree_right_rotate(root, sentinel, temp->parent);
                w = temp->parent->left;
            }
			
            if (rbtree_is_black(w->left) && rbtree_is_black(w->right)) 
			{
                rbtree_red(w);
                temp = temp->parent;
            } 
			else 
			{
                if (rbtree_is_black(w->left)) 
				{
                    rbtree_black(w->right);
                    rbtree_red(w);
                    rbtree_left_rotate(root, sentinel, w);
                    w = temp->parent->left;
                }
				
                rbtree_copy_color(w, temp->parent);
                rbtree_black(temp->parent);
                rbtree_black(w->left);
                rbtree_right_rotate(root, sentinel, temp->parent);
				
                temp = *root;
            }
        }
    }
	
    rbtree_black(temp);
	
    return;
}

static _xinline void rbtree_left_rotate(rbtree_node_t **root, 
	                                       rbtree_node_t *sentinel, 
	                                       rbtree_node_t *node)
{
    rbtree_node_t *temp = NULL;

    temp = node->right;
    node->right = temp->left;
	
    if (temp->left != sentinel) 
	{
        temp->left->parent = node;
    }
	
    temp->parent = node->parent;
    if (node == *root) 
	{
        *root = temp;
    } 
	else if (node == node->parent->left) 
	{
        node->parent->left = temp;
    } 
	else 
	{
        node->parent->right = temp;
    }
	
    temp->left = node;
    node->parent = temp;
	
    return;
}

static _xinline void rbtree_right_rotate(rbtree_node_t **root, 
	                                         rbtree_node_t *sentinel, 
	                                         rbtree_node_t *node)
{
    rbtree_node_t *temp = NULL;

    temp = node->left;
    node->left = temp->right;
	
    if (temp->right != sentinel) 
	{
        temp->right->parent = node;
    }
	
    temp->parent = node->parent;
	
    if (node == *root) 
	{
        *root = temp;
    } 
	else if (node == node->parent->right) 
	{
        node->parent->right = temp;
    } 
	else 
	{
        node->parent->left = temp;
    }
	
    temp->right = node;
    node->parent = temp;
}

rbtree_node_t * rbtree_min(rbtree_node_t *node, rbtree_node_t *sentinel)
{
    while (node->left != sentinel) 
	{
        node = node->left;
    }
    
    return node;
}

