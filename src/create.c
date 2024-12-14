#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"

int main () {
  size_t col_types[4] = {
    MAKE_TYPE(TABLE_TYPE_INT, sizeof(int)) | KEY_FIELD,
    MAKE_TYPE(TABLE_TYPE_FLOAT, sizeof(float)),
    MAKE_TYPE(TABLE_TYPE_DATATIME, DATATIME_SIZE),
    MAKE_TYPE(TABLE_TYPE_VARCHAR, (256 + 1))
  };
  const char* col_names[] = {
    "id",
    "height",
    "birthday",
    "name"
  };

  if (0 != create_table(4, 32, col_types, col_names, "data/table.bin")) {
    fprintf(stderr, "Could not create table already exists\n");
  }

  return 0;
}
