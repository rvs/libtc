/* Modified 2003 by Måns Rullgård.  Comments may not match code. */

/*
 * Dictionary Abstract Data Type
 * Copyright (C) 1997 Kaz Kylheku <kaz@ashi.footprints.net>
 *
 * Free Software License:
 *
 * All rights are reserved by the author, with the following exceptions:
 * Permission is granted to freely reproduce and distribute this software,
 * possibly in exchange for a fee, provided that this copyright notice appears
 * intact. Permission is also granted to adapt this software to produce
 * derivative works, as long as the modified versions carry this copyright
 * notice and additional notices stating that the work has been modified.
 * This source code may be translated into executable form and incorporated
 * into proprietary software; there is no requirement for such software to
 * contain a copyright notice related to this source.
 *
 * $Id: tree.c 1.8 04/03/02 18:35:12+01:00 mru@ford.pronto.tv $
 * $Name: <Not implemented> $
 */

#define NDEBUG 1

#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include <tctree.h>
#include <assert.h>

typedef enum { dnode_red, dnode_black } dnode_color_t;

typedef struct dnode_t {
    struct dnode_t *left;
    struct dnode_t *right;
    struct dnode_t *parent;
    dnode_color_t color;
    void *key;
} dnode_t;

struct tctree {
    dnode_t nilnode;
    int nodecount;
    tccompare_fn compare;
    int locking;
    pthread_mutex_t lock;
    uint32_t flags;
};

#define dict_root(D) ((D)->nilnode.left)
#define dict_nil(D) (&(D)->nilnode)
#define DICT_DEPTH_MAX 64

/*
 * Perform a ``left rotation'' adjustment on the tree.  The given node P and
 * its right child C are rearranged so that the P instead becomes the left
 * child of C.   The left subtree of C is inherited as the new right subtree
 * for P.  The ordering of the keys within the tree is thus preserved.
 */

static void rotate_left(dnode_t *upper)
{
    dnode_t *lower, *lowleft, *upparent;

    lower = upper->right;
    upper->right = lowleft = lower->left;
    lowleft->parent = upper;

    lower->parent = upparent = upper->parent;

    /* don't need to check for root node here because root->parent is
       the sentinel nil node, and root->parent->left points back to root */

    if (upper == upparent->left) {
	upparent->left = lower;
    } else {
	assert (upper == upparent->right);
	upparent->right = lower;
    }

    lower->left = upper;
    upper->parent = lower;
}

/*
 * This operation is the ``mirror'' image of rotate_left. It is
 * the same procedure, but with left and right interchanged.
 */

static void rotate_right(dnode_t *upper)
{
    dnode_t *lower, *lowright, *upparent;

    lower = upper->left;
    upper->left = lowright = lower->right;
    lowright->parent = upper;

    lower->parent = upparent = upper->parent;

    if (upper == upparent->right) {
	upparent->right = lower;
    } else {
	assert (upper == upparent->left);
	upparent->left = lower;
    }

    lower->right = upper;
    upper->parent = lower;
}

static inline void
tree_lock(tctree_t *t)
{
    if(t->locking)
	pthread_mutex_lock(&t->lock);
}

static inline void
tree_unlock(tctree_t *t)
{
    if(t->locking)
	pthread_mutex_unlock(&t->lock);
}

extern tctree_t *
tctree_new(int lock, tccompare_fn cmp, uint32_t flags)
{
    tctree_t *t = calloc(1, sizeof(*t));

    t->nilnode.left = &t->nilnode;
    t->nilnode.right = &t->nilnode;
    t->nilnode.parent = &t->nilnode;
    t->nilnode.color = dnode_black;
    t->compare = cmp;
    t->locking = lock;
    pthread_mutex_init(&t->lock, NULL);
    t->flags = flags;

    return t;
}

/*
 * Do a postorder traversal of the tree rooted at the specified
 * node and free everything under it.  Used by dict_free().
 */

static void
free_nodes(dnode_t *node, dnode_t *nil, tcfree_fn fr)
{
    if (node == nil)
	return;
    free_nodes(node->left, nil, fr);
    free_nodes(node->right, nil, fr);
    if(fr)
	fr(node->key);
    free(node);
}

extern int
tctree_destroy(tctree_t *t, tcfree_fn f)
{
    dnode_t *nil = dict_nil(t), *root = dict_root(t);
    free_nodes(root, nil, f);
    pthread_mutex_destroy(&t->lock);
    return 0;
}

/*
 * Locate a node in the dictionary having the given key.
 * If the node is not found, a null a pointer is returned (rather than 
 * a pointer that dictionary's nil sentinel node), otherwise a pointer to the
 * located node is returned.
 */

static dnode_t *
do_find(tctree_t *dict, void *key)
{
    dnode_t *root = dict_root(dict);
    dnode_t *nil = dict_nil(dict);
    int result;

    while (root != nil) {
	result = dict->compare(key, root->key);
	if (result < 0)
	    root = root->left;
	else if (result > 0)
	    root = root->right;
	else {
	    return root;
	}
    }

    return NULL;
}

extern int
tctree_find(tctree_t *dict, void *key, void *r)
{
    dnode_t *node;
    void **ret = r;

    tree_lock(dict);

    node = do_find(dict, key);
    if(node && ret)
	*ret = node->key;

    tree_unlock(dict);
    return !node;
}

/*
 * Insert a node into the dictionary. The node should have been
 * initialized with a data field. All other fields are ignored.
 * The behavior is undefined if the user attempts to insert into
 * a dictionary that is already full (for which the dict_isfull()
 * function returns true).
 */

static int
do_search(tctree_t *dict, void *key,  void *r, int replace)
{
    dnode_t *where = dict_root(dict), *nil = dict_nil(dict);
    dnode_t *parent = nil, *uncle, *grandpa;
    dnode_t *node;
    void **ret = r;
    int result = -1;

    tree_lock(dict);

    while (where != nil) {
	parent = where;
	result = dict->compare(key, where->key);
	if(result < 0){
	    where = where->left;
	} else if(result > 0){
	    where = where->right;
	} else {
	    *ret = where->key;
	    if(replace)
		where->key = key;
	    tree_unlock(dict);
	    return 0;
	}
    }

    if(ret)
	*ret = key;

    node = malloc(sizeof(*node));
    node->key = key;

    if (result < 0)
	parent->left = node;
    else
	parent->right = node;

    node->parent = parent;
    node->left = nil;
    node->right = nil;

    dict->nodecount++;

    /* red black adjustments */

    node->color = dnode_red;

    while (parent->color == dnode_red) {
	grandpa = parent->parent;
	if (parent == grandpa->left) {
	    uncle = grandpa->right;
	    if (uncle->color == dnode_red) {	/* red parent, red uncle */
		parent->color = dnode_black;
		uncle->color = dnode_black;
		grandpa->color = dnode_red;
		node = grandpa;
		parent = grandpa->parent;
	    } else {				/* red parent, black uncle */
	    	if (node == parent->right) {
		    rotate_left(parent);
		    parent = node;
		    assert (grandpa == parent->parent);
		    /* rotation between parent and child preserves grandpa */
		}
		parent->color = dnode_black;
		grandpa->color = dnode_red;
		rotate_right(grandpa);
		break;
	    }
	} else { 	/* symmetric cases: parent == parent->parent->right */
	    uncle = grandpa->left;
	    if (uncle->color == dnode_red) {
		parent->color = dnode_black;
		uncle->color = dnode_black;
		grandpa->color = dnode_red;
		node = grandpa;
		parent = grandpa->parent;
	    } else {
	    	if (node == parent->left) {
		    rotate_right(parent);
		    parent = node;
		    assert (grandpa == parent->parent);
		}
		parent->color = dnode_black;
		grandpa->color = dnode_red;
		rotate_left(grandpa);
		break;
	    }
	}
    }

    dict_root(dict)->color = dnode_black;

    tree_unlock(dict);
    return 1;
}

extern int
tctree_search(tctree_t *t, void *key, void *ret)
{
    return do_search(t, key, ret, 0);
}

extern int
tctree_replace(tctree_t *t, void *key, void *ret)
{
    return do_search(t, key, ret, 1);
}

/*
 * Return the given node's successor node---the node which has the
 * next key in the the left to right ordering. If the node has
 * no successor, a null pointer is returned rather than a pointer to
 * the nil node.
 */

dnode_t *dict_next(tctree_t *dict, dnode_t *curr)
{
    dnode_t *nil = dict_nil(dict), *parent, *left;

    if (curr->right != nil) {
	curr = curr->right;
	while ((left = curr->left) != nil)
	    curr = left;
	return curr;
    }

    parent = curr->parent;

    while (parent != nil && curr == parent->right) {
	curr = parent;
	parent = curr->parent;
    }

    return (parent == nil) ? NULL : parent;
}

/*
 * Delete the given node from the dictionary. If the given node does not belong
 * to the given dictionary, undefined behavior results.  A pointer to the
 * deleted node is returned.
 */

extern int
tctree_delete(tctree_t *dict, void *key, void *r)
{
    dnode_t *nil = dict_nil(dict), *child, *delparent, *delete;
    void **ret = r;

    tree_lock(dict);

    delete = do_find(dict, key);
    if(!delete){
	tree_unlock(dict);
	return 1;
    }

    delparent = delete->parent;

    if(ret)
	*ret = delete->key;

    /*
     * If the node being deleted has two children, then we replace it with its
     * successor (i.e. the leftmost node in the right subtree.) By doing this,
     * we avoid the traditional algorithm under which the successor's key and
     * value *only* move to the deleted node and the successor is spliced out
     * from the tree. We cannot use this approach because the user may hold
     * pointers to the successor, or nodes may be inextricably tied to some
     * other structures by way of embedding, etc. So we must splice out the
     * node we are given, not some other node, and must not move contents from
     * one node to another behind the user's back.
     */

    if (delete->left != nil && delete->right != nil) {
	dnode_t *next = dict_next(dict, delete);
	dnode_t *nextparent = next->parent;
	dnode_color_t nextcolor = next->color;

	assert (next != nil);
	assert (next->parent != nil);
	assert (next->left == nil);

	/*
	 * First, splice out the successor from the tree completely, by
	 * moving up its right child into its place.
	 */

	child = next->right;
	child->parent = nextparent;

	if (nextparent->left == next) {
	    nextparent->left = child;
	} else {
	    assert (nextparent->right == next);
	    nextparent->right = child;
	}

	/*
	 * Now that the successor has been extricated from the tree, install it
	 * in place of the node that we want deleted.
	 */

	next->parent = delparent;
	next->left = delete->left;
	next->right = delete->right;
	next->left->parent = next;
	next->right->parent = next;
	next->color = delete->color;
	delete->color = nextcolor;

	if (delparent->left == delete) {
	    delparent->left = next;
	} else {
	    assert (delparent->right == delete);
	    delparent->right = next;
	}

    } else {
	assert (delete != nil);
	assert (delete->left == nil || delete->right == nil);

	child = (delete->left != nil) ? delete->left : delete->right;

	child->parent = delparent = delete->parent;	    

	if (delete == delparent->left) {
	    delparent->left = child;    
	} else {
	    assert (delete == delparent->right);
	    delparent->right = child;
	}
    }

    delete->parent = NULL;
    delete->right = NULL;
    delete->left = NULL;

    dict->nodecount--;

    /* red-black adjustments */

    if (delete->color == dnode_black) {
	dnode_t *parent, *sister;

	dict_root(dict)->color = dnode_red;

	while (child->color == dnode_black) {
	    parent = child->parent;
	    if (child == parent->left) {
		sister = parent->right;
		assert (sister != nil);
		if (sister->color == dnode_red) {
		    sister->color = dnode_black;
		    parent->color = dnode_red;
		    rotate_left(parent);
		    sister = parent->right;
		    assert (sister != nil);
		}
		if (sister->left->color == dnode_black
			&& sister->right->color == dnode_black) {
		    sister->color = dnode_red;
		    child = parent;
		} else {
		    if (sister->right->color == dnode_black) {
			assert (sister->left->color == dnode_red);
			sister->left->color = dnode_black;
			sister->color = dnode_red;
			rotate_right(sister);
			sister = parent->right;
			assert (sister != nil);
		    }
		    sister->color = parent->color;
		    sister->right->color = dnode_black;
		    parent->color = dnode_black;
		    rotate_left(parent);
		    break;
		}
	    } else {	/* symmetric case: child == child->parent->right */
		assert (child == parent->right);
		sister = parent->left;
		assert (sister != nil);
		if (sister->color == dnode_red) {
		    sister->color = dnode_black;
		    parent->color = dnode_red;
		    rotate_right(parent);
		    sister = parent->left;
		    assert (sister != nil);
		}
		if (sister->right->color == dnode_black
			&& sister->left->color == dnode_black) {
		    sister->color = dnode_red;
		    child = parent;
		} else {
		    if (sister->left->color == dnode_black) {
			assert (sister->right->color == dnode_red);
			sister->right->color = dnode_black;
			sister->color = dnode_red;
			rotate_left(sister);
			sister = parent->left;
			assert (sister != nil);
		    }
		    sister->color = parent->color;
		    sister->left->color = dnode_black;
		    parent->color = dnode_black;
		    rotate_right(parent);
		    break;
		}
	    }
	}

	child->color = dnode_black;
	dict_root(dict)->color = dnode_black;
    }

    tree_unlock(dict);
    return 0;
}
