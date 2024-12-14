#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"

int main () {
  TABLE_STATE table_state = { 0 };

  open_table("data/table.bin", &table_state);

  int a;
  scanf("%d", &a);
  void *entries;
  size_t count = beautiful_find_entry(0, &a, &table_state, &entries, NULL);

  printf("Found %ld entries\n", count);
  for (size_t i = 0; i < count; i++) {
    display_entry(((void**)entries)[i], table_state.entry_raw_size, &table_state);
    free(((void**)entries)[i]);
  }
  printf("\n");

  close_table(&table_state);
  
  return 0;
}
