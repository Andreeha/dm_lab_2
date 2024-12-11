
#define NCOLS_OFFSET (1+DATA_OFFSET)
#define FMLEN_OFFSET (1+NCOLS_OFFSET)
#define COL_TYPES_OFFSET (1 + FMLEN_OFFSET)

char table_version(FILE* file);
size_t next_empty_read(FILE* file);
size_t next_empty_update(TABLE_STATE* ts);
char read_ncols(FILE* file);
char read_name_len(FILE* file);
void read_col_types(TABLE_STATE* table_state);
char* read_field_name(size_t col, TABLE_STATE* ts);
size_t read_rb_head(size_t col, TABLE_STATE* ts);


char table_version(FILE* file) {
  fseek(file, 0L, SEEK_SET);
  char version;
  fread(&version, sizeof(char), 1, file);
  return version;
}

size_t next_empty_read(FILE* file) {
  fseek(ts->file, 1, SEEK_SET);
  size_t pos;
  fread(&pos, sizeof(size_t), 1, file);
  return pos;
}

size_t next_empty_update(TABLE_STATE* ts) {
  size_t curr_empty = next_empty_read(ts);
  assert(curr_empty != 0);
  // TODO: 
}

char read_ncols(FILE* file) {
  fseek(file, NCOLS_OFFSET, SEEK_SET);
  char ncols;
  fread(&ncols, sizeof(char), 1, file);
  return ncols;
}

char read_name_len(FILE* file) {
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
  table_state->key_col_offsets = malloc(sizeof(size_t) * table_state->ncols);
  size_t nkey_cols;
  size_t col_offset = 0;
  size_t key_col_offset = 0;
  for (size_t i = 0; i < table_state->ncols; i++) {
    size_t type_ = table_state->col_types[i] = (0xff&info_header[2*i+2+data_offset]) | (0xff00&(info_header[2*i+3+data_offset]<<8));
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
  fread(name, 1, ts->name_len + 1, ts->file;
  return name;
}

size_t read_rb_head(size_t col, TABLE_STATE* ts) {
  if (!IS_KEY(ts->col_types[col])) {
    return 0;
  }
  size_t offset = ts->key_col_relpos * sizeof(size_t) + INFO_HEADER_LEN + ts->ncols*(ts->name_len+1);
  fseek(ts->file, offset, SEEK_SET);
  size_t rb_head;
  fread(&rb_head, sizeof(size_t), 1, ts->file);
  return rb_head;
}

void header_read(FILE* file, TABLE_STATE* table_state) {
  rewind(file);

  table_state->version   = table_version(file);
  assert(table_state->version == sizeof(size_t));
  table_state->ncols     = read_ncols(file);
  table_state->name_len  = read_name_len(file);

  read_col_types(table_state);

  table_state->col_names = (char**)malloc(sizeof(char*) * table_state->ncols);

  for (size_t i = 0; i < table_state->ncols; i++) {
    table_state->col_names[i] = read_field_name(i, table_state);
  }

  table_state->rb_heads = malloc(sizeof(size_t) * table_state->name_len;
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
  free(info_header);
}
