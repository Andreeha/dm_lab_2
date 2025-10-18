#define TABLE_FILE_H_IMPLEMENTATION
#include "file.h"
#include <ctype.h>

#define TKN_FLOAT 0
#define TKN_INT   1
#define TKN_WORD  2
#define TKN_OP    3
#define TKN_STR   4

typedef struct {
  size_t position;
  size_t type;
  char* content;
} Token;

Token* get_token_word(FILE* file) {
  Token *tkn = malloc(sizeof(Token));
  tkn->content = malloc(256 * sizeof(char));
  tkn->position = ftell(file);
  char c;
  size_t index = 0;
  while ((c = getc(file)) != EOF && (isalpha(c) || isdigit(c) || c == '_')) {
    tkn->content[index++] = c;
  }
  tkn->content[index] = 0;
  tkn->type = TKN_WORD;
  return tkn;
}

Token* get_token_str(FILE* file) {
  Token *tkn = malloc(sizeof(Token));
  tkn->content = malloc(256 * sizeof(char));
  tkn->position = ftell(file);
  char c, p = '"';
  size_t index = 0;
  while ((c = getc(file)) != EOF && (isalpha(c) || isdigit(c) || c == '_' || (c == '"' && p == '\\'))) {
    tkn->content[index++] = p = c;
  }
  tkn->content[index] = 0;
  tkn->type = TKN_STR;
  return tkn;
}

Token* get_token_number(FILE* file) {
  Token *tkn = malloc(sizeof(Token));
  tkn->content = malloc(256 * sizeof(char));
  tkn->position = ftell(file);
  char c;
  size_t index = 0, dot = 0;
  while ((c = getc(file)) != EOF && (isdigit(c) || (c == '.' && dot++ == 0))) {
    tkn->content[index++] = c; 
  }
  if (dot) tkn->type = TKN_FLOAT;
  else tkn->type = TKN_INT;
  return tkn;
}

void* tokenize(const char* file_name, Token*** tokens) {
  struct darray tkns;
  FILE* file = fopen(file_name, "rb+");
  char c;
  while((c = getc(file)) != EOF) {
    size_t position = ftell(file) - 1;
    Token *tkn;
    if (c == '"') {
      tkn = get_token_str(file);
      da_append(&tkns, tkn);
      continue;
    } else if (isalpha(c)) {
      fseek(file, position, SEEK_SET);
      tkn = get_token_word(file);
    } else if (isspace(c)) {
      continue;
    } else if (isdigit(c)) {
      fseek(file, position, SEEK_SET);
      tkn = get_token_number(file);
    } else { // special symbol
      tkn = malloc(sizeof(Token));
      tkn->position = position;
      tkn->content = malloc(sizeof(char));
      tkn->content[0] = c;
      tkn->type = TKN_OP;
      da_append(&tkns, tkn);
      continue;
    }
    fseek(file, -1, SEEK_CUR);
    da_append(&tkns, tkn);
  }
  for (size_t i = 0; i < tkns.count; i++) {
    printf("pos: %ld type: %ld content: `%s'\n", ((Token**)tkns.items)[i]->position, ((Token**)tkns.items)[i]->type, ((Token**)tkns.items)[i]->content);
  }
  if (tokens)
    *tokens = tkns.items;
}

size_t is_update(size_t offset, const Token** tokens) {
  if (0 == strcmp(tokens[offset]->content, "UPDATE")) {
    
  }
}

int main () {
  Token** tokens;
  tokenize("data/program.txt", &tokens);
  return 0;
}
