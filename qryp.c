#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "qryp.h"
#include "y.tab.h"

/* Field delimiter. Should be eventually customizable via flag */
const char zzfielddelim = ' ';

/* Line delimiter. */
const char zzlinedelim = '\n';

FILE *zzin;
FILE *zzout;

typedef struct bit {
  unsigned long key_beg; /* pointer to key in buffer */
  unsigned long val_beg;
  size_t key_sz;
  size_t val_sz;
  qnode_value_t val_type;
  union { /* cached values */
    long fixnum;
    double flonum;
  };
  /* TODO: flag for cache */
} bit_t;

typedef struct buffer {
  char *buf;
  unsigned long buf_i;
  size_t buf_sz;
} buffer_t;

/* The current line. Since qryp only cares about the current line,
 * this is more or less a singleton, but it doesn't have to be.
 */
struct cursor {
  buffer_t buffer;

  /* Storage for key value pairs within */
  bit_t *bits;
  unsigned long bits_i;
  size_t bits_sz;
};

struct lex {
  buffer_t buffer;
  unsigned long lineno;
  unsigned long colno;
  char peek;
};

static struct lex lexstate;
static struct cursor zzcur;

static void
buffer_init(buffer_t *buf, size_t init_sz)
{
  buf->buf = calloc(init_sz, sizeof(*buf->buf));
  if (buf->buf == NULL) {
    fputs("ERROR: Out of memory. Aborting.\n", stderr);
    exit(EXIT_FAILURE);
  }

  buf->buf_sz = init_sz;
  buf->buf_i = 0;
}

static void
buffer_free(buffer_t *buf)
{
  if (buf->buf) {
    free(buf->buf);
  }
  buf->buf_sz = 0;
  buf->buf_i = 0;
}

static void
buffer_append(buffer_t *buf, char c)
{
  if ((buf->buf_i + 1) >= buf->buf_sz) {
    /* reallocate */
    buf->buf_sz *= 2;
    buf->buf = realloc(buf->buf, buf->buf_sz * sizeof(*buf->buf));
    if (buf->buf == NULL) {
      fputs("ERROR: Out of memory. Aborting\n", stderr);
      exit(EXIT_FAILURE);
    }
  }

  buf->buf[buf->buf_i] = c;
  buf->buf_i++;
  /* Always terminate, even though we keep the size */
  buf->buf[buf->buf_i] = '\0';
}

/* Soft reset of cursor state. Keep the allocations, just reset pointers. */
static void
cursor_reset(struct cursor *cur)
{
  cur->buffer.buf_i = 0;
  cur->bits_i = 0;
}

static void
cursor_init(struct cursor *cur, size_t buf_sz)
{
  buffer_init(&(cur->buffer), buf_sz);
  cursor_reset(cur);
  cur->bits = NULL;
  cur->bits_sz = 0;
}

/* Appends a bit to the cursor after parsing the current line */
static void
cursor_bitappend(struct cursor *cur, bit_t bit)
{
  if ((cur->bits_i + 1) >= cur->bits_sz) {
    /* reallocate */
    cur->bits_sz = (cur->bits_sz == 0) ? 32: cur->bits_sz * 2;
    cur->bits = realloc(cur->bits, cur->bits_sz * sizeof(*cur->bits));
    if (cur->bits == NULL) {
      fputs("ERROR: Out of memory. Aborting\n", stderr);
      exit(EXIT_FAILURE);
    }
  }

  cur->bits[cur->bits_i] = bit;
  cur->bits_i++;
}


/* Locates a bit in cursor with named key */
static long
cursor_bitfind(struct cursor *cur, char *key, size_t key_sz)
{
  char *bitkey;
  int i;

  for (i = 0; i < cur->bits_i; i++) {
    if (cur->bits[i].key_sz == key_sz) { /* might have a match */
      bitkey = &(cur->buffer.buf[cur->bits[i].key_beg]);
      if (strncmp(bitkey, key, key_sz) == 0) {
        return i;
      }
    }
  }
  return -1;
}


/**
 * Makes a zeroed qnode initialized to `type`
 */
qnode_t *
mkqnode(qnode_type_t type)
{
  qnode_t *node = calloc(1, sizeof(qnode_t));
  if (!node) {
    fputs("ERROR: Out of memory. Aborting.\n", stderr);
    exit(EXIT_FAILURE);
  }

  /* NULL any pointers (in case NULL != 0) */
  node->left = NULL;
  node->right = NULL;
  node->regexp = NULL;
  node->valstr = NULL;
  node->name = NULL;

  return node;
}

/**
 * Returns 1 match of `l` against the qnode, `n`
 */
static int
qnode_match(qnode_t *n, struct cursor *l)
{
  if (n == NULL || l == NULL) {
    return 0;
  }

  switch (n->type) {
  case QNEG:
    return !qnode_match(n->left, l);
  case QAND:
    return qnode_match(n->left, l) && qnode_match(n->right, l);
  case QOR:
    return qnode_match(n->left, l) || qnode_match(n->right, l);
  case QKEY:
    return 1;
  case QRXP:
    return 1;
  case QIN:
    return 1;
  default:
    fputs("ERROR: Invalid query type.\n", stderr);
  }
  return 0;
}

/**
 * Lexer
 */

static int
lexnext()
{
  int c;
  if (lexstate.peek != EOF) {
    c = lexstate.peek;
    lexstate.peek = EOF;
    return c;
  }
  if (lexstate.buffer.buf_i == lexstate.buffer.buf_sz) {
    return EOF;
  }
  lexstate.colno++;
  c = lexstate.buffer.buf[lexstate.buffer.buf_i];
  lexstate.buffer.buf_i++;
  return c;
}

static int
lexnum(int c)
{
  int i, ret = EOF, flop = 0;
  char *endptr;
  buffer_t buf;
  buffer_init(&buf, 32);
  buffer_append(&buf, c);

  for (i = 0; i < 32; i++) {
    c = lexnext();
    if (isdigit(c)) {
      buffer_append(&buf, c);
    }
    else if (c == '.' || c == 'e' || c == 'E') {
      flop = 1;
      buffer_append(&buf, c);
    }
    else {
      lexstate.peek = c;
      break;
    }
  }

  if (buf.buf_i > 0 && flop) {
    /* parse as flonum */
    yylval.flo = strtod(buf.buf, &endptr);
    if ((errno == ERANGE) || (buf.buf == endptr)
        || (errno != 0 && yylval.flo == 0)) {
      fprintf(stderr, "ERROR: Unable to parse '%s' as flonum: line %d:%d\n",
              buf.buf, lexstate.lineno, lexstate.colno);
      goto error;
    }
    ret = TFLO;
    goto done;
  }
  else if (buf.buf_i > 0) {
    /* parse as fixnum */
    yylval.fix = strtol(buf.buf, &endptr, 10);
    if ((errno == ERANGE &&
         (yylval.fix == LONG_MAX || yylval.fix == LONG_MIN))
        || (buf.buf == endptr)
        || (errno != 0 && yylval.fix == 0)) {
      fprintf(stderr, "ERROR: Unable to parse '%s' as fixnum: line %d:%d\n",
              buf.buf, lexstate.lineno, lexstate.colno);
      goto error;
    }
    ret = TFIX;
    goto done;
  }
  else {
    fprintf(stderr, "ERROR: Expected number: line %d:%d\n",
            lexstate.lineno, lexstate.colno);
    goto error;
  }

 error:
  buffer_free(&buf);
  exit(EXIT_FAILURE);

 done:
  buffer_free(&buf);
  return ret;
}

static int
lexword(int c)
{
  buffer_t buf;
  buffer_init(&buf, 32);
  buffer_append(&buf, c);

  for (;;) {
    c = lexnext();
    if (isalpha(c)) {
      buffer_append(&buf, c);
    }
    else if (c == '#' || c == '.' || c == '_') {
      buffer_append(&buf, c);
    }
    else {
      lexstate.peek = c;
      goto done;
    }
  }

 error:
  buffer_free(&buf);
  exit(EXIT_FAILURE);

 done:
  yylval.str = strndup(buf.buf, buf.buf_i);
  if (yylval.str == NULL) {
    fputs("ERROR: Out of memory. Aborting.\n", stderr);
    goto error;
  }
  buffer_free(&buf);
  return TWORD;
}

static int
lexstr(int c)
{
  int d;
  buffer_t buf;
  buffer_init(&buf, 32);

  /* Unlike in lexnum / lexword, we throw away the initial c,
   * because it's just " */

  for (;;) {
    c = lexnext();
    switch (c) {
    case EOF:
      fprintf(stderr, "ERROR: Unterminated string found line: %d:%d\n",
              lexstate.lineno, lexstate.colno);
      goto error;
    case '\\':
      d = lexnext();
      switch (d) {
      case '\\':
        buffer_append(&buf, d);
        lexstate.peek = EOF;
        break;
      case '"':
        buffer_append(&buf, d);
        lexstate.peek = EOF;
        break;
      default:
        fprintf(stderr, "ERROR: Improper escape sequence line: %d:%d\n",
                lexstate.lineno, lexstate.colno);
      }
    case '"':
      c = EOF;
      goto done;
    default:
      buffer_append(&buf, c);
      break;
    }
  }

 error:
  buffer_free(&buf);
  exit(EXIT_FAILURE);

 done:
  yylval.str = strndup(buf.buf, buf.buf_i);
  if (yylval.str == NULL) {
    fputs("ERROR: Out of memory. Aborting.\n", stderr);
    goto error;
  }
  buffer_free(&buf);
  return TSTR;
}

static int
lexrxp(int c)
{
  return 0;
}

int
yylex()
{
  char c, d;
  for (;;) {
    c = lexnext();
    switch (c) {
    case EOF:
      return EOF;
    case '\n':
      lexstate.lineno++;
      lexstate.colno = 0;
    case '0': case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9':
      return lexnum(c);

    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
    case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
    case 's': case 't': case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z': case 'A': case 'B': case 'C': case 'D':
    case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
    case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
    case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V':
    case 'W': case 'X': case 'Y': case 'Z': case '_':
      return lexword(c);

    case '=': case '(': case ')': case '-':
      return c;
    case '"':
      return lexstr(c);
    case '>':
      d = lexnext();
      if (d == '=') {
        return TGE;
      }
      lexstate.peek = d;
      return c;
    case '<':
      d = lexnext();
      if (d == '=') {
        return TGE;
      }
      lexstate.peek = d;
      return c;
    case '!':
      d = lexnext();
      if (d == '=') {
        return TNE;
      }
      lexstate.peek = d;
      return c;
    case '&':
      d = lexnext();
      if (d == '&') {
        return TAND;
      }
      fprintf(stderr, "ERROR: Expected '&', found '%c' at %ld:%ld\n",
              c, lexstate.lineno, lexstate.colno);
      exit(EXIT_FAILURE);
    case '|':
      printf("op |\n");
      d = lexnext();
      if (d == '|') {
        return TAND;
      }
      fprintf(stderr, "ERROR: Expected '|', found '%c' at %ld:%ld\n",
              c, lexstate.lineno, lexstate.colno);
      exit(EXIT_FAILURE);
    case '/':
      return lexrxp(c);
    case ' ':
      lexstate.colno++;
      continue;
    default:
      fprintf(stderr, "Unknown character at %ld:%ld\n",
              lexstate.lineno, lexstate.colno);
      exit(EXIT_FAILURE);
    }
  }

  return EOF;
}

void
yyerror(char *s)
{
  fprintf(stderr, "ERROR: %s at line %d:%d\n",
          s, lexstate.lineno, lexstate.colno);
  exit(EXIT_FAILURE);
}


/* Read a line, and put it in zzcur */
static void
zznext(FILE *in)
{
  /* Read until a zzlinedelim is found, parsing while doing so. */
  /* We'll then attempt to call qnode_match on it, and transform */
  /* the output as necessary */

  int c = 0;
  int i = -1; /* haven't gotten a character yet */
  int curkeyval = 0; /* are we currently looking at a possible
                        key value pair? */
  int sawflo = 0;
  int start = 0;
  bit_t b;

#define NEXT_CHAR() {c = fgetc(in); i++; buffer_append(&zzcur.buffer, c);}

 dispatch:
  NEXT_CHAR();
  if (isdigit(c) || c == '-' || c == '+' || c == '.') {
    b.val_beg = i;
    if (!curkeyval) {
      b.key_sz = 0;
      b.key_beg = 0;
    }

#ifdef DEBUG
    fprintf(stderr, "Heading off to numval\n");
#endif

    goto numval;
  }
  else if (c == '"') {
    b.val_beg = i+1;
#ifdef DEBUG
    fprintf(stderr, "Heading off to qval\n");
#endif
    goto qval;
  }
  else if (isalpha(c) || c == '_') {
    /* if we're in a key value pair and get a key */
    if (!curkeyval) {
      b.key_beg = i;
      curkeyval = 1;
    }
    else {
      b.val_beg = i;
    }

#ifdef DEBUG
    fprintf(stderr, "Heading off to key\n");
#endif
    goto key;
  }
  else if (c == zzlinedelim) {
    return;
  }
  else { /* extras right away */
#ifdef DEBUG
    fprintf(stderr, "Heading off to extras\n");
#endif
    b.val_type = QVALSTR; /* e.g. just junk */
    b.val_beg = i; /* that starts here... */
    b.key_beg = 0;
    b.key_sz = 0;
    curkeyval = 0;
  }

 extras:
  for (;;) {
    if (c == EOF || c == zzlinedelim || c == zzfielddelim) {
      /* commit current bit. */
      b.val_sz = i - b.val_beg;
      cursor_bitappend(&zzcur, b);
      if (c == zzfielddelim) {
        goto dispatch;
      }
      return;
    }
    /* otherwise, just get the next character ... */
    NEXT_CHAR();
  }

 key:
  for (;;) {
    if (c == EOF || c == zzlinedelim || c == zzfielddelim) {
      /* If we get a delimiter, we must not have been at a key, since there
         can't possibly be a cooresponding value

         Unless we were in a keyval pair to begin with... Oddly.
      */
      b.val_beg = b.key_beg;
      b.key_beg = 0;
      b.key_sz = 0;
      b.val_sz = i - b.val_beg;
      b.val_type = QVALSTR;

      cursor_bitappend(&zzcur, b);

      if (c == zzfielddelim) {
        curkeyval = 0;
#ifdef DEBUG
        fprintf(stderr, "Oops. Headed off to dispatch\n");
#endif
        goto dispatch;
      }
      return;
    }
    else if (c == '=') {
      b.key_sz = i - b.key_beg;
      curkeyval = 1;
      goto dispatch;
    }
    else if (!(isalpha(c) || c == '#' || c == '.' || c == '_' || c == '-')) {
      curkeyval = 0;
      /* Then, it's not a key. Switch to value and jump to extras */
      b.val_beg = b.key_beg;
      b.key_beg = 0;
      b.val_type = QVALSTR;
      goto extras;
    }

    NEXT_CHAR();
  }

 numval:
  for (;;) {
    if (c == EOF || c == zzlinedelim || c == zzfielddelim) {
      b.val_sz = i - b.val_beg;
      if (sawflo) {
        b.val_type = QVALFLO;
        sawflo = 0;
      }
      else {
        b.val_type = QVALFIX;
      }
      cursor_bitappend(&zzcur, b);

      if (c == zzfielddelim) {
        goto dispatch;
      }
      return;
    }
    else if (c == '.' && !sawflo) {
      sawflo = 1;
    }
    else if (!(isdigit(c) || c == 'e' || c == 'E' || c == '+' || c == '-')) {
      /* Then, it's not a number. Switch to value and jump to extras */
      if (curkeyval) {
        b.val_beg = b.key_beg;
        b.key_beg = 0;
        b.val_type = QVALSTR;
      }
      goto extras;
    }

    NEXT_CHAR();
  }

 qval:
  for (;;) {
    NEXT_CHAR();
    if (c == EOF || c == zzlinedelim) {
      b.val_sz = i - b.val_beg;
      b.val_type = QVALSTR;
      return;
    }
    else if (c == '\\') {
      NEXT_CHAR();
      if (c != '"') { /* ??? */
        continue;
      }
    }
    else if (c == '"') {
      fprintf(stderr, "GOT A QUOTE. AWAY WITH YOU i=%d b.val_beg=%d.\n", i, b.val_beg);
      b.val_sz = i - b.val_beg; /* don't include the quote */
      b.val_type = QVALSTR;
      cursor_bitappend(&zzcur, b);
      goto dispatch;
    }
  }
}

void
loop(qnode_t *qry)
{
  cursor_init(&(zzcur), 4096);
  while (!feof(zzin)) {
    cursor_reset(&zzcur);
    zznext(zzin);

    /* does zzcur match against the qry? */
    if (qnode_match(qry, &zzcur)) {
      fprintf(zzout, zzcur.buffer.buf);
    }
  }
}

int
main(int argc, char **argv)
{
  int i;
  /* printf("Parsing: '%s'\n", argv[1]); */

  /* Initialize static lexstate */
  lexstate.lineno = 1;
  lexstate.peek = EOF;
  lexstate.colno = 0;

  /* ASSUME argv[1] is a query */
  lexstate.buffer.buf = argv[1];
  lexstate.buffer.buf_i = 0;
  lexstate.buffer.buf_sz = strlen(argv[1]);
  /* printf("Lexstate: buf=\"%s\" buf_i=%ld buf_sz=%ld\n", lexstate.buffer.buf, */
  /*        lexstate.buffer.buf_i, lexstate.buffer.buf_sz); */

  /* get the first line. then, print the cursor. */

  zzin = stdin;
  zzout = stdout;

  /* while (!feof(stdin)) { */
  /*   cursor_init(&(zzcur), 4096); */
  /*   zznext(zzin); */

  /*   for (i = 0; i < zzcur.bits_i; i++) { */
  /*     printf("BIT(%d): key_beg=%lu, key_sz=%lu, " */
  /*            " val_beg=%lu, val_sz=%lu, " */
  /*            " val_type=%d\n", i, */
  /*            zzcur.bits[i].key_beg, */
  /*            zzcur.bits[i].key_sz, */
  /*            zzcur.bits[i].val_beg, */
  /*            zzcur.bits[i].val_sz, */
  /*            zzcur.bits[i].val_type); */
  /*   } */
  /* } */


  yyparse();

  return 0;
}
