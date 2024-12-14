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
// [1+DATA_OFFSET]                      [char] number of columns
// [2+DATA_OFFSET]                      [char] max length of the field names
// ...                                  <- column data types 2*char wide each
// [NAMES_OFFSET]                       1st column name (name_len + 1) symbols wide
// [INFO_HEADER_LEN+ncols*(name_len+1)] <- end of table data

struct darray {
  size_t count, capacity;
  void** items;
};

#define da_append(xs, x)                                                             \
    do {                                                                             \
        if ((xs)->count >= (xs)->capacity) {                                         \
            if ((xs)->capacity == 0) (xs)->capacity = 256;                           \
            else (xs)->capacity *= 2;                                                \
            (xs)->items = realloc((xs)->items, (xs)->capacity*sizeof(*(xs)->items)); \
        }                                                                            \
                                                                                     \
        (xs)->items[(xs)->count++] = (x);                                            \
    } while (0)

typedef struct {
  size_t value;
  void* next;
} stack;

typedef struct {
  char version;
  size_t init;
  size_t header_offset;
  size_t offset;
  size_t append_offset;
  size_t ncols;
  size_t nkey_cols;
  size_t name_len;
  size_t* col_types;
  size_t* col_offsets;
  size_t* key_col_relpos;
  rbtree** rb_trees;
  size_t entry_size;
  size_t entry_metadata_size;
  size_t entry_raw_size;
  char** col_names;
  struct darray stage;
  // stack* stage_deleted;
  // size_t stage_next_free;
  size_t stage_append_offset;
  FILE* file;
  size_t last_inserted;
} TABLE_STATE;

typedef struct {
  size_t type;
  char* data;
} STAGE_EVENT;

#define RB_DATA_LEN 4
#define RB_DATA_SIZE (RB_DATA_LEN * sizeof(size_t))
#define RB_INDEX_PARENT 0
#define RB_INDEX_LEFT   1
#define RB_INDEX_RIGHT  2
#define RB_INDEX_COLOR  3

#define TABLE_TYPE_INT 0
#define TABLE_TYPE_FLOAT 1
#define TABLE_TYPE_VARCHAR 2
#define TABLE_TYPE_DATATIME 3
#define DATATIME_SIZE 8 // Y0xfff M0xff D0xff h0xff m0xff s0xff ms0xfff
#define VARCHAR_MAX_LEN (0xffffff - 1)

#define SE_CREATE        0
#define SE_DELETE        1
#define SE_EDIT          2
#define SE_EDIT_SELECTED 3

void header_write(size_t ncols, size_t name_len, size_t* col_types, const char** col_names, FILE* file);
void header_read(FILE* file, TABLE_STATE* table_state);

void tset(size_t offset, TABLE_STATE* table_state);
size_t tappend(void* entry, TABLE_STATE* table_state);
void tedit(void* data, TABLE_STATE* table_state);

size_t entry_offset(size_t index, TABLE_STATE* table_state);
void create_entry(TABLE_STATE* table_state, size_t nargs, ...);
size_t edit_entry(size_t col, void* old_val, void* new_val, TABLE_STATE* table_state);

void commit_changes(TABLE_STATE* table_state);

char* get_by_tindex(size_t index, TABLE_STATE* table_state);

size_t create_table(size_t ncols, size_t name_len, size_t* col_types, const char** col_names, const char* file_name);
size_t open_table(const char* file_name, TABLE_STATE* table_state);
size_t close_table(TABLE_STATE *table_state);
size_t delete_table(const char* file_name);
size_t erase_table(const char* file_name);
size_t save_table(TABLE_STATE* table_state);
void display_entry(unsigned char* entry, size_t size, TABLE_STATE* table_state);

void rb_table_update(rbtree* rbt, rbnode* node);
void rb_from_raw_table(rbtree* tree, TABLE_STATE* table_state);

unsigned char table_version(FILE* file);
size_t next_empty_read(FILE* file);
size_t next_empty_write(FILE* file, size_t node_ptr);
size_t next_empty_withdraw(TABLE_STATE* ts);
unsigned char read_ncols(FILE* file);
unsigned char read_name_len(FILE* file);
void read_col_types(TABLE_STATE* table_state);
char* read_field_name(size_t col, TABLE_STATE* ts);
size_t* read_rb_head(size_t col, TABLE_STATE* ts);
size_t write_rb_head(size_t col, size_t rb_head, TABLE_STATE* ts);
rbtree* rb_restore_from_table(size_t col, TABLE_STATE* table_state, int (*cmp)(const void*, const void*, const void*));

size_t get_stage_next_free(TABLE_STATE* ts);
void add_stage_deleted(TABLE_STATE* ts, size_t val);

int cmp_int     (const void* rb, const void* a, const void* b);
int cmp_float   (const void* rb, const void* a, const void* b);
int cmp_str     (const void* rb, const void* a, const void* b);
int cmp_datatime(const void* rb, const void* a, const void* b);
int (*pick_cmp(size_t type))(const void*, const void*, const void*);
void _destroy(void* a);

size_t find_entry(size_t col, void* value, TABLE_STATE* table_state, void** result, size_t** indices);
size_t beautiful_find_entry(size_t col, void* value, TABLE_STATE* table_state, void** result, size_t** indices);
size_t delete_entry(size_t col, void* value, TABLE_STATE* table_state);

int archive(int type, const char* table_name, const char* backup_name);
size_t restore_from_backup(const char* file_name, const char* backup_name);
size_t create_backup(const char* file_name, const char* backup_name);

size_t encode_datatime(size_t Y, size_t M, size_t D, size_t h, size_t m, size_t s, size_t ms);

#endif // TABLE_FILE_H

#ifdef TABLE_FILE_H_IMPLEMENTATION

size_t encode_datatime(size_t Y, size_t M, size_t D, size_t h, size_t m, size_t s, size_t ms) {
  // [ms][ms]s[ms]mshmhDMDYMYY
  size_t res = ms&0xff;
  res <<= 8;
  res |= ((s&0x0f)<<4)+((ms&0xf00)>>8);
  res <<= 8;
  res |= ((s&0xf0)>>4)+((m&0x0f)<<4);
  res <<= 8;
  res |= ((m&0xf0)>>4)+((h&0x0f)<<4);
  res <<= 8;
  res |= ((h&0xf0)>>4)+((D&0x0f)<<4);
  res <<= 8;
  res |= ((D&0xf0)>>4)+((M&0x0f)<<4);
  res <<= 8;
  res |= ((M&0xf0)>>4)+((Y&0xf00)>>4);
  res <<= 8;
  res |= Y&0xff;
  return res;
}

int (*pick_cmp(size_t type_))(const void*, const void*, const void*) {
  if (TYPE_NUMBER(type_) == TABLE_TYPE_INT)
    return cmp_int;
  if (TYPE_NUMBER(type_) == TABLE_TYPE_FLOAT)
    return cmp_float;
  if (TYPE_NUMBER(type_) == TABLE_TYPE_VARCHAR)
    return cmp_str;
  if (TYPE_NUMBER(type_) == TABLE_TYPE_DATATIME)
    return cmp_datatime;
  assert(0 && "should never happen");
  return NULL;
}

void _destroy(void* a) {
  free(a);
}

int cmp_int     (const void* rb, const void* a, const void* b) {
  const rbtree* rbt = rb;
  TABLE_STATE* ts = rbt->table_state;
  size_t offset = ts->col_offsets[rbt->col];
  int l = *(int*)&((char*)a)[offset];
  int r = *(int*)&((char*)b)[offset];
  if (l == r) return 0;
  if (l < r) return -1;
  return 1;
}
int cmp_float   (const void* rb, const void* a, const void* b) {
  const rbtree* rbt = rb;
  TABLE_STATE* ts = rbt->table_state;
  size_t offset = ts->col_offsets[rbt->col];
  float l = *(float*)&((char*)a)[offset];
  float r = *(float*)&((char*)b)[offset];
  if (l == r) return 0;
  if (l < r) return -1;
  return 1;
}
int cmp_str     (const void* rb, const void* a, const void* b) {
  const rbtree* rbt = rb;
  TABLE_STATE* ts = rbt->table_state;
  size_t offset = ts->col_offsets[rbt->col];
  char* l = &((char*)a)[offset];
  char* r = &((char*)b)[offset];
  return strcmp(l, r);
}
int cmp_datatime(const void* rb, const void* a, const void* b) {
  const rbtree* rbt = rb;
  TABLE_STATE* ts = rbt->table_state;
  size_t offset = ts->col_offsets[rbt->col];
  char* l = &((char*)a)[offset];
  char* r = &((char*)b)[offset];
  for (size_t i = 0; i < DATATIME_SIZE; i++) {
    if (l[i] < r[i]) return -1;
    if (r[i] > l[i]) return 1;
  }
  return 0;
}

/*
void add_stage_deleted(TABLE_STATE* ts, size_t val) {
  //stack* q = malloc(sizeof(stack));
  //q->value = val;
  //q->next = ts->stage_deleted;
  //ts->stage_deleted = q;
}
*/

/*
size_t get_stage_next_free(TABLE_STATE* ts) {
  if (ts->stage_next_free) {
    size_t offset = entry_offset(ts->stage_next_free, ts);
    size_t curr = ts->stage_next_free;
    size_t next_free;
    if (ts->stage_deleted == NULL) {
      fseek(ts->file, offset, SEEK_SET);
      fread(&next_free, sizeof(size_t), 1, ts->file);
      ts->stage_next_free = next_free;
    } else {
      stack* q = ts->stage_deleted;
      next_free = q->value;
      ts->stage_deleted = q->next;
      free(q);
    }
    return curr;
  } else {
    size_t curr = (ts->stage_append_offset - ts->header_offset) / ts->entry_raw_size;
    ts->stage_append_offset += ts->entry_raw_size;
    return curr;
  }
}
*/

void header_write(size_t ncols, size_t name_len, size_t* col_types, const char** col_names, FILE* file) {
  char* info_header = (char*)malloc(sizeof(char) * NAMES_OFFSET);
  
  info_header[0] = sizeof(size_t);
  
  size_t next_empty_place = 0;
  memcpy(&info_header[1], &next_empty_place, sizeof(size_t)); // TODO:
  
  size_t tree_head = 0;
  memcpy(&info_header[1+sizeof(size_t) / sizeof(char)], &tree_head, sizeof(size_t));
  
  int data_offset = DATA_OFFSET;
  
  info_header[data_offset] = ncols;
  info_header[data_offset+1] = name_len;
  size_t nkey_fields = 0;
  size_t entry_size = 0;
  for (size_t i = 0; i < ncols; i++) {
    info_header[2*i+2+data_offset] = (col_types[i]) & 0xff;
    info_header[2*i+3+data_offset] = (col_types[i] >> 8) & 0xff;
    nkey_fields += IS_KEY(col_types[i]);
    entry_size += TYPE_SIZE(col_types[i]);
  }

  size_t header_size = NAMES_OFFSET + ncols*(name_len+1) + nkey_fields * RB_DATA_SIZE;
  char* result = (char*)malloc(header_size);
  
  memcpy(result, info_header, NAMES_OFFSET);
  
  for (size_t i = 0; i < ncols; i++) {
    strcpy(&(result[NAMES_OFFSET + i * (name_len + 1)]), col_names[i]);
  }

  size_t rb_nil[4] = {-1,-1,-1,-1};

  size_t rb_offset = NAMES_OFFSET + ncols*(name_len+1);
  for (size_t t = 0; t < nkey_fields; t++) {
    memcpy(&result[rb_offset + t * RB_DATA_SIZE], &rb_nil, RB_DATA_SIZE);
    // fprintf(stderr, "%ld: %lx\n", t, *(size_t*)&result[rb_offset + t * sizeof(size_t)]);
  }
  size_t index = 0;
  // fprintf(stderr, "sz: %lx\n", header_size);
  // fprintf(stderr, "%08lx: ", index);
  for (size_t i = 0; i < header_size; i += 2) {
    // fprintf(stderr, "%02x%02x ", (unsigned char)result[i], (unsigned char)result[i+1]);
    if (i % 16 == 14) {
      index += 16;
      // fprintf(stderr, "\n%08lx: ", index);
    }
  }
  // fprintf(stderr, "\n");
  
  rewind(file);
  fwrite(result, header_size, 1, file);
  char zero = 0;
  for (size_t i = 0; i < entry_size; i++) fwrite(&zero, 1, 1, file);
  free(result);
  free(info_header);
}

void tset(size_t offset, TABLE_STATE* table_state) {
  table_state->offset = offset;
}

size_t entry_offset(size_t index, TABLE_STATE* table_state) {
  return table_state->header_offset + index * (table_state->entry_size + table_state->entry_metadata_size);
}

size_t tappend(void* entry, TABLE_STATE* table_state) { // TODO: add check if there is a free place in table
  size_t next_empty = next_empty_read(table_state->file);
  size_t offset = table_state->append_offset;
  size_t was_empty = 0;
  if (next_empty) {
    was_empty = 1;
    offset = entry_offset(next_empty, table_state);
    next_empty_withdraw(table_state);
  }
  table_state->last_inserted = (offset - table_state->header_offset) / table_state->entry_raw_size;
  fseek(table_state->file, offset + table_state->entry_metadata_size, SEEK_SET);
  size_t r = fwrite(&((char*)entry)[table_state->entry_metadata_size], table_state->entry_size, 1, table_state->file);
  table_state->append_offset = ftell(table_state->file);
  size_t ret = 0;
  for (size_t i = 0; i < table_state->nkey_cols; i++) {
    ret = !rb_insert(table_state->rb_trees[i], entry);
    if (ret && !was_empty) {
      table_state->append_offset = offset;
      ftruncate(fileno(table_state->file), offset);
      break;
    }
  }
  return ret;
  // Update next free
}

void tedit(void* data, TABLE_STATE* table_state) {
  size_t col = *((size_t*)data);
  size_t size = TYPE_SIZE(table_state->col_types[col]);
  void* old_value = &((char*)data)[sizeof(size_t)];
  void* new_value = &((char*)data)[sizeof(size_t) + size];
  size_t* indices;
  size_t count = beautiful_find_entry(col, old_value, table_state, NULL, &indices);
  char* new_data = get_by_tindex(0, table_state);
  memcpy(&new_data[table_state->col_offsets[col]], new_value, TYPE_SIZE(table_state->col_types[col]));

  if(IS_KEY(table_state->col_types[col])) {
    size_t c = find_entry(col, new_data, table_state, NULL, NULL);
    if (c != 0) {
      if (count) free(indices);
      free(new_data);
      return; // entry with same value already exist
    }
  }

  for (size_t i = 0; i < count; i++) {
    table_state->last_inserted = indices[i];
    if (IS_KEY(table_state->col_types[col])) {
      rbtree* rbt = table_state->rb_trees[table_state->key_col_relpos[col]];
      rb_delete(rbt, indices[i], 1);
      rb_insert(rbt, new_data);
    } else {
      size_t offset = entry_offset(indices[i], table_state);
      fseek(table_state->file, offset + table_state->col_offsets[col], SEEK_SET);
      fwrite(&((char*)new_data)[table_state->col_offsets[col]], TYPE_SIZE(table_state->col_types[col]), 1, table_state->file);
    }
  }
  if (count) free(indices);
  free(new_data);
}

size_t tdelete(void *data, TABLE_STATE* table_state) {
  size_t col = *(size_t*)data;
  void* value = &((char*)data)[sizeof(size_t)];
  char* query = get_by_tindex(0, table_state);
  memcpy(&query[table_state->col_offsets[col]], value, TYPE_SIZE(table_state->col_types[col]));
  size_t is_key = IS_KEY(table_state->col_types[col]);
  if (is_key) {
    size_t *index;
    size_t count = find_entry(col, query, table_state, NULL, &index); // TODO :
    for (size_t i = 0; *index && i < table_state->nkey_cols; i++) {
      rb_delete(table_state->rb_trees[i], *index, 0);
    }
    if (query) free(query);
    return *index;
  } else {
    size_t *indices;
    size_t count = find_entry(col, query, table_state, NULL, &indices);
    for (size_t i = 0; i < count; i++) {
      for (size_t j = 0; j < table_state->nkey_cols; j++) {
        rb_delete(table_state->rb_trees[j], indices[i], 1);
      }
      // set_free(indices[i])
      size_t next_empty = next_empty_read(table_state->file);
      next_empty_write(table_state->file, indices[i]);
      
      size_t offset_parent = entry_offset(indices[i], table_state) + 0 + sizeof(size_t) * RB_INDEX_PARENT;
      fseek(table_state->file, offset_parent, SEEK_SET);
      fwrite(&next_empty, sizeof(size_t), 1, table_state->file);
      
      size_t offset_color = entry_offset(indices[i], table_state) + 0 + sizeof(size_t) * RB_INDEX_COLOR;
      fseek(table_state->file, offset_color, SEEK_SET);
      size_t minusone = -1;
      fwrite(&minusone, sizeof(size_t), 1, table_state->file);
    }
    if (query) free(query);
  }
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

size_t next_empty_write(FILE* file, size_t node_ptr) {
  fseek(file, 1, SEEK_SET);
  return fwrite(&node_ptr, sizeof(size_t), 1, file);
}

size_t next_empty_withdraw(TABLE_STATE* ts) {
  size_t curr_empty = next_empty_read(ts->file);
  if (curr_empty != 0) {
    size_t* next_empty_entry = get_by_tindex(curr_empty, ts);
    next_empty_write(ts->file, next_empty_entry[RB_INDEX_PARENT]);
    free(next_empty_entry);
  }
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
    // printf("%ld\n", TYPE_SIZE(type_));
  }

  table_state->nkey_cols = nkey_cols;
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

size_t *read_rb_head(size_t col, TABLE_STATE* ts) {
  if (!IS_KEY(ts->col_types[col])) {
    return 0;
  }
  // size_t offset = ts->key_col_relpos[col] * sizeof(size_t) + NAMES_OFFSET + ts->ncols*(ts->name_len+1);
  size_t offset = 
    NAMES_OFFSET + 
    ts->ncols*(ts->name_len+1) +
    ts->key_col_relpos[col] * RB_DATA_SIZE;
  fseek(ts->file, offset, SEEK_SET);
  size_t rb_head[RB_DATA_LEN];
  fread(&rb_head, RB_DATA_SIZE, 1, ts->file);
  return NULL;
}

size_t write_rb_head(size_t col, size_t rb_head, TABLE_STATE* ts) {
  if (!IS_KEY(ts->col_types[col])) {
    return 0;
  }
  size_t offset = ts->key_col_relpos[col] * sizeof(size_t) + NAMES_OFFSET + ts->ncols*(ts->name_len+1);
  fseek(ts->file, offset, SEEK_SET);
  return fwrite(&rb_head, sizeof(size_t), 1, ts->file);
}

void header_read(FILE* file, TABLE_STATE* table_state) {
  rewind(file);

  table_state->file      = file;
  table_state->version   = table_version(file);
  // table_state->stage_next_free = next_empty_read(file);
  assert(table_state->version == sizeof(size_t));
  table_state->ncols     = read_ncols(file);
  table_state->name_len  = read_name_len(file);

  read_col_types(table_state);

  table_state->col_names = (char**)malloc(sizeof(char*) * table_state->ncols);

  for (size_t i = 0; i < table_state->ncols; i++) {
    table_state->col_names[i] = read_field_name(i, table_state);
  }

  table_state->header_offset = ftell(file); // INFO_HEADER_LEN + table_state->ncols * ((table_state->name_len + 1) + sizeof(size_t));


  table_state->rb_trees = malloc(sizeof(rbtree*) * table_state->nkey_cols);
  for (size_t i = 0; i < table_state->nkey_cols; i++) {
    read_rb_head(i, table_state);
  }

  
  fseek(file, 0L, SEEK_END);
  table_state->stage_append_offset = table_state->append_offset = ftell(file);
  // printf("header_offset: %lx\nappend_offset: %lx\n", table_state->header_offset, table_state->append_offset);
  rewind(file);
  table_state->file = file;
  table_state->init = 1;
  table_state->stage = (struct darray){ 0 };
  table_state->entry_raw_size = table_state->entry_metadata_size + table_state->entry_size;

  for(size_t i = 0; i < table_state->ncols; i++) {
    int (*cmp)(const void*, const void*, const void*) = pick_cmp(table_state->col_types[i]);
    if (IS_KEY(table_state->col_types[i])) {
      table_state->rb_trees[table_state->key_col_relpos[i]] = rb_restore_from_table(table_state->key_col_relpos[i], table_state, cmp);
    }
  }
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
  size_t zero = 0;
  for (size_t i = 0; i < table_state->entry_metadata_size; i+=sizeof(size_t)) {
    memcpy(it, &zero, sizeof(size_t));
    it+=sizeof(size_t);
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
    if (TYPE_NUMBER(table_state->col_types[i]) == TABLE_TYPE_DATATIME) {
      const char* val = va_arg(args, const char*);
      memcpy(it, val, DATATIME_SIZE);
      it += TYPE_SIZE(table_state->col_types[i]);
    }
  }
  STAGE_EVENT *se = malloc(sizeof(STAGE_EVENT));
  se->type = SE_CREATE;
  se->data = data;
  da_append(&table_state->stage, se);
  va_end(args);
  // printf("WRITING\n");
  // display_entry(data, table_state->entry_raw_size);
}

void commit_changes(TABLE_STATE* table_state) {
  for (size_t i = 0; i < table_state->stage.count; i++) {
    STAGE_EVENT* se = table_state->stage.items[i];
    if (se->type == SE_CREATE) {
      tappend(se->data, table_state);
      free(se->data);
    }
    if (se->type == SE_DELETE) {
      tdelete(se->data, table_state);
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
  if (access(file_name, F_OK) == 0) {// Check if the file doesn't exist
    return 1;
  }
  FILE* file = fopen(file_name, "wb");
  header_write(ncols, name_len, col_types, col_names, file);
  fclose(file);
  return 0;
}

size_t open_table(const char* file_name, TABLE_STATE* table_state) {
  if (access(file_name, F_OK) != 0) { // Check if the file does exist
    return 1;
  }
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

void display_entry(unsigned char* entry, size_t size, TABLE_STATE *table_state) {
  #ifdef DEBUG
  printf("sz: %ld\n", size);
  for (size_t i = 0; i < size; i+=2) {
    if (i + 1 < size) printf("%02x%02x ", entry[i], entry[i+1]);
    if (i + 1 == size) printf("%02x", entry[i]);
    if (i % 16 == 14) printf("\n");
  }
  printf("\n");
  #endif
  for (size_t i = 0; i < table_state->ncols; i++) {
    printf("[%ld]: ", i);
    if (TYPE_NUMBER(table_state->col_types[i]) == TABLE_TYPE_INT) {
      printf("%s: %d\n", table_state->col_names[i], *(int*)&entry[table_state->col_offsets[i]]);
    }
    if (TYPE_NUMBER(table_state->col_types[i]) == TABLE_TYPE_FLOAT) {
      printf("%s: %f\n", table_state->col_names[i], *(float*)&entry[table_state->col_offsets[i]]);
    }
    if (TYPE_NUMBER(table_state->col_types[i]) == TABLE_TYPE_DATATIME) {
      int Y =    (int)entry[table_state->col_offsets[i]]                // e8
            +  (((int)entry[table_state->col_offsets[i] + 1]&0xf0)<<4); // 7
      int M =  (((int)entry[table_state->col_offsets[i] + 1]&0x0f)<<4)  //  0
            +  (((int)entry[table_state->col_offsets[i] + 2]&0xf0)>>4); // c
      int D =  (((int)entry[table_state->col_offsets[i] + 2]&0x0f)<<4)  //  0
            +  (((int)entry[table_state->col_offsets[i] + 3]&0xf0)>>4); // e
      int h =  (((int)entry[table_state->col_offsets[i] + 3]&0x0f)<<4)  //  0
            +  (((int)entry[table_state->col_offsets[i] + 4]&0xf0)>>4); // b
      int m =  (((int)entry[table_state->col_offsets[i] + 4]&0x0f)<<4)  //  0
            +  (((int)entry[table_state->col_offsets[i] + 5]&0xf0)>>4); // b
      int s =  (((int)entry[table_state->col_offsets[i] + 5]&0x0f)<<4)  //  0
            +  (((int)entry[table_state->col_offsets[i] + 6]&0xf0)>>4); // b
      int ms = (((int)entry[table_state->col_offsets[i] + 6]&0x0f)<<8)  //  0
            +  (((int)entry[table_state->col_offsets[i] + 7]&0xff));    // 7b
      printf("%s: Y%04d M%02d D%02d h%02d m%02d s%02d ms%02d\n", 
              table_state->col_names[i],
              Y, M, D, h, m, s, ms);
    }
    if (TYPE_NUMBER(table_state->col_types[i]) == TABLE_TYPE_VARCHAR) {
      printf("%s: `%s'\n", table_state->col_names[i], &entry[table_state->col_offsets[i]]);
    }
  }
  printf("\n");
}

void rb_table_update(rbtree* rbt, rbnode* node) {
  return;
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
    size_t offset = entry_offset(i, table_state);
    fseek(table_state->file, offset, SEEK_SET);
    char* entry = malloc(table_state->entry_raw_size);
    size_t r = fread(entry, sizeof(char), table_state->entry_raw_size, table_state->file);

    // char *val = malloc(sizeof(size_t) + TYPE_SIZE(table_state->col_types[0]));
    // memcpy(val, &i, sizeof(size_t));
    // memcpy(val + sizeof(size_t) / sizeof(char), &entry[table_state->col_offsets[0]], TYPE_SIZE(table_state->col_types[0]));
    rb_insert(rbt, entry);
  }
}

// must free all entries
size_t find_entry(size_t col, void* value, TABLE_STATE* table_state, void** result, size_t** indices) {
  size_t is_key = IS_KEY(table_state->col_types[col]);
  if (is_key) {
    size_t index = rb_find(table_state->rb_trees[table_state->key_col_relpos[col]], value);
    if (result && index) {
      void** a = malloc(sizeof(void*));
      a[0] = get_by_tindex(index, table_state);
      *result = a;
    }
    if (indices && index) {
      *indices = malloc(sizeof(size_t));
      (*indices)[0] = index;
    }
    return (index ? 1 : 0);
  } else {
    rbtree rbt = { 0 };
    rbt.col = col;
    rbt.table_state = table_state;
    int (*cmp)(const void*, const void*, const void*) = pick_cmp(table_state->col_types[col]);
    rbt.compare = cmp;
    struct darray entr = { 0 }, indx = { 0 };
    size_t diff = (table_state->append_offset - table_state->header_offset); 
    size_t len = diff / table_state->entry_raw_size;
    // fprintf(stderr, "DIFF: %ld Len: %ld\n", diff, len);
    size_t count = 0;
    for (size_t i = 1; i < len; i++) {
      void* entry = get_by_tindex(i, table_state);  
      // display_entry(entry, table_state->entry_raw_size);
      if (((size_t*)entry)[RB_INDEX_COLOR] == -1) // deleted row
        continue;
      if (rbt.compare(&rbt, value, entry) == 0) {
        count++;
        if (result)
          da_append(&entr, entry);
        if (indices)
          da_append(&indx, i);
      }
      if (!result) free(entry);
    }
    if (result)
      *result = entr.items;
    if (indices)
      *indices = indx.items;
    return count;
  }
}

size_t beautiful_find_entry(size_t col, void* value, TABLE_STATE* table_state, void** result, size_t** indices) {
  char* query = get_by_tindex(0, table_state);
  memcpy(&query[table_state->col_offsets[col]], value, TYPE_SIZE(table_state->col_types[col]));
  size_t ret = find_entry(col, query, table_state, result, indices);
  free(query);
  return ret;
}

size_t delete_entry(size_t col, void* value, TABLE_STATE* table_state) {
  size_t size = TYPE_SIZE(table_state->col_types[col]);
  STAGE_EVENT *se = malloc(sizeof(STAGE_EVENT));
  se->type = SE_DELETE;
  se->data = malloc(sizeof(size_t) + size);
  memcpy(se->data, &col, sizeof(size_t));
  memcpy(&se->data[sizeof(size_t)], value, size);
  da_append(&table_state->stage, se);
  return 0;
}

size_t edit_entry(size_t col, void* old_value, void* new_value, TABLE_STATE* table_state) {
  size_t size = TYPE_SIZE(table_state->col_types[col]);
  STAGE_EVENT *se = malloc(sizeof(STAGE_EVENT));
  se->type = SE_EDIT;
  se->data = malloc(sizeof(size_t) + 2 * size);
  memcpy(se->data, &col, sizeof(size_t));
  memcpy(&se->data[sizeof(size_t)], old_value, size);
  memcpy(&se->data[sizeof(size_t) + size], new_value, size);
  da_append(&table_state->stage, se);
  return 0;
}

size_t create_backup(const char* file_name, const char* backup_name) {
  archive(0, file_name, backup_name);
}

size_t restore_from_backup(const char* file_name, const char* backup_name) {
  archive(1, file_name, backup_name);
}

#endif // TABLE_FILE_H_IMPLEMENTATION
