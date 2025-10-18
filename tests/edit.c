#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"

int main () {
  TABLE_STATE table_state = { 0 };

  open_table("data/table.bin", &table_state);

  int old_value = 1, new_value = 2;
  edit_entry(0, &old_value, &new_value, &table_state);
  commit_changes(&table_state);

  close_table(&table_state);
  
  return 0;
}
