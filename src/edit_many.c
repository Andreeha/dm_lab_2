#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"

int main () {
  TABLE_STATE table_state = { 0 };

  open_table("data/table.bin", &table_state);

  const char* old_value = "76543210";
  const char* new_value = "12345678";
  edit_entry(2, old_value, new_value, &table_state);
  commit_changes(&table_state);

  close_table(&table_state);
  
  return 0;
}
