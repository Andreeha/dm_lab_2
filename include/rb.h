/*
 * Copyright (c) 2019 xieqing. https://github.com/xieqing
 * May be freely redistributed, but copyright notice must be retained.
 */

#ifndef _RB_HEADER
#define _RB_HEADER

#define RB_DUP 1
#define RB_MIN 1

#define RED 0
#define BLACK 1

enum rbtraversal {
	PREORDER,
	INORDER,
	POSTORDER
};

typedef struct rbnode {
	struct rbnode *left;
	struct rbnode *right;
	struct rbnode *parent;
	char color;
	void *data;
} rbnode;

typedef struct {
	int (*compare)(const void *, const void *);
	void (*print)(void *);
	void (*destroy)(void *);

	rbnode root;
	rbnode nil;

	#ifdef RB_MIN
	size_t min_ptr;
	#endif

  size_t col;
  void* table_state;
} rbtree;

#define RB_ROOT(rbt) (&(rbt)->root)
#define RB_NIL(rbt) (&(rbt)->nil)
#define RB_FIRST(rbt) ((rbt)->root.left)
#define RB_MINIMAL(rbt) ((rbt)->min)

#define RB_ISEMPTY(rbt) ((rbt)->root.left == &(rbt)->nil && (rbt)->root.right == &(rbt)->nil)
#define RB_APPLY(rbt, f, c, o) rbapply_node((rbt), (rbt)->root.left, (f), (c), (o))

rbtree *rb_create(int (*compare_func)(const void *, const void *), void (*destroy_func)(void *));
void rb_destroy(rbtree *rbt);

size_t rb_find(rbtree *rbt, void *data);
size_t rb_successor(rbtree *rbt, size_t node_ptr);

int rb_apply_node(rbtree *rbt, size_t node_ptr, int (*func)(void *, void *), void *cookie, enum rbtraversal order);
void rb_print(rbtree *rbt, void (*print_func)(void *));

size_t rb_insert(rbtree *rbt, void *data);
void rb_delete(rbtree *rbt, size_t node_ptr, int keep);

int rb_check_order(rbtree *rbt, void *min, void *max);
int rb_check_black_height(rbtree *rbt);

#endif /* _RB_HEADER */
