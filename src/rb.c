/*
 * Copyright (c) 2019 xieqing. https://github.com/xieqing
 * May be freely redistributed, but copyright notice must be retained.
 */

#include <stdio.h>
#include <stdlib.h>
#include "rb.h"
#include "file.h"

static void insert_repair(rbtree *rbt, size_t current_ptr);
static void delete_repair(rbtree *rbt, size_t current_ptr);
static void rotate_left(rbtree *, size_t);
static void rotate_right(rbtree *, size_t);
static int check_order(rbtree *rbt, size_t n_ptr, void *min, void *max);
static int check_black_height(rbtree *rbt, size_t node_ptr);
static void print(rbtree *rbt, size_t n_ptr, void (*print_func)(void *), int depth, char *label);
static void destroy(rbtree *rbt, rbnode *node);

static size_t read_rb_data_by_index(rbtree *rbt, size_t node_ptr, size_t rb_index) {
  if (!(node_ptr+1)) {
    return BLACK;
  }
  size_t rb_data;
  TABLE_STATE* ts = rbt->table_state;
  size_t offset = entry_offset(node_ptr, ts) + rbt->col * RB_DATA_SIZE + sizeof(size_t) * rb_index;
  fseek(ts->file, offset, SEEK_SET);
  fread(&rb_data, sizeof(size_t), 1, ts->file);
  return rb_data;
}

static size_t get_left  (rbtree *rbt, size_t node_ptr) {
  return read_rb_data_by_index(rbt, node_ptr, RB_INDEX_LEFT);
}
static size_t get_right (rbtree *rbt, size_t node_ptr) {
  return read_rb_data_by_index(rbt, node_ptr, RB_INDEX_RIGHT);
}
static size_t get_parent(rbtree *rbt, size_t node_ptr) {
  return read_rb_data_by_index(rbt, node_ptr, RB_INDEX_PARENT);
}
static size_t get_color (rbtree *rbt, size_t node_ptr) {
  return read_rb_data_by_index(rbt, node_ptr, RB_INDEX_COLOR);
}

static size_t write_rb_data_by_index(rbtree *rbt, size_t node_ptr, size_t value, size_t rb_index) {
  assert(node_ptr+1);
  TABLE_STATE* ts = rbt->table_state;
  size_t offset = entry_offset(node_ptr, ts) + rbt->col * RB_DATA_SIZE + sizeof(size_t) * rb_index;
  fseek(ts->file, offset, SEEK_SET);
  return fwrite(&value, sizeof(size_t), 1, ts->file);
}


static size_t set_left_from_root (rbtree *rbt, size_t node_ptr, size_t value) {
  return write_rb_head(rbt->col, value, rbt->table_state);
}
static size_t set_left (rbtree *rbt, size_t node_ptr, size_t value) {
  return write_rb_data_by_index(rbt, node_ptr, value, RB_INDEX_LEFT);
}
static size_t set_right (rbtree *rbt, size_t node_ptr, size_t value) {
  return write_rb_data_by_index(rbt, node_ptr, value, RB_INDEX_RIGHT);
}
static size_t set_parent (rbtree *rbt, size_t node_ptr, size_t value) {
  return write_rb_data_by_index(rbt, node_ptr, value, RB_INDEX_PARENT);
}
static size_t set_color (rbtree *rbt, size_t node_ptr, size_t value) {
  return write_rb_data_by_index(rbt, node_ptr, value, RB_INDEX_COLOR);
}

static size_t set_free  (rbtree *rbt, size_t node_ptr) {
  assert(0 && "Not impl yet");
}

static size_t set_rb_data(rbtree *rbt, void *data, void *rb_data) {
  memcpy(data + rbt->col * RB_DATA_SIZE, rb_data, RB_DATA_SIZE);
  return 0;
}

static void* get_data(rbtree *rbt, size_t node_ptr) {
  TABLE_STATE* ts = rbt->table_state;
  size_t offset = entry_offset(node_ptr, ts);
  fseek(ts->file, offset, SEEK_SET);
  fread(rbt->copy_data, ts->entry_raw_size, 1, ts->file);
  return rbt->copy_data;
}

#define RB_ROOT_PTR 0
#define RB_NIL_PTR (-1)

static size_t RB_FIRST_PTR(rbtree* rbt) {
  return get_left(rbt, 0);
}

rbtree* rb_restore_from_table(size_t col, TABLE_STATE* table_state, int (*cmp)(const void*, const void*, const void*)) {
  rbtree* rbt = rb_create(cmp, _destroy);
  rbt->table_state = table_state;
  rbt->col = col;
  rbt->copy_data = malloc(table_state->entry_raw_size);
  set_color(rbt, 0, BLACK);
  return rbt;
}

/*
 * construction
 * return NULL if out of memory
 */
rbtree *rb_create(int (*compare)(const void*, const void *, const void *), void (*destroy)(void *))
{
	rbtree *rbt;

	rbt = (rbtree *) malloc(sizeof(rbtree));
	if (rbt == NULL)
		return NULL; /* out of memory */

	rbt->compare = compare;
	rbt->destroy = destroy;

	/* sentinel node nil */
	rbt->nil.left = rbt->nil.right = rbt->nil.parent = RB_NIL(rbt);
	rbt->nil.color = BLACK;
	rbt->nil.data = NULL;

	/* sentinel node root */
	rbt->root.left = rbt->root.right = rbt->root.parent = RB_NIL(rbt);
	rbt->root.color = BLACK;
	rbt->root.data = NULL;

	#ifdef RB_MIN
	rbt->min_ptr = RB_NIL_PTR; // = NULL;
	#endif

  rbt->table_state = NULL;
	
	return rbt;
}

/*
 * destruction
 */
void rb_destroy(rbtree *rbt)
{
	// destroy(rbt, RB_FIRST(rbt));
  if (rbt->copy_data)
    free(rbt->copy_data);
	free(rbt);
}

/*
 * look up
 * return NULL if not found
 */
size_t rb_find(rbtree *rbt, void *data)
{
	//rbnode *p;
  size_t p_ptr;

	//p = RB_FIRST(rbt);
  p_ptr = RB_FIRST_PTR(rbt);

	while (p_ptr != RB_NIL_PTR) { // != RB_NIL(rbt)) {
		int cmp;
    void* d = get_data(rbt, p_ptr);
		cmp = rbt->compare(rbt, data, d);
		if (cmp == 0)
			return p_ptr; /* found */
		// p = cmp < 0 ? p->left : p->right;
    p_ptr = cmp < 0 ? get_left(rbt, p_ptr) : get_right(rbt, p_ptr);
	}

	return 0; /* not found */
}

/*
 * next larger
 * return NULL if not found
 */
size_t rb_successor(rbtree *rbt, size_t node_ptr)
{
	// rbnode *p;
  size_t p_ptr;

	// p = node->right;
  p_ptr = get_right(rbt, node_ptr);

	if (p_ptr != RB_NIL_PTR) {
		/* move down until we find it */
		// for ( ; p->left != RB_NIL_PTR(rbt); p = p->left) ;
    size_t pleft = get_left(rbt, p_ptr);
    for (; pleft != RB_NIL_PTR; ) {
      p_ptr = pleft;
      pleft = get_left(rbt, p_ptr);
    }
	} else {
		/* move up until we find it or hit the root */
		// for (p = node->parent; node == p->right; node = p, p = p->parent) ;
    for (p_ptr = get_parent(rbt, node_ptr); node_ptr == get_right(rbt, p_ptr); node_ptr = p_ptr, p_ptr = get_parent(rbt, p_ptr));

		if (p_ptr == RB_ROOT_PTR)// RB_ROOT_PTR(rbt))
			p_ptr = RB_NIL_PTR; /* not found */
	}

	return p_ptr;
}

/*
 * apply func
 * return non-zero if error
 */
int rb_apply(rbtree *rbt, size_t node_ptr, int (*func)(void *, void *), void *cookie, enum rbtraversal order)
{
	int err;


	if (node_ptr != RB_NIL_PTR) { //RB_NIL(rbt)) {
    void* d = get_data(rbt, node_ptr);
		if (order == PREORDER && (err = func(d, cookie)) != 0) /* preorder */
			return err;
		if ((err = rb_apply(rbt, get_left(rbt, node_ptr), func, cookie, order)) != 0) /* left */
			return err;
		if (order == INORDER && (err = func(d, cookie)) != 0) /* inorder */
			return err;
		if ((err = rb_apply(rbt, get_right(rbt, node_ptr), func, cookie, order)) != 0) /* right */
			return err;
		if (order == POSTORDER && (err = func(d, cookie)) != 0) /* postorder */
			return err;
	}

	return 0;
}

/*
 * rotate left about x
 */
void rotate_left(rbtree *rbt, size_t x_ptr)
{
	// rbnode *y;


	// y = x->right; /* child */
  size_t y_ptr = get_right(rbt, x_ptr);

	/* tree x */
	// x->right = y->left;
  set_right(rbt, x_ptr, get_left(rbt, y_ptr));
	// if (x->right != RB_NIL_PTR(rbt)) {
  if (get_right(rbt, x_ptr) != RB_NIL_PTR) {
		// x->right->parent = x;
    set_parent(rbt, get_right(rbt, x_ptr), x_ptr);
    //rb_table_update(rbt, x);
  }

	/* tree y */
	// y->parent = x->parent;
  set_parent(rbt, y_ptr, get_parent(rbt, x_ptr));
	// if (x == x->parent->left)
  if (x_ptr == get_left(rbt, get_parent(rbt, x_ptr)))
		// x->parent->left = y;
    set_left(rbt, get_parent(rbt, x_ptr), y_ptr);
	else
		// x->parent->right = y;
    set_right(rbt, get_parent(rbt, x_ptr), y_ptr);
  //rb_table_update(rbt, x->parent);

	/* assemble tree x and tree y */
	// y->left = x;
  set_left(rbt, y_ptr, x_ptr);
  //rb_table_update(rbt, y);

	// x->parent = y;
  set_parent(rbt, x_ptr, y_ptr);
  //rb_table_update(rbt, x);
}

/*
 * rotate right about x
 */
void rotate_right(rbtree *rbt, size_t x_ptr)
{
	// rbnode *y;

	// y = x->left; /* child */
  size_t y_ptr = get_left(rbt, x_ptr);

	/* tree x */
	// x->left = y->right;
  set_left(rbt, x_ptr, get_right(rbt, y_ptr));
	// if (x->left != RB_NIL_PTR(rbt)) {
  if (get_left(rbt, x_ptr) != RB_NIL_PTR) {
		// x->left->parent = x;
    set_parent(rbt, get_left(rbt, x_ptr), x_ptr);
    //rb_table_update(rbt, x->left);
  }

	/* tree y */
	// y->parent = x->parent;
  set_parent(rbt, y_ptr, get_parent(rbt, x_ptr));
	// if (x == x->parent->left)
  if (x_ptr == get_left(rbt, get_parent(rbt, x_ptr)))
		// x->parent->left = y;
    set_left(rbt, get_parent(rbt, x_ptr), y_ptr);
	else
		// x->parent->right = y;
    set_right(rbt, get_parent(rbt, x_ptr), y_ptr);
  //rb_table_update(rbt, x->parent);

	/* assemble tree x and tree y */
	// y->right = x;
  set_right(rbt, y_ptr, x_ptr);
  //rb_table_update(rbt, y);
	// x->parent = y;
  set_parent(rbt, x_ptr, y_ptr);
  //rb_table_update(rbt, x);
}


/*
 * insert (or update) data
 * return NULL if out of memory
 */
size_t rb_insert(rbtree *rbt, void *data)
{
	// rbnode *current, *parent;
	// rbnode *new_node;
  size_t current_ptr, parent_ptr;
  size_t new_node_ptr;

	/* do a binary search to find where it should be */

	// current = RB_FIRST(rbt);
	// parent = RB_ROOT_PTR(rbt);
  current_ptr = RB_FIRST_PTR(rbt);
  parent_ptr = RB_ROOT_PTR;

	// while (current != RB_NIL_PTR(rbt)) {
  while (current_ptr != RB_NIL_PTR) {
		int cmp;
    void* d = get_data(rbt, current_ptr);
		cmp = rbt->compare(rbt, data, d);

		if (cmp == 0) {
			// rbt->destroy(current->data);
			// current->data = data;
			// return current; /* updated */
      // assert(0 && "repeating value");
      return 0;
		}

		parent_ptr = current_ptr;
		current_ptr = cmp < 0 ? get_left(rbt, current_ptr) : get_right(rbt, current_ptr);
	}

	/* replace the termination NIL pointer with the new node pointer */

	// current = new_node = (rbnode *) malloc(sizeof(rbnode));

	// if (current == NULL)
	//	return NULL; /* out of memory */

	// current->left = current->right = RB_NIL_PTR(rbt);
	// current->parent = parent;
	// current->color = RED;
	// current->data = data;
  current_ptr = get_stage_next_free(rbt->table_state);
  // size_t rb_data[RB_DATA_LEN];
  // rb_data[RB_INDEX_PARENT] = parent_ptr;
  // rb_data[RB_INDEX_LEFT] = rb_data[RB_INDEX_RIGHT] = 0;
  // rb_data[RB_INDEX_COLOR] = RED;
  // set_rb_data(rbt, data, rb_data); // TODO : ?
  set_parent(rbt, current_ptr, parent_ptr);
  set_left(rbt, current_ptr, RB_NIL_PTR);
  set_right(rbt, current_ptr, RB_NIL_PTR);
  set_color(rbt, current_ptr, RED);

  
	if (parent_ptr == RB_ROOT_PTR || rbt->compare(rbt, data, get_data(rbt, parent_ptr)) < 0)
		// parent->left = current;
    set_left(rbt, parent_ptr, current_ptr);
	else
		// parent->right = current;
    set_right(rbt, parent_ptr, current_ptr);
  //rb_table_update(rbt, parent);

	#ifdef RB_MIN
	if (rbt->min_ptr == RB_NIL_PTR || rbt->compare(rbt, get_data(rbt, current_ptr), get_data(rbt, rbt->min_ptr)) < 0)
		rbt->min_ptr = current_ptr;
	#endif
	
	/*
	 * insertion into a red-black tree:
	 *   0-children root cluster (parent node is BLACK) becomes 2-children root cluster (new root node)
	 *     paint root node BLACK, and done
	 *   2-children cluster (parent node is BLACK) becomes 3-children cluster
	 *     done
	 *   3-children cluster (parent node is BLACK) becomes 4-children cluster
	 *     done
	 *   3-children cluster (parent node is RED) becomes 4-children cluster
	 *     rotate, and done
	 *   4-children cluster (parent node is RED) splits into 2-children cluster and 3-children cluster
	 *     split, and insert grandparent node into parent cluster
	 */
	// if (current->parent->color == RED) {
  if (get_color(rbt, get_parent(rbt, current_ptr)) == RED) {
		/* insertion into 3-children cluster (parent node is RED) */
		/* insertion into 4-children cluster (parent node is RED) */
		insert_repair(rbt, current_ptr);
	} else {
		/* insertion into 0-children root cluster (parent node is BLACK) */
		/* insertion into 2-children cluster (parent node is BLACK) */
		/* insertion into 3-children cluster (parent node is BLACK) */
	}

	/*
	 * the root is always BLACK
	 * insertion into 0-children root cluster or insertion into 4-children root cluster require this recoloring
	 */
	// RB_FIRST(rbt)->color = BLACK;
  set_color(rbt, RB_FIRST_PTR(rbt), BLACK);
	
	return new_node_ptr;
}

/*
 * rebalance after insertion
 * RB_ROOT_PTR(rbt) is always BLACK, thus never reach beyond RB_FIRST(rbt)
 * after insert_repair, RB_FIRST(rbt) might be RED
 */
void insert_repair(rbtree *rbt, size_t current_ptr)
{
	// rbnode *uncle;
  size_t uncle_ptr;

	do {
		/* current node is RED and parent node is RED */

		// if (current->parent == current->parent->parent->left) {
    if (get_parent(rbt, current_ptr) == get_left(rbt, get_parent(rbt, get_parent(rbt, current_ptr)))) {
			// uncle = current->parent->parent->right;
      uncle_ptr = get_right(rbt, get_parent(rbt, get_parent(rbt, current_ptr)));
			// if (uncle->color == RED) {
      if (get_color(rbt, uncle_ptr) == RED) {
				/* insertion into 4-children cluster */

				/* split */
				// current->parent->color = BLACK;
        set_color(rbt, get_parent(rbt, current_ptr), BLACK);
        //rb_table_update(rbt, current->parent);
				// uncle->color = BLACK;
        set_color(rbt, uncle_ptr, BLACK);
        // //rb_table_update(rbt, uncle);

				/* send grandparent node up the tree */
				// current = current->parent->parent; /* goto loop or break */
        current_ptr = get_parent(rbt, get_parent(rbt, current_ptr));
				// current->color = RED;
        set_color(rbt, current_ptr, RED);
        //rb_table_update(rbt, current);
			} else {
				/* insertion into 3-children cluster */

				/* equivalent BST */
				// if (current == current->parent->right) {
        if (current_ptr == get_right(rbt, get_parent(rbt, current_ptr))) {
					// current = current->parent;
          current_ptr = get_parent(rbt, current_ptr);
					rotate_left(rbt, current_ptr);
				}

				/* 3-children cluster has two representations */
				// current->parent->color = BLACK; /* thus goto break */
        set_color(rbt, get_parent(rbt, current_ptr), BLACK);
        //rb_table_update(rbt, current->parent);
				// current->parent->parent->color = RED;
        set_color(rbt, get_parent(rbt, get_parent(rbt, current_ptr)), RED);
        //rb_table_update(rbt, current->parent->parent);
				// rotate_right(rbt, current->parent->parent);
        rotate_right(rbt, get_parent(rbt, get_parent(rbt, current_ptr)));
			}
		} else {
			// uncle = current->parent->parent->left;
      uncle_ptr = get_left(rbt, get_parent(rbt, get_parent(rbt, current_ptr)));
			// if (uncle->color == RED) {
      if (get_color(rbt, uncle_ptr) == RED) {
				/* insertion into 4-children cluster */

				/* split */
				// current->parent->color = BLACK;
        set_color(rbt, get_parent(rbt, current_ptr), BLACK);
        //rb_table_update(rbt, current->parent);
				// uncle->color = BLACK;
        set_color(rbt, uncle_ptr, BLACK);
        //rb_table_update(rbt, uncle);

				/* send grandparent node up the tree */
				// current = current->parent->parent; /* goto loop or break */
        current_ptr = get_parent(rbt, get_parent(rbt, current_ptr));
				// current->color = RED;
        set_color(rbt, current_ptr, RED);
        //rb_table_update(rbt, current);
			} else {
				/* insertion into 3-children cluster */

				/* equivalent BST */
				// if (current == current->parent->left) {
        if (current_ptr == get_left(rbt, get_parent(rbt, current_ptr))) {
					// current = current->parent;
          current_ptr = get_parent(rbt, current_ptr);
					rotate_right(rbt, current_ptr);
				}

				/* 3-children cluster has two representations */
				// current->parent->color = BLACK; /* thus goto break */
        set_color(rbt, get_parent(rbt, current_ptr), BLACK);
        //rb_table_update(rbt, current->parent);
				// current->parent->parent->color = RED;
        set_color(rbt, get_parent(rbt, get_parent(rbt, current_ptr)), RED);
        //rb_table_update(rbt, current->parent->parent);
				// rotate_left(rbt, current->parent->parent);
        rotate_left(rbt, get_parent(rbt, get_parent(rbt, current_ptr)));
			}
		}
	// } while (current->parent->color == RED);
  } while (get_color(rbt, get_parent(rbt, current_ptr)) == RED);
}

/*
 * delete node
 * return NULL if keep is zero (already freed)
 */
void rb_delete(rbtree *rbt, size_t node_ptr, int keep)
{
	// rbnode *target, *child;
  size_t target_ptr, child_ptr;
	void *data;
	
	data = get_data(rbt, node_ptr); //->data;

	/* choose node's in-order successor if it has two children */
	
	// if (node->left == RB_NIL_PTR(rbt) || node->right == RB_NIL(rbt)) {
  if (get_left(rbt, node_ptr) == RB_NIL_PTR || get_right(rbt, node_ptr) == RB_NIL_PTR) {
		target_ptr = node_ptr;

		#ifdef RB_MIN
		if (rbt->min_ptr == target_ptr)
			rbt->min_ptr = rb_successor(rbt, target_ptr); /* deleted, thus min = successor */
		#endif
	} else {
		target_ptr = rb_successor(rbt, node_ptr); /* node->right must not be NIL, thus move down */

		// node->data = target->data; /* data swapped */

		#ifdef RB_MIN
		/* if min == node, then min = successor = node (swapped), thus idle */
		/* if min == target, then min = successor, which is not the minimal, thus impossible */
		#endif
	}

	// child = (target->left == RB_NIL_PTR(rbt)) ? target->right : target->left; /* child may be NIL */
  child_ptr = (get_left(rbt, target_ptr) == RB_NIL_PTR ? get_right(rbt, target_ptr) : get_left(rbt, target_ptr));

	/*
	 * deletion from red-black tree
	 *   4-children cluster (RED target node) becomes 3-children cluster
	 *     done
	 *   3-children cluster (RED target node) becomes 2-children cluster
	 *     done
	 *   3-children cluster (BLACK target node, RED child node) becomes 2-children cluster
	 *     paint child node BLACK, and done
	 *
	 *	 2-children root cluster (BLACK target node, BLACK child node) becomes 0-children root cluster
	 *     done
	 *
	 *   2-children cluster (BLACK target node, 4-children sibling cluster) becomes 3-children cluster
	 *     transfer, and done
	 *   2-children cluster (BLACK target node, 3-children sibling cluster) becomes 2-children cluster
	 *     transfer, and done
	 *
	 *   2-children cluster (BLACK target node, 2-children sibling cluster, 3/4-children parent cluster) becomes 3-children cluster
	 *     fuse, paint parent node BLACK, and done
	 *   2-children cluster (BLACK target node, 2-children sibling cluster, 2-children parent cluster) becomes 3-children cluster
	 *     fuse, and delete parent node from parent cluster
	 */
	// if (target->color == BLACK) {
  if (get_color(rbt, target_ptr) == BLACK) {
		// if (child->color == RED) {
    if (get_color(rbt, child_ptr) == RED) {
			/* deletion from 3-children cluster (BLACK target node, RED child node) */
			// child->color = BLACK;
      set_color(rbt, child_ptr, BLACK);
      //rb_table_update(rbt, child);
		} else if (target_ptr == RB_FIRST_PTR(rbt)) {
			/* deletion from 2-children root cluster (BLACK target node, BLACK child node) */
		} else {
			/* deletion from 2-children cluster (BLACK target node, ...) */
			delete_repair(rbt, target_ptr);
		}
	} else {
		/* deletion from 4-children cluster (RED target node) */
		/* deletion from 3-children cluster (RED target node) */
	}

	if (child_ptr != RB_NIL_PTR) { // RB_NIL(rbt)) {
		// child->parent = target->parent;
    set_parent(rbt, child_ptr, get_parent(rbt, target_ptr));
    //rb_table_update(rbt, child);
  }

	// if (target == target->parent->left)
  if (target_ptr == get_left(rbt, get_parent(rbt, target_ptr)))
		// target->parent->left = child;
    set_left(rbt, get_parent(rbt, target_ptr), child_ptr);
	else
		// target->parent->right = child;
    set_right(rbt, get_parent(rbt, target_ptr), child_ptr);
  //rb_table_update(rbt, target->parent);

	set_free(rbt, target_ptr);
	
	/* keep or discard data */
	if (keep == 0) {
		// rbt->destroy(data);
		// data = NULL;
	}

	// return data;
}

/*
 * rebalance after deletion
 */
void delete_repair(rbtree *rbt, size_t current_ptr)
{
	// rbnode *sibling;
  size_t sibling_ptr;
	do {
		// if (current == current->parent->left) {
    if (current_ptr == get_left(rbt, get_parent(rbt, current_ptr))) {
			// sibling = current->parent->right;
      sibling_ptr = get_right(rbt, get_parent(rbt, current_ptr));

			// if (sibling->color == RED) {
      if (get_color(rbt, sibling_ptr) == RED) {
				/* perform an adjustment (3-children parent cluster has two representations) */
				// sibling->color = BLACK;
        set_color(rbt, sibling_ptr, BLACK);
        //rb_table_update(rbt, sibling);
				// current->parent->color = RED;
        set_color(rbt, get_parent(rbt, current_ptr), RED);
        //rb_table_update(rbt, current->parent);
				// rotate_left(rbt, current->parent);
        rotate_left(rbt, get_parent(rbt, current_ptr));
				// sibling = current->parent->right;
        sibling_ptr = get_right(rbt, get_parent(rbt, current_ptr));
			}

			/* sibling node must be BLACK now */

			// if (sibling->right->color == BLACK && sibling->left->color == BLACK) {
      if (get_color(rbt, get_right(rbt, sibling_ptr)) == BLACK && get_color(rbt, get_left(rbt, sibling_ptr)) == BLACK) {
				/* 2-children sibling cluster, fuse by recoloring */
				// sibling->color = RED;
        set_color(rbt, sibling_ptr, RED);
        //rb_table_update(rbt, sibling);
				// if (current->parent->color == RED) { /* 3/4-children parent cluster */
        if (get_color(rbt, get_parent(rbt, current_ptr)) == RED) {
					// current->parent->color = BLACK;
          set_color(rbt, get_parent(rbt, current_ptr), BLACK);
          //rb_table_update(rbt, current->parent);
					break; /* goto break */
				} else { /* 2-children parent cluster */
					// current = current->parent; /* goto loop */
          current_ptr = get_parent(rbt, current_ptr);
				}
			} else {
				/* 3/4-children sibling cluster */
				
				/* perform an adjustment (3-children sibling cluster has two representations) */
				// if (sibling->right->color == BLACK) {
        if (get_color(rbt, get_right(rbt, sibling_ptr)) == BLACK) {
					// sibling->left->color = BLACK;
          set_color(rbt, get_left(rbt, sibling_ptr), BLACK);
          //rb_table_update(rbt, sibling->left);
					// sibling->color = RED;
          set_color(rbt, sibling_ptr, RED);
          //rb_table_update(rbt, sibling);
					rotate_right(rbt, sibling_ptr);
					// sibling = current->parent->right;
          sibling_ptr = get_right(rbt, get_parent(rbt, current_ptr));
				}

				/* transfer by rotation and recoloring */
				// sibling->color = current->parent->color;
        set_color(rbt, sibling_ptr, get_color(rbt, get_parent(rbt, current_ptr)));
        //rb_table_update(rbt, sibling);
				// current->parent->color = BLACK;
        set_color(rbt, get_parent(rbt, current_ptr), BLACK);
        //rb_table_update(rbt, current->parent);
				// sibling->right->color = BLACK;
        set_color(rbt, get_right(rbt, sibling_ptr), BLACK);
        //rb_table_update(rbt, current->right);
				// rotate_left(rbt, current->parent);
        rotate_left(rbt, get_parent(rbt, current_ptr));
				break; /* goto break */
			}
		} else {
			// sibling = current->parent->left;
      sibling_ptr = get_left(rbt, get_parent(rbt, current_ptr));

			// if (sibling->color == RED) {
      if (get_color(rbt, sibling_ptr) == RED) {
				/* perform an adjustment (3-children parent cluster has two representations) */
				// sibling->color = BLACK;
        set_color(rbt, sibling_ptr, BLACK);
        //rb_table_update(rbt, sibling);
				// current->parent->color = RED;
        set_color(rbt, get_parent(rbt, current_ptr), RED);
        //rb_table_update(rbt, current->parent);
				// rotate_right(rbt, current->parent);
        rotate_right(rbt, get_parent(rbt, current_ptr));
				// sibling = current->parent->left;
        sibling_ptr = get_left(rbt, get_parent(rbt, current_ptr));
			}

			/* sibling node must be BLACK now */

			// if (sibling->right->color == BLACK && sibling->left->color == BLACK) {
      if (get_color(rbt, get_right(rbt, sibling_ptr)) == BLACK && get_color(rbt, get_left(rbt, sibling_ptr)) == BLACK) {
				/* 2-children sibling cluster, fuse by recoloring */
				// sibling->color = RED;
        set_color(rbt, sibling_ptr, RED);
        //rb_table_update(rbt, sibling);
				// if (current->parent->color == RED) { /* 3/4-children parent cluster */
        if (get_color(rbt, get_parent(rbt, current_ptr)) == RED) {
					// current->parent->color = BLACK;
          set_color(rbt, get_parent(rbt, current_ptr), BLACK);
          //rb_table_update(rbt, current->parent);
					break; /* goto break */
				} else { /* 2-children parent cluster */
					// current = current->parent; /* goto loop */
          current_ptr = get_parent(rbt, current_ptr);
				}
			} else {
				/* 3/4-children sibling cluster */

				/* perform an adjustment (3-children sibling cluster has two representations) */
				// if (sibling->left->color == BLACK) {
        if (get_color(rbt, get_left(rbt, sibling_ptr)) == BLACK) {
					// sibling->right->color = BLACK;
          set_color(rbt, get_right(rbt, sibling_ptr), BLACK);
          //rb_table_update(rbt, sibling->right);
					// sibling->color = RED;
          set_color(rbt, sibling_ptr, RED);
          //rb_table_update(rbt, sibling);
					rotate_left(rbt, sibling_ptr);
					// sibling = current->parent->left;
          sibling_ptr = get_left(rbt, get_parent(rbt, current_ptr));
				}

				/* transfer by rotation and recoloring */
				// sibling->color = current->parent->color;
        set_color(rbt, sibling_ptr, get_color(rbt, get_parent(rbt, current_ptr)));
        //rb_table_update(rbt, sibling);
				// current->parent->color = BLACK;
        set_color(rbt, get_parent(rbt, current_ptr), BLACK);
        //rb_table_update(rbt, current->parent);
				// sibling->left->color = BLACK;
        set_color(rbt, get_left(rbt, sibling_ptr), BLACK);
        //rb_table_update(rbt, sibling->left);
				// rotate_right(rbt, current->parent);
        rotate_right(rbt, get_parent(rbt, current_ptr));
				break; /* goto break */
			}
		}
	// } while (current != RB_FIRST(rbt));
  } while (current_ptr != RB_FIRST_PTR(rbt));
}

/*
 * check order of tree
 */
int rb_check_order(rbtree *rbt, void *min, void *max)
{
	return check_order(rbt, RB_FIRST_PTR(rbt), min, max);
}

/*
 * check order recursively
 */
int check_order(rbtree *rbt, size_t n_ptr, void *min, void *max)
{
	if (n_ptr == RB_NIL_PTR) // == RB_NIL(rbt))
		return 1;

	#ifdef RB_DUP
	if (rbt->compare(rbt, get_data(rbt, n_ptr), min) < 0 || rbt->compare(rbt, get_data(rbt, n_ptr), max) > 0)
	#else
	if (rbt->compare(rbt, get_data(rbt, n_ptr), min) <= 0 || rbt->compare(rbt, get_data(rbt, n_ptr), max) >= 0)
	#endif
		return 0;

	// return check_order(rbt, n->left, min, n->data) && check_order(rbt, n->right, n->data, max);
	return check_order(rbt, get_left(rbt, n_ptr), min, get_data(rbt, n_ptr))
        && check_order(rbt, get_right(rbt, n_ptr), get_data(rbt, n_ptr), max);
}

/*
 * check black height of tree
 */

int rb_check_black_height(rbtree *rbt)
{
	if (RB_ROOT(rbt)->color == RED || get_color(rbt, RB_FIRST_PTR(rbt)) == RED || RB_NIL(rbt)->color == RED)

		return 0;

	return check_black_height(rbt, RB_FIRST_PTR(rbt));
}

/*
 * check black height recursively
 */
int check_black_height(rbtree *rbt, size_t n_ptr)
{
	int lbh, rbh;

	if (n_ptr == RB_NIL_PTR) // RB_NIL(rbt))
		return 1;

	// if (n->color == RED && (n->left->color == RED || n->right->color == RED || n->parent->color == RED))
  if (get_color(rbt, n_ptr) == RED && (
  get_color(rbt, get_left(rbt, n_ptr)) == RED ||
  get_color(rbt, get_right(rbt, n_ptr)) == RED ||
  get_color(rbt, get_parent(rbt, n_ptr)) == RED))
		return 0;

	if ((lbh = check_black_height(rbt, get_left(rbt, n_ptr))) == 0)
		return 0;

	if ((rbh = check_black_height(rbt, get_right(rbt, n_ptr))) == 0)
		return 0;

	if (lbh != rbh)
		return 0;

	return lbh + (get_color(rbt, n_ptr) == BLACK ? 1 : 0);
}

/*
 * print tree
 */
void rb_print(rbtree *rbt, void (*print_func)(void *))
{
	printf("\n--\n");
	print(rbt, RB_FIRST_PTR(rbt), print_func, 0, "T");
	printf("\ncheck_black_height = %d\n", rb_check_black_height(rbt));
}

/*
 * print node recursively
 */
void print(rbtree *rbt, size_t n_ptr, void (*print_func)(void *), int depth, char *label)
{
	if (n_ptr != RB_NIL_PTR) { // RB_NIL(rbt)) {
		print(rbt, get_right(rbt, n_ptr), print_func, depth + 1, "R");
		printf("%*s", 8 * depth, "");
		if (label)
			printf("%s: ", label);
		print_func(get_data(rbt, n_ptr));
		printf(" (%s)\n", get_color(rbt, n_ptr) == RED ? "r" : "b");
		print(rbt, get_left(rbt, n_ptr), print_func, depth + 1, "L");
	}
}

/*
 * destroy node recursively
 */
void destroy(rbtree *rbt, rbnode *n)
{
/*
	if (n != RB_NIL_PTR(rbt)) {
		destroy(rbt, n->left);
		destroy(rbt, n->right);
		rbt->destroy(n->data);
		free(n);
	}
*/
}
