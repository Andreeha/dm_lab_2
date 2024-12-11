#ifndef TABLE_FILE_H
#define TABLE_FILE_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include <rb.h>

#define INFO_HEADER_LEN 64
#define DATA_OFFSET (1+sizeof(size_t))
#define NCOLS_OFFSET DATA_OFFSET
#define FMLEN_OFFSET (1+NCOLS_OFFSET)
#define COL_TYPES_OFFSET (1 + FMLEN_OFFSET)
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
#define KEY_FIELD 1

// Header view
// [0]                                  [char] = sizeof(size_t) on the moment of table creation
// [1]                                  [size_t] next_empty_space
// [1+DATA_OFFSET]                      [char] number of columns (MAX_NUMBER DEPENDS ON PLATFORM size_t)
// [2+DATA_OFFSET]                      [char] max length of the field names
// ...                                  <- column data types 2*char wide each
// [NAMES_OFFSET]                       1st column name (name_len + 1) symbols wide
// [INFO_HEADER_LEN+ncols*(name_len+1)] <- end of table data
// 

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
          }\
          (da)->items[(da)->count++] = item;\
        }

typedef struct {
  char version;
  size_t init;
  size_t header_offset;
  size_t offset;
  size_t append_offset;
  size_t ncols;
  size_t name_len;
  size_t* col_types;
  size_t* col_offsets;
  size_t* key_col_relpos;
  size_t* rb_heads;
  size_t entry_size;
  size_t entry_metadata_size;
  size_t entry_raw_size;
  char** col_names;
  struct darray stage;
  FILE* file;
} TABLE_STATE;

typedef struct {
  size_t type;
  char* data;
} STAGE_EVENT;

#define RB_DATA_SIZE (3 * sizeof(size_t))

#define TABLE_TYPE_INT 0
#define TABLE_TYPE_FLOAT 1
#define TABLE_TYPE_VARCHAR 2
#define TABLE_TYPE_DATETIME 3
#define DATETIME_SIZE 8 // Y0xfff M0xff D0xff h0xff m0xff s0xff ms0xfff
#define VARCHAR_MAX_LEN (0xffffff - 1)

#define SE_CREATE 1
#define SE_EDIT 2

void header_write(size_t ncols, size_t name_len, size_t* col_types, 
const char** col_names, FILE* file);
void header_read(FILE* file, TABLE_STATE* table_state);

void tset(size_t offset, TABLE_STATE* table_state);
void tappend(void* entry, TABLE_STATE* table_state);
void tedit(void* data, TABLE_STATE* table_state);

size_t entry_offset(size_t index, TABLE_STATE* table_state);
void create_entry(TABLE_STATE* table_state, size_t nargs, ...);
void edit_entry(size_t entry_index, size_t column_number, void* new_val, TABLE_STATE* table_state);

void commit_changes(TABLE_STATE* table_state);

char* get_by_tindex(size_t index, TABLE_STATE* table_state);

size_t create_table(size_t ncols, size_t name_len, size_t* col_types, const char** col_names, const char* file_name);
size_t open_table(const char* file_name, TABLE_STATE* table_state);
size_t close_table(TABLE_STATE *table_state);
size_t delete_table(const char* file_name);
size_t erase_table(const char* file_name);
size_t save_table(TABLE_STATE* table_state);
void display_entry(unsigned char* entry, size_t size);

void rb_table_update(rbtree* rbt, rbnode* node);
void rb_from_raw_table(rbtree* tree, TABLE_STATE* table_state);

unsigned char table_version(FILE* file);
size_t next_empty_read(FILE* file);
size_t next_empty_update(TABLE_STATE* ts);
unsigned char read_ncols(FILE* file);
unsigned char read_name_len(FILE* file);
void read_col_types(TABLE_STATE* table_state);
char* read_field_name(size_t col, TABLE_STATE* ts);
size_t read_rb_head(size_t col, TABLE_STATE* ts);

#endif // TABLE_FILE_H

#ifdef TABLE_FILE_H_IMPLEMENTATION

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
  size_t nkey_fields = 0;
  for (size_t i = 0; i < ncols; i++) {
    info_header[2*i+2+data_offset] = (col_types[i]) & 0xff;
    info_header[2*i+3+data_offset] = (col_types[i] >> 8) & 0xff;
    nkey_fields += IS_KEY(col_types[i]);
  }

  size_t header_size = sizeof(char)*(INFO_HEADER_LEN + ncols*(name_len+1) + nkey_fields * sizeof(size_t));
  char* result = (char*)malloc(header_size);
  
  memcpy(result, info_header, NAMES_OFFSET);
  
  for (size_t i = 0; i < ncols; i++) {
    strcpy(&(result[NAMES_OFFSET + i * (name_len + 1)]), col_names[i]);
  }

  size_t zero = 0;
  for (size_t t = 0; t < nkey_fields; t++) {
    memcpy(&result[INFO_HEADER_LEN + ncols*(name_len+1) + t * sizeof(size_t)], &zero, sizeof(size_t));
  }
  
  rewind(file);
  fwrite(result, sizeof(char), header_size/sizeof(char), file);
  free(result);
  free(info_header);
}

void tset(size_t offset, TABLE_STATE* table_state) {
  table_state->offset = offset;
}

size_t entry_offset(size_t index, TABLE_STATE* table_state) {
  return table_state->header_offset + index * (table_state->entry_size + table_state->entry_metadata_size);
}

void tappend(void* entry, TABLE_STATE* table_state) { // TODO: add check if there is a free place in table
  fseek(table_state->file, table_state->append_offset, SEEK_SET);
  size_t r = fwrite(entry, table_state->entry_raw_size, 1, table_state->file);
  table_state->append_offset = ftell(table_state->file);
}

void tedit(void* data, TABLE_STATE* table_state) {
  size_t index = *((size_t*)data);
  size_t col = ((size_t*)data)[1];
  void* val = ((size_t**)data)[2];
  fseek(table_state->file, entry_offset(index, table_state), SEEK_SET);
  fseek(table_state->file, table_state->col_offsets[col], SEEK_CUR);
  ftell(table_state->file);
  fwrite(val, sizeof(char), TYPE_SIZE(table_state->col_types[col]), table_state->file);
}

unsigned char table_version(FILE* file) {
  fseek(file, 0L, SEEK_SET);
  char version;
  fread(&version, sizeof(char), 1, file);
  return version;
}

size_t next_empty_read(FILE* file) {
  fseek(file, 1, SEEK_SET);
  size_t pos;
  fread(&pos, sizeof(size_t), 1, file);
  return pos;
}

size_t next_empty_update(TABLE_STATE* ts) {
  size_t curr_empty = next_empty_read(ts->file);
  assert(curr_empty != 0);
  // TODO: 
}

unsigned char read_ncols(FILE* file) {
  fseek(file, NCOLS_OFFSET, SEEK_SET);
  char ncols;
  fread(&ncols, sizeof(char), 1, file);
  return ncols;
}

unsigned char read_name_len(FILE* file) {
  fseek(file, FMLEN_OFFSET, SEEK_SET);
  char mlen;
  fread(&mlen, sizeof(char), 1, file);
  return mlen;
}

// Reads col types and sets 
// col_offsets
// key_col_relpos
// entry_metadata_size
void read_col_types(TABLE_STATE* table_state) {
  table_state->col_types = (size_t*)malloc(sizeof(size_t) * table_state->ncols);
  table_state->col_offsets = malloc(sizeof(size_t) * table_state->ncols);
  table_state->key_col_relpos = malloc(sizeof(size_t) * table_state->ncols);
  size_t nkey_cols = 0;
  size_t col_offset = 0;
  size_t key_col_relpos = 0;
  fseek(table_state->file, COL_TYPES_OFFSET, SEEK_SET);
  for (size_t i = 0; i < table_state->ncols; i++) {
    char ttype_[2];
    fread(ttype_  , sizeof(char), 2, table_state->file);
    size_t type_ = table_state->col_types[i] = (0xff&ttype_[0]) | (0xff00&(ttype_[1]<<8));
    table_state->entry_size += TYPE_SIZE(type_);
    table_state->col_offsets[i] = col_offset;
    table_state->key_col_relpos[i] = (IS_KEY(type_) ? key_col_relpos : 0);
    col_offset += TYPE_SIZE(type_);
    key_col_relpos += IS_KEY(type_);
    nkey_cols += IS_KEY(type_);
    printf("%ld\n", TYPE_SIZE(type_));
  }

  table_state->entry_metadata_size = nkey_cols * RB_DATA_SIZE;
  for (size_t i = 0; i < table_state->ncols; i++) {
    table_state->col_offsets[i] += table_state->entry_metadata_size;
  }
}

char* read_field_name(size_t col, TABLE_STATE* ts) {
  fseek(ts->file, NAMES_OFFSET + col * (ts->name_len + 1), SEEK_SET);
  char* name = malloc(sizeof(char) * (ts->name_len+1));
  fread(name, 1, ts->name_len + 1, ts->file);
  return name;
}

size_t read_rb_head(size_t col, TABLE_STATE* ts) {
  if (!IS_KEY(ts->col_types[col])) {
    return 0;
  }
  size_t offset = ts->key_col_relpos[col] * sizeof(size_t) + INFO_HEADER_LEN + ts->ncols*(ts->name_len+1);
  fseek(ts->file, offset, SEEK_SET);
  size_t rb_head;
  fread(&rb_head, sizeof(size_t), 1, ts->file);
  return rb_head;
}

void header_read(FILE* file, TABLE_STATE* table_state) {
  rewind(file);

  table_state->file      = file;
  table_state->version   = table_version(file);
  assert(table_state->version == sizeof(size_t));
  table_state->ncols     = read_ncols(file);
  table_state->name_len  = read_name_len(file);

  read_col_types(table_state);

  table_state->col_names = (char**)malloc(sizeof(char*) * table_state->ncols);

  for (size_t i = 0; i < table_state->ncols; i++) {
    table_state->col_names[i] = read_field_name(i, table_state);
  }

  table_state->rb_heads = malloc(sizeof(size_t) * table_state->name_len)  ;
  for (size_t i = 0; i < table_state->ncols; i++) {
    table_state->rb_heads = read_rb_head(i, table_state);
  }

  table_state->header_offset = INFO_HEADER_LEN + table_state->ncols * ((table_state->name_len + 1) + sizeof(size_t));
  fseek(file, 0L, SEEK_END);
  table_state->append_offset = ftell(file);
  rewind(file);
  table_state->file = file;
  table_state->init = 1;
  table_state->stage = (struct darray){ 0 };
  table_state->entry_raw_size = table_state->entry_metadata_size + table_state->entry_size;
}

void create_entry(TABLE_STATE* table_state, size_t nargs, ...) {
  assert(table_state->init);
  assert(nargs == table_state->ncols);

  va_list args;
  va_start(args, nargs);
  char* data = malloc(table_state->entry_raw_size);
  char* it = data;

  // TODO: maybe refactor this to tree handling
  // Allocation of parent, left and right leafs
  size_t treeval = 0;
  char zero = 0;
  for (size_t i = 0; i < table_state->entry_metadata_size; i++) {
    memcpy(it, &zero, 1);
    it++;
  }

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
      strcpy(it, val);
      it += TYPE_SIZE(table_state->col_types[i]);
    }
    if (TYPE_NUMBER(table_state->col_types[i]) == TABLE_TYPE_DATETIME) {
      const char* val = va_arg(args, const char*);
      memcpy(it, val, DATETIME_SIZE);
      it += TYPE_SIZE(table_state->col_types[i]);
    }
  }
  STAGE_EVENT *se = malloc(sizeof(STAGE_EVENT));
  se->type = SE_CREATE;
  se->data = data;
  da_append(&table_state->stage, se);
  va_end(args);
}

void edit_entry(size_t entry_index, size_t column_number, void* new_val, TABLE_STATE* table_state) {
  STAGE_EVENT *se = malloc(sizeof(STAGE_EVENT));
  se->type = SE_EDIT;
  se->data = malloc(sizeof(size_t) * 2 + sizeof(void*));
  memcpy(se->data, &entry_index, sizeof(size_t));
  memcpy(se->data + sizeof(size_t) / sizeof(char), &column_number, sizeof(size_t));
  memcpy(se->data + 2 * sizeof(size_t) / sizeof(char), &new_val, sizeof(void*));
  da_append(&table_state->stage, se);
}

void commit_changes(TABLE_STATE* table_state) {
  for (size_t i = 0; i < table_state->stage.count; i++) {
    STAGE_EVENT* se = table_state->stage.items[i];
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

char* get_by_tindex(size_t index, TABLE_STATE* table_state) {
  fseek(table_state->file, entry_offset(index, table_state), SEEK_SET);
  char* buf = malloc(table_state->entry_raw_size);
  fread(buf, 1, table_state->entry_raw_size, table_state->file);
  return buf;
}

size_t create_table(size_t ncols, size_t name_len, size_t* col_types, const char** col_names, const char* file_name) {
  assert(access(file_name, F_OK) != 0); // Check if the file doesn't exist
  FILE* file = fopen(file_name, "wb");
  header_write(ncols, name_len, col_types, col_names, file);
  fclose(file);
  return 0;
}

size_t open_table(const char* file_name, TABLE_STATE* table_state) {
  assert(access(file_name, F_OK) == 0); // Check if the file does exist
  FILE* file = fopen(file_name, "rb+");
  header_read(file, table_state);
  return 0;
}

size_t close_table(TABLE_STATE *table_state) {
  fclose(table_state->file);
  free(table_state->col_offsets);
  free(table_state->col_types);
  free(table_state->col_names);
}

size_t delete_table(const char* file_name) {
  assert(access(file_name, F_OK) == 0); // Check if the file does exist
  return unlink(file_name); 
}

size_t erase_table(const char* file_name) {
  assert(access(file_name, F_OK) == 0); // Check if the file does exist
  TABLE_STATE table_state = { 0 };
  open_table(file_name, &table_state);
  fclose(table_state.file);
  delete_table(file_name);
  create_table(table_state.ncols, table_state.name_len, table_state.col_types, table_state.col_names, file_name);
}

size_t save_table(TABLE_STATE* table_state) {
  commit_changes(table_state);
}

void display_entry(unsigned char* entry, size_t size) {
  for (size_t i = 0; i < size; i+=2) {
    printf("%02x%02x ", entry[i], entry[i+1]);
    if (i % 16 == 14) printf("\n");
  }
  printf("\n");
}

void rb_table_update(rbtree* rbt, rbnode* node) {
  if (node->data == NULL)
    return;

  TABLE_STATE* table_state = rbt->table_state;
  if (table_state == NULL)
    return;

  size_t metadata[3] = { 0 };

  if (node->parent->data != NULL) {
    metadata[0] = 1 + ((size_t*)node->parent->data)[0];
  }
  if (node->left->data != NULL) {
    metadata[1] = 1 + ((size_t*)node->left->data)[0];
  }
  if (node->right->data != NULL) {
    metadata[2] = 1 + ((size_t*)node->right->data)[0];
  }
  size_t offset = entry_offset(((size_t*)node->data)[0], table_state);
  fseek(table_state->file, offset, SEEK_SET);
  assert(sizeof(metadata) == table_state->entry_metadata_size);
  fwrite(metadata, 1, sizeof(metadata), table_state->file);
}

void rb_from_raw_table(rbtree* rbt, TABLE_STATE* table_state) {
  fseek(table_state->file, 0L, SEEK_END);
  size_t fend = ftell(table_state->file);
  size_t count = (fend - table_state->header_offset) / (table_state->entry_raw_size);
  for (size_t i = 0; i < count; i++) {
    fseek(table_state->file, entry_offset(i, table_state), SEEK_SET);
    char* entry = malloc(table_state->entry_raw_size);
    size_t r = fread(entry, sizeof(char), table_state->entry_raw_size, table_state->file);

    char *val = malloc(sizeof(size_t) + TYPE_SIZE(table_state->col_types[0]));
    memcpy(val, &i, sizeof(size_t));
    memcpy(val + sizeof(size_t) / sizeof(char), &entry[table_state->col_offsets[0]], TYPE_SIZE(table_state->col_types[0]));
    rb_insert(rbt, val);
    free(entry);
  }
}

#endif // TABLE_FILE_H_IMPLEMENTATION
