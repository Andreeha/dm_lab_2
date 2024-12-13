#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"

int main () {
  TABLE_STATE table_state = { 0 };
  size_t col_types[4] = {
    MAKE_TYPE(0, sizeof(int)) | KEY_FIELD,
    MAKE_TYPE(1, sizeof(float)),
    MAKE_TYPE(3, sizeof(char) * DATETIME_SIZE),
    MAKE_TYPE(2, sizeof(char) * (1 + 30))
  };
  const char* col_names[] = {"First", "Second", "Third", "Foutrth"};
  #ifdef CREATE_TABLE
    create_table(4, MAX_COL_NAME_LEN, col_types, col_names, "data/file.bin");
  #endif

  FILE* file = fopen("data/file.bin", "rb+");
  if (!file) {
    return 1;
  }

  header_read(file, &table_state);
  int a;
  scanf("%d", &a);
  create_entry(&table_state, 4, a, 0.123, "76543210", "012345678901234567890123456789");
  commit_changes(&table_state);

  char* r0 = get_by_tindex(0, &table_state);
  display_entry(r0, table_state.entry_raw_size);
  return 0;

  printf("ncols: %ld name_len: %ld\n", table_state.ncols, table_state.name_len);
  for (int i = 0; i < table_state.ncols; i++) {
    printf("raw type: %ld col_name: `%s'\n", table_state.col_types[i], table_state.col_names[i]);
  }
  char buff = 0xFF; 
  fseek(table_state.file, 1085, SEEK_SET);
  fwrite(&buff, sizeof(char), 1, table_state.file);

  printf("entry size: %ld\n", table_state.entry_size + table_state.entry_metadata_size);
  create_entry(&table_state, 4, 1, 0.5, "01234567", "012345678901234567890123456789");
  commit_changes(&table_state);
  int v = 10;
  char* r1 = get_by_tindex(0, &table_state);
  display_entry(r1, table_state.entry_metadata_size + table_state.entry_size);
  printf("\nr1: %d\n", *(int*)&r1[table_state.col_offsets[0]]);
  edit_entry(0, 0, &v, &table_state);
  commit_changes(&table_state);
//  char* r2 = get_by_tindex(0, &table_state);
//  printf("r1: %d\n", *(int*)&r2[table_state.col_offsets[0]]);
//  commit_changes(&table_state);
  free(r1);
//  free(r2);

  fclose(file);

  return 0;
}
