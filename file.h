#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>

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

#define TYPE_NUMBER(t) (t&0x000e)
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

struct TABLE_STATE {
  size_t init;
  size_t header_offset;
  size_t offset;
  size_t append_offset;
  size_t ncols;
  size_t name_len;
  size_t* col_types;
  size_t entry_size;
  size_t entry_metadata_size;
  char** col_names;
  FILE* file;
};

struct TABLE_STATE table_state = { 0 };

void tseek(int offset) {
  table_state.offset = offset;
}

void tappend(void* entry) { // TODO: check if there is a free place in table
  fseek(table_state.file, table_state.append_offset, SEEK_SET);
  size_t r = fwrite(entry, table_state.entry_size, 1, table_state.file);
  table_state.append_offset += r;
}

void header_read(size_t *ncols, size_t *name_len, size_t **col_types, char*** col_names, FILE* file) {
  rewind(file);

  char* info_header = (char*)malloc(sizeof(char) * INFO_HEADER_LEN);
  size_t r;
  r = fread(info_header, sizeof(char), INFO_HEADER_LEN, file);
  tseek(table_state.offset + r);
  assert(info_header[0] == sizeof(size_t)); // check for alignment compatibility

  size_t data_offset = DATA_OFFSET;

  *ncols = info_header[data_offset];
  *name_len = (unsigned char)info_header[data_offset+1];
  *col_types = (size_t*)malloc(sizeof(size_t) * (*ncols));
  for (size_t i = 0; i < *ncols; i++) {
    (*col_types)[i] = (0xff&info_header[2*i+2+data_offset]) | (0xff00&(info_header[2*i+3+data_offset]<<8));
    table_state.entry_size += TYPE_SIZE((*col_types)[i]);
  }

  *col_names = (char**)malloc(sizeof(char*) * (*ncols));

  fseek(file, NAMES_OFFSET, SEEK_SET);
  for (size_t i = 0; i < *ncols; i++) {
    (*col_names)[i] = malloc(sizeof(char) * (1 + *name_len));
    r = fread((*col_names)[i], sizeof(char), 1 + *name_len, file);
    tseek(table_state.offset + r);
  }
  table_state.header_offset = table_state.offset;
  fseek(file, 0L, SEEK_END);
  table_state.append_offset = ftell(file);
  fseek(file, 0L, SEEK_SET);
  table_state.ncols = *ncols;
  table_state.name_len = *name_len;
  table_state.col_types = *col_types;
  table_state.col_names = *col_names;
  table_state.file = file;
  table_state.init = 1;
  table_state.entry_metadata_size = 3 * sizeof(size_t); // TODO: maybe refactor this somehow
  free(info_header);
}

struct darray {
  size_t count, capacity;
  void** items;
};

#define da_append(da, item) {\
          if ((da)->count == (da)->capacity) {\
            (da)->capacity = ((da)->capacity + 1) * 2;\
            void** tmp = malloc(sizeof(void*) * (da)->capacity);\
            if ((da)->items) {\
              memcpy(tmp, (da)->items, (da)->count);\
              free((da)->items);\
            }\
            (da)->items = tmp;\
            (da)->items[(da)->count++] = item;\
          }\
        }

struct darray stage = {0};

#define TABLE_TYPE_INT 0
#define TABLE_TYPE_FLOAT 1
#define TABLE_TYPE_VARCHAR 2
#define VARCHAR_MAX_LEN 0b11110 // 30

#define ENTRY_CREATE 0

void create_entry(size_t entry_type, size_t nargs, ...) {
  assert(table_state.init);
  assert((entry_type != ENTRY_CREATE) || (nargs == table_state.ncols));

  va_list args;
  va_start(args, nargs);
  char* data = malloc(table_state.entry_size + table_state.entry_metadata_size);
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
    if (TYPE_NUMBER(table_state.col_types[i]) == TABLE_TYPE_INT) {
      int val = va_arg(args, int);
      memcpy(it, &val, sizeof(int));
      it += sizeof(int) / sizeof(char);
    }
    if (TYPE_NUMBER(table_state.col_types[i]) == TABLE_TYPE_FLOAT) {
      float val = va_arg(args, double);
      memcpy(it, &val, sizeof(float));
      it += sizeof(float) / sizeof(char);
    }
    if (TYPE_NUMBER(table_state.col_types[i]) == TABLE_TYPE_VARCHAR) {
      const char* val = va_arg(args, const char*);
      memcpy(it, val, strlen(val));
      it += TYPE_SIZE(table_state.col_types[i]);
    }
  }
  da_append(&stage, data);
  va_end(args);
}

void delete_entry() {
  
}

void commit_changes() {
  for (size_t i = 0; i < stage.count; i++) {
    tappend(stage.items[i]);
    free(stage.items[i]);
  }
}
