#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>

#define INFO_HEADER_LEN 64
#define DATA_OFFSET (1 + 2 * sizeof(size_t) / sizeof(char))
#define NAMES_OFFSET (INFO_HEADER_LEN + DATA_OFFSET)
#define MAX_COL_NUMBER ((INFO_HEADER_LEN - 2) / 2)  // It takes 2 bytes for ncols and name_len
                                                    // and 2 bytes per column to store
#define MAX_COL_NAME_LEN (0xff - 1)

// TODO: data type
// T = 0x0000
// T & 0x0001 <-> if it's a key field or not
// T & 0x000e <-> data type
// (T & 0xfff0) >> 4 <-> Type size

#define TYPE_NUMBER(t) ((t&0x000e)>>1)
#define IS_KEY(t) (t&0x0001)
#define TYPE_SIZE(t) ((t&0xfff0)>>4)
#define MAKE_TYPE(number, size) (0xffff & (((number<<1)&0xe) + (size << 4)))

// Header view
// [0]                                sizeof(size_t) on table creation
// [1]                                next_empty_space [size_t]
// ...
// [1+sizeof(size_t)/sizeof(char)]    position of the HEAD of RB-tree
// ...
// [1+DATA_OFFSET]                    number of columns (MAX_NUMBER DEPENDS ON PLATFORM size_t)
// [2+DATA_OFFSET]                    max length of the field names
// ...                                <- column data types 2*char wide each
// [NAMES_OFFSET]                     1st column name (name_len + 1) symbols wide

void header_write(size_t ncols, size_t name_len, size_t* col_types, const char** col_names, FILE* file) {
  char* info_header = (char*)malloc(sizeof(char) * NAMES_OFFSET + sizeof(size_t));
  
  info_header[0] = sizeof(size_t);
  
  size_t next_empty_place = 0;
  memcpy(&info_header[1], &next_empty_place, sizeof(size_t)); // TODO:
  
  size_t tree_head = 0;
  memcpy(&info_header[1+sizeof(size_t) / sizeof(char)], &tree_head, sizeof(size_t));
  
  int data_offset = DATA_OFFSET;
  
  info_header[data_offset] = ncols;
  info_header[data_offset+1] = name_len;
  for (size_t i = 0; i < ncols; i++) {
    info_header[2*i+2+data_offset] = (col_types[i]) & 0xff;
    info_header[2*i+3+data_offset] = (col_types[i] >> 8) & 0xff;
  }

  size_t header_size = sizeof(char)*(INFO_HEADER_LEN + ncols*(name_len+1));
  char* result = (char*)malloc(header_size);
  
  memcpy(result, info_header, NAMES_OFFSET);
  
  for (size_t i = 0; i < ncols; i++) {
    strcpy(&(((char*)result)[NAMES_OFFSET + i * (name_len + 1)]), col_names[i]);
  }
  
  rewind(file);
  fwrite(result, sizeof(char), header_size/sizeof(char), file);
  free(result);
  free(info_header);
}

struct darray {
  size_t count, capacity;
  void** items;
};

struct TABLE_STATE {
  size_t init;
  size_t header_offset;
  size_t offset;
  size_t append_offset;
  size_t ncols;
  size_t name_len;
  size_t* col_types;
  size_t* col_offsets;
  size_t entry_size;
  size_t entry_metadata_size;
  char** col_names;
  struct darray stage;
  FILE* file;
};

struct STAGE_EVENT {
  size_t type;
  char* data;
};

#define SE_CREATE 1
#define SE_EDIT 2

void tset(size_t offset, struct TABLE_STATE* table_state) {
  table_state->offset = offset;
}

size_t entry_offset(size_t index, struct TABLE_STATE* table_state) {
  return table_state->header_offset + index * (table_state->entry_size + table_state->entry_metadata_size);
}

void tappend(void* entry, struct TABLE_STATE* table_state) { // TODO: add check if there is a free place in table
  fseek(table_state->file, table_state->append_offset, SEEK_SET);
  size_t r = fwrite(entry, table_state->entry_size + table_state->entry_metadata_size, 1, table_state->file);
  table_state->append_offset = ftell(table_state->file);
}

void tedit(void* data, struct TABLE_STATE* table_state) {
  size_t index = *((size_t*)data);
  size_t col = ((size_t*)data)[1];
  void* val = ((size_t**)data)[2];
  fseek(table_state->file, entry_offset(index, table_state), SEEK_SET);
  fseek(table_state->file, table_state->col_offsets[col], SEEK_CUR);
  ftell(table_state->file);
  fwrite(val, sizeof(char), TYPE_SIZE(table_state->col_types[col]), table_state->file);
}

void header_read(FILE* file, struct TABLE_STATE* table_state) {
  rewind(file);

  char* info_header = (char*)malloc(sizeof(char) * INFO_HEADER_LEN);
  size_t r;
  r = fread(info_header, sizeof(char), INFO_HEADER_LEN, file);
  assert(r == INFO_HEADER_LEN);
  
  assert(info_header[0] == sizeof(size_t)); // check for alignment compatibility

  size_t data_offset = DATA_OFFSET;

  table_state->ncols = info_header[data_offset];
  table_state->name_len = (unsigned char)info_header[data_offset+1];
  table_state->col_types = (size_t*)malloc(sizeof(size_t) * table_state->ncols);

  table_state->entry_metadata_size = 3 * sizeof(size_t); // TODO: maybe refactor this somehow
  
  size_t col_offset = table_state->entry_metadata_size;
  table_state->col_offsets = malloc(sizeof(size_t) * table_state->ncols);
  
  for (size_t i = 0; i < table_state->ncols; i++) {
    table_state->col_types[i] = (0xff&info_header[2*i+2+data_offset]) | (0xff00&(info_header[2*i+3+data_offset]<<8));
    table_state->entry_size += TYPE_SIZE(table_state->col_types[i]);
    table_state->col_offsets[i] = col_offset;
    col_offset += TYPE_SIZE(table_state->col_types[i]);
  }

  table_state->col_names = (char**)malloc(sizeof(char*) * table_state->ncols);

  fseek(file, NAMES_OFFSET, SEEK_SET);
  for (size_t i = 0; i < table_state->ncols; i++) {
    table_state->col_names[i] = malloc(sizeof(char) * (1 + table_state->name_len));
    r = fread(table_state->col_names[i], sizeof(char), 1 + table_state->name_len, file);
  }
  table_state->header_offset = INFO_HEADER_LEN + table_state->ncols * (table_state->name_len + 1);
  fseek(file, 0L, SEEK_END);
  table_state->append_offset = ftell(file);
  rewind(file);
  table_state->file = file;
  table_state->init = 1;
  table_state->stage = (struct darray){ 0 };
  free(info_header);
}

#define da_append(da, item) {\
          if ((da)->count == (da)->capacity) {\
            (da)->capacity = ((da)->capacity + 1) * 2;\
            void** tmp = malloc(sizeof(void*) * (da)->capacity);\
            if ((da)->items) {\
              memcpy(tmp, (da)->items, (da)->count);\
              free((da)->items);\
            }\
            (da)->items = tmp;\
          }\
          (da)->items[(da)->count++] = item;\
        }

#define TABLE_TYPE_INT 0
#define TABLE_TYPE_FLOAT 1
#define TABLE_TYPE_VARCHAR 2
#define VARCHAR_MAX_LEN (0xffffff - 1)

#define ENTRY_CREATE 0

void create_entry(size_t entry_type, struct TABLE_STATE* table_state, size_t nargs, ...) {
  assert(table_state->init);
  assert((entry_type != ENTRY_CREATE) || (nargs == table_state->ncols));

  va_list args;
  va_start(args, nargs);
  char* data = malloc(table_state->entry_size + table_state->entry_metadata_size);
  char* it = data;

  // TODO: maybe refactor this to tree handling
  // Allocation of parent, left and right leafs
  size_t treeval = 0;
  memcpy(it, &treeval, sizeof(size_t));
  it += sizeof(size_t) / sizeof(char);
  memcpy(it, &treeval, sizeof(size_t));
  it += sizeof(size_t) / sizeof(char);
  memcpy(it, &treeval, sizeof(size_t));
  it += sizeof(size_t) / sizeof(char);

  for (size_t i = 0; i < nargs; i++) {
    if (TYPE_NUMBER(table_state->col_types[i]) == TABLE_TYPE_INT) {
      int val = va_arg(args, int);
      memcpy(it, &val, sizeof(int));
      it += sizeof(int) / sizeof(char);
    }
    if (TYPE_NUMBER(table_state->col_types[i]) == TABLE_TYPE_FLOAT) {
      float val = va_arg(args, double);
      memcpy(it, &val, sizeof(float));
      it += sizeof(float) / sizeof(char);
    }
    if (TYPE_NUMBER(table_state->col_types[i]) == TABLE_TYPE_VARCHAR) {
      const char* val = va_arg(args, const char*);
      memcpy(it, val, strlen(val));
      it += TYPE_SIZE(table_state->col_types[i]);
    }
  }
  struct STAGE_EVENT *se = malloc(sizeof(struct STAGE_EVENT));
  se->type = SE_CREATE;
  se->data = data;
  da_append(&table_state->stage, se);
  va_end(args);
}

void edit_entry(size_t entry_index, size_t column_number, void* new_val, struct TABLE_STATE* table_state) {
  struct STAGE_EVENT *se = malloc(sizeof(struct STAGE_EVENT));
  se->type = SE_EDIT;
  se->data = malloc(sizeof(size_t) * 2 + sizeof(void*));
  memcpy(se->data, &entry_index, sizeof(size_t));
  memcpy(se->data + sizeof(size_t) / sizeof(char), &column_number, sizeof(size_t));
  memcpy(se->data + 2 * sizeof(size_t) / sizeof(char), &new_val, sizeof(void*));
  da_append(&table_state->stage, se);
}

void commit_changes(struct TABLE_STATE* table_state) {
  for (size_t i = 0; i < table_state->stage.count; i++) {
    struct STAGE_EVENT* se = table_state->stage.items[i];
    if (se->type == SE_CREATE) {
      tappend(se->data, table_state);
      free(se->data);
    }
    if (se->type == SE_EDIT) {
      tedit(se->data, table_state);
    }
    free(se);
  }
  table_state->stage.count = 0;
  fflush(table_state->file);
}

char* get_by_tindex(size_t index, struct TABLE_STATE* table_state) {
  fseek(table_state->file, entry_offset(index, table_state), SEEK_SET);
  char* buf = malloc(table_state->entry_metadata_size + table_state->entry_size);
  fread(buf, 1, table_state->entry_metadata_size + table_state->entry_size, table_state->file);
  return buf;
}

size_t create_table(size_t ncols, size_t name_len, size_t* col_types, const char** col_names, const char* file_name) {
  assert(access(file_name, F_OK) != 0); // Check if the file doesn't exist
  FILE* file = fopen(file_name, "wb");
  header_write(ncols, name_len, col_types, col_names, file);
  fclose(file);
  return 0;
}

size_t open_table(const char* file_name, struct TABLE_STATE* table_state) {
  assert(access(file_name, F_OK) == 0); // Check if the file does exist
  FILE* file = fopen(file_name, "rb+");
  header_read(file, table_state);
  return 0;
}

size_t delete_table(const char* file_name) {
  assert(access(file_name, F_OK) == 0); // Check if the file does exist
  return unlink(file_name); 
}

size_t erase_table(const char* file_name) {
  assert(access(file_name, F_OK) == 0); // Check if the file does exist
  struct TABLE_STATE table_state = { 0 };
  open_table(file_name, &table_state);
  fclose(table_state.file);
  delete_table(file_name);
  create_table(table_state.ncols, table_state.name_len, table_state.col_types, table_state.col_names, file_name);
}

size_t save_table(struct TABLE_STATE* table_state) {
  commit_changes(table_state);
}

void display_entry(unsigned char* entry, size_t size) {
  for (size_t i = 0; i < size; i+=2) {
    printf("%02x%02x ", entry[i], entry[i+1]);
    if (i % 16 == 14) printf("\n");
  }
  printf("\n");
}
