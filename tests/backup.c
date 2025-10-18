#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"

int main () {
  create_backup("data/table.bin", "data/table.backup");
  restore_from_backup("data/table.restore", "data/table.backup");
  
  return 0;
}
