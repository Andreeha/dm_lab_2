#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"

int main () {
  TABLE_STATE table_state = { 0 };

  open_table("data/table.bin", &table_state);

  int a;
  scanf("%d", &a);
  create_entry(&table_state, 4, a, 0.123, "76543210", "012345678901234567890123456789");
  commit_changes(&table_state);

  close_table(&table_state);
  
  return 0;
}
