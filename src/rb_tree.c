#include <stdlib.h>
#include <stdio.h>

#include "rb.h"
#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"

int compare_func(const void* rbt, const void* ra, const void* rb) {
  TABLE_STATE* ts = ((rbtree*)rbt)->table_state;
  int a = ((size_t*)ra)[1];
  int b = ((size_t*)rb)[1];
  if (a == b) return 0;
  if (a > b) return 1;
  return -1;
}

void destroy_func(void* a) {
  free(a);
}

void print_char_func(void* a) {
  printf("%d ", *(int*)&((size_t*)a)[4]);
}

int main () {
  rbtree *rbt;
	
	/* create a red-black tree */

  TABLE_STATE table_state = { 0 };

  open_table("data/file.bin", &table_state);

  // rb_from_raw_table(rbt, &table_state);
  rbt = rb_restore_from_table(0, &table_state, compare_func);

//  close_table(&table_state);

/*
	char a[] = {'R', 'E', 'D', 'S', 'O', 'X', 'C', 'U', 'B', 'T'};
	int i;
	char *data;
	for (i = 0; i < sizeof(a) / sizeof(a[0]); i++) {
		if ((data = &a[i]) == NULL || rb_insert(rbt, data) == NULL) {
			fprintf(stderr, "insert %c: out of memory\n", a[i]);
			break;
		}
		printf("insert %c", a[i]);
		rb_print(rbt, print_char_func);
		printf("\n");
	}

	rbnode *node;
	char query = 'O';
	printf("delete %c", query);
	if ((node = rb_find(rbt, &query)) != NULL) {
    printf("DATA: %c\n", *(char*)node->data);
		rb_delete(rbt, node, 0);
  }
*/
	rb_print(rbt, print_char_func);

	rb_destroy(rbt);
  return 0;
}
