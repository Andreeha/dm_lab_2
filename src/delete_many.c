#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"

int main () {
  TABLE_STATE table_state = { 0 };

  open_table("data/table.bin", &table_state);

  size_t datatime = encode_datatime(2024, 12, 14, 11, 11, 11, 123);
  delete_entry(2, &datatime, &table_state);
  commit_changes(&table_state);

  close_table(&table_state);
  
  return 0;
}
