#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#define da_append((da), item) {\
          if ((da)->count == (da)->capacity) {\
            void* tmp = (void*)malloc(sizeof(void) * ((da)->capacity + 1) * 2);\
            if ((da)->items) {\
              memcpy(tmp, (da)->items, (da)->count);\
              free((da)->items);\
            }\
            (da)->items[(da)->count++] = item;
        }

enum DataTypes {
  integer,
  floating,
  varchar,
  boolean
};

struct Data {
  int type;
  void* data;
};

void pack_data(Data* data, int type, void* value) {
  data->type = type;
  data->data = value;
}

void pack_data(Data* data, int value) {
  int* a = (int*)malloc(sizeof(int));
  *a = value;
  pack_data(data, integer, a);
}

void pack_data(Data* data, char* s) {
  char* a = (char*)malloc(sizeof(char) * (strlen(s) + 1));
  strcpy(a, s);
  pack_data(data, varchar, a);
}

void pack_data(Data* data, bool value) {
  bool* a = (bool*)malloc(sizeof(bool));
  *a = value;
  pack_data(data, boolean, a);
}

void pack_data(Data* data, float value) {
  float* a = (float*)malloc(sizeof(float));
  *a = value;
  pack_data(data, floating, a);
}

void write_data(FILE* file, Data* data, const char* format="%$") {
  char* f = (char*)malloc(sizeof(char) * (strlen(format) + 1));
  strcpy(f, format);
  int idc = 0;
  for (int i = 0; f[i]; i++) {
    if (f[i] == '$') { idc = i; break; }
  }
  if (data->type == integer) {
    f[idc] = 'd';
    fprintf(file, f, *(int*)data->data);
  } else if (data->type == floating) {
    f[idc] = 'f';
    fprintf(file, f, *(int*)data->data);
  } else if (data->type == varchar) {
    f[idc] = 's';
    fprintf(file, f, (char*)data->data);
  } else if (data->type == boolean) {
    f[idc] = 's';
    fprintf(file, f, (*(bool*)data->data ? "true" : "false"));
  }
  free(f);
}

struct Cell {
  int row, col;
  Data* data;
};

struct Cells {
  int count, capacity;
  void* items;
};

struct Row {
  int file_pos_begin, file_pos_end;
  Cells* cols;
};

struct Table {
  int count, capacity;
  void* items;
  int ncols;
  int* col_types;
  char** col_names;
  int file_pos;
};

void read_row(FILE* in, Table* table) {
  Row* r = (Row*)malloc(sizeof(Row));
  r->file_pos_begin = table->file_pos;
  for (int i = 0; i < table->ncols; i++) {
    read_column(in, table);
  }
}

std::string read_data(FILE* file, char delim, Table* table = NULL) {
  char c;
  std::string s;
  while ((c=fgetc(file)) != delim) {
    s += c;
    if (table) table->file_pos++;
  }
  if (table) table->file_pos++;
  return s;
}

void write_header(FILE* out, Table* table) {
  fprintf(out, "%d\n", table->ncols);
  for (int i = 0; i < table->ncols; i++) {
    fprintf(out, "%s\n", table->col_names[i]);
  }
  for (int i = 0; i < table->ncols; i++) {
    fprintf(out, "%dH", table->col_types[i]);
  }
}

void read_header(FILE* in, Table* table) {
  int ncols = atoi(read_data(in, '\n', table).c_str());
  int* col_types = (int*)malloc(sizeof(int) * ncols);
  char** col_names = (char**)malloc(sizeof(char*) * ncols);
  for (int i = 0; i < ncols; i++) {
    std::string s = read_data(in, '\n', table);
    col_names[i] = (char*)malloc(sizeof(char) * (s.length() + 1));
    strcpy(col_names[i], s.c_str());
  }
  for (int i = 0; i < ncols; i++) {
    col_types[i] = atoi(read_data(in, 'H', table).c_str());
  }
  table->ncols = ncols;
  table->col_names = col_names;
  table->col_types = col_types;
}

void write_table(FILE* out, Table* table) {
  write_header(out, table);
}

int main () {
  FILE* fout = fopen("name.mydb", "a");
  if (!fout) return 1;
  int col_types[] = {0,1,2,3};
  const char* col_names[] = {"1", "2", "3", "4"};
  Table t {0,0,NULL,4,(int*)&col_types, (char**)&col_names};
  write_header(fout, &t);
  write_header(stdout, &t);
  fclose(fout);
  printf("\n----------------\n");
  FILE* fin = fopen("name.mydb", "r");
  if (!fin) return 1;
  Table t2;
  read_header(fin, &t2);
  write_header(stdout, &t2);
  fclose(fin);
  return 0;
}
