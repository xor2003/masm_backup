#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "masm_lexer.h"
#include "masm.h"


struct masm_lexer {
  struct istream* m_ifs;
  char* m_buffer;
  char* m_cursor;
  char* m_limit;
  char* m_token;
  char* m_marker;
  size_t m_buffer_size;
  int m_lineno;
  int m_in_comment;
  int m_eof;
};


struct masm_lexer* masm_lexer_init(struct istream* ifs, size_t init_size) {
  struct masm_lexer* out = (struct masm_lexer*)malloc(sizeof(struct masm_lexer));
  out->m_buffer_size = init_size;
  out->m_buffer = (char*)malloc(init_size);
  memset (out->m_buffer, 0, init_size);
  out->m_cursor = out->m_limit = out->m_token = out->m_marker = out->m_buffer;
  out->m_ifs = ifs;
  out->m_lineno = 1;
  out->m_in_comment = 0;
  out->m_eof = 0;
  return out;
}

void masm_lexer_done(struct masm_lexer* victim) {
  if (victim) {
    if (victim->m_buffer) {
      free(victim->m_buffer);
    }
    free(victim);
  }
}

static int fill(struct masm_lexer* lexer, int n) {
  int rest_size = lexer->m_limit - lexer->m_token;
  int i = 0;
  char* newBuffer = NULL;
  int read_size = 0;
  ssize_t has_bytes = 0;
  if (istream_eof (lexer->m_ifs)) {
    if (lexer->m_limit - lexer->m_cursor <= 0) {
      lexer->m_eof = 1;
      return 0;
    }
  }
  if (rest_size+n >= lexer->m_buffer_size) {
	/* extend buffer */
    lexer->m_buffer_size *= 2;
	newBuffer = (char*)malloc(lexer->m_buffer_size);
    memset(newBuffer, 0, lexer->m_buffer_size);
	for (i=0; i<rest_size; ++i) { // memcpy
	    *(newBuffer+i) = *(lexer->m_token+i);
	}
    lexer->m_cursor = newBuffer + (lexer->m_cursor-lexer->m_token);
    lexer->m_token = newBuffer;
    lexer->m_limit = newBuffer + rest_size;

	free(lexer->m_buffer);
    lexer->m_buffer = newBuffer;
    } else {
	// move remained data to head.
	for (i=0; i<rest_size; ++i) { //memmove( m_buffer, m_token, (restSize)*sizeof(char) );
	    *(lexer->m_buffer+i) = *(lexer->m_token+i);
	}
    lexer->m_cursor = lexer->m_buffer + (lexer->m_cursor-lexer->m_token);
    lexer->m_token = lexer->m_buffer;
    lexer->m_limit = lexer->m_buffer+rest_size;
    }

    // fill to buffer
    read_size = lexer->m_buffer_size - rest_size;
    has_bytes = istream_read (lexer->m_ifs, lexer->m_limit, read_size);
    lexer->m_limit += has_bytes;
    return 1;
}

void masm_lexer_get_token(struct masm_lexer* lexer, struct masm_token* tok) {
  if (lexer == NULL || tok == NULL) {
    return;
  }
  tok->line = lexer->m_lineno;
  tok->begin = lexer->m_token;
  tok->end = lexer->m_cursor;
}

int masm_lexer_is_eof(struct masm_lexer* lexer) {
  return lexer->m_eof;
}

static int icase_cmp_token(struct masm_lexer* lexer, const char* name) {
  size_t tok_size = lexer->m_cursor - lexer->m_token;
  int i = 0;
  if (strlen(name) != tok_size) {
    return 0;
  }
  for (i=0; i<tok_size; i++) {
    if (tolower(name[i]) != tolower (lexer->m_token[i])) {
      return 0;
    }
  }
  return 1;
}

#define dTEST_MACRO(NAME) icase_cmp_token(lexer, NAME)

static int handle_identifier(struct masm_lexer* lexer) {
#include "lexer_kw_keywords.h"
  return MASM_ID;
}

int masm_lexer_scan(struct masm_lexer* lexer) {
  std:;
    lexer->m_token = lexer->m_cursor;

  /*!re2c
      re2c:define:YYCTYPE = "char";
      re2c:define:YYCURSOR = lexer->m_cursor;
      re2c:define:YYMARKER = lexer->m_marker;
      re2c:define:YYLIMIT = lexer->m_limit;
      re2c:define:YYFILL:naked = 1;
      re2c:define:YYFILL@len = #;
      re2c:define:YYFILL = "if (!fill(lexer, #)) { return lexer->m_in_comment ? MASM_COMMENT : MASM_EOL; }";
      re2c:yyfill:enable = 1;
      re2c:indent:top = 2;
      re2c:indent:string="    ";

      INTEGER                = [1-9][0-9]*;
      WS                     = [ \r\t\f];
      ANY_CHARACTER          = [^];
      EOL                    = [\n];

      D        = [0-9] ;
      L        = [a-zA-Z] ;
      identifier = L ( L | D )*;

      WS { goto std; }
      EOL { lexer->m_lineno++;  return MASM_EOL; }
      ";" { goto comment; }
      !include "lexer_keywords.h";

      ANY_CHARACTER { goto any_char; }

    */
    comment:;
    lexer->m_in_comment = 1;
    /*!re2c
     [^\r\n]+ { lexer->m_in_comment = 0; return MASM_COMMENT; }
     [\r\n] { return MASM_EOL;}
     */

    any_char:;
    /*!re2c
     [a-zA-Z] [a-zA-Z0-9]*  { return handle_identifier(lexer); }
     [^] { goto std; }
     */

    lexer->m_eof = 1;
    return MASM_EOL;
}