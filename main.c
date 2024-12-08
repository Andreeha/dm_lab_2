#include "file.h"

int main () {
  FILE* file = fopen("file.bin", "rb+");
  if (!file) {
    return 1;
  }
  size_t col_types[4] = {
    MAKE_TYPE(0, sizeof(int)),
    MAKE_TYPE(1, sizeof(float)),
    MAKE_TYPE(2, sizeof(char) * (1 + 10)),
    MAKE_TYPE(2, sizeof(char) * (1 + 30))
  };
  const char* col_names[] = {"0", "1", "2", "3"};
  int w;
  scanf("%d", &w);
  if (w) {
    header_write(4, MAX_COL_NAME_LEN, col_types, col_names, file);
  }

  size_t ncols, name_len;
  size_t* col_types_;
  char** col_names_;
  header_read(&ncols, &name_len, &col_types_, &col_names_, file);

  printf("ncols: %ld name_len: %ld\n", ncols, name_len);
  for (int i = 0; i < ncols; i++) {
    printf("raw type: %ld col_name: `%s'\n", col_types_[i], col_names_[i]);
  }

  printf("entry size: %ld\n", table_state.entry_size + table_state.entry_metadata_size);
  create_entry(ENTRY_CREATE, 4, 1, 0.5, "0123456789", "012345678901234567890123456789");
  commit_changes();

  fclose(file);

  return 0;
}
