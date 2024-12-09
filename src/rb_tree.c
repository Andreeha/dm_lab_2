#include <stdlib.h>
#include <stdio.h>

#include "rb.h"
#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"

int compare_func(const void* ra, const void* rb) {
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
  printf("%ld", ((size_t*)a)[1]);
}

int main () {
  rbtree *rbt;
	
	/* create a red-black tree */
	if ((rbt = rb_create(compare_func, destroy_func)) == NULL) {
		fprintf(stderr, "create red-black tree failed\n");
		return 1;
	}

  TABLE_STATE table_state = { 0 };

  rbt->table_state = &table_state;

  open_table("data/file.bin", &table_state);

  rb_from_raw_table(rbt, &table_state);

  close_table(&table_state);

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
