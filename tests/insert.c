#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"

int main () {
  TABLE_STATE table_state = { 0 };

  open_table("data/table.bin", &table_state);

  int a;
  scanf("%d", &a);
  size_t b = encode_datatime(2024, 12, 11, 11, 11, 11, 123);
  char* datatime = &b;
  create_entry(&table_state, 4, a, 0.123, datatime, "First name Second name Surname");
  commit_changes(&table_state);

  close_table(&table_state);
  
  return 0;
}
