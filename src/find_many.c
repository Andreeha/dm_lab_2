#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"

int main () {
  TABLE_STATE table_state = { 0 };

  open_table("data/table.bin", &table_state);

  size_t datatime = encode_datatime(2024, 12, 14, 11, 11, 11, 123);
  void* entries;
  size_t count = beautiful_find_entry(2, &datatime, &table_state, &entries, NULL);

  printf("Found %ld entries\n", count);
  for (size_t i = 0; i < count; i++) {
    display_entry(((void**)entries)[i], table_state.entry_raw_size, &table_state);
    free(((void**)entries)[i]);
  }
  free(entries);
  printf("\n");

  close_table(&table_state);
  
  return 0;
}
