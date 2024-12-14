#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"

int main () {
  TABLE_STATE table_state = { 0 };

  open_table("data/table.bin", &table_state);

  int a;
  scanf("%d", &a);
  delete_entry(0, &a, &table_state);
  commit_changes(&table_state);

  close_table(&table_state);
  
  return 0;
}
