%{

#include <regex.h>
#include <stdio.h>
#include "qryp.h"

%}

%union {
  qnode_t *node;
  qnode_op_t op;
  long fix;
  double flo;
  char *str;
  regex_t *rxp;
}

%type <node> top query query1 query2 keyval
%type <fix> fix
%type <flo> flo
%type <op> cmpop
%token '=' '>' '<' '(' ')' '!' '-'
%token TLE TGE TNE TAND TOR
%token <fix> TFIX
%token <flo> TFLO
%token <str> TWORD
%token <str> TSTR
%token <rxp> TRXP

%%

top:
  query {
    loop($$);
  }

query:
  query1
| '!' query {
    $$ = mkqnode(QNEG);
    $$->left = $2;
  }

query1:
  query2
| query1 TAND query2 {
    $$ = mkqnode(QAND);
    $$->left = $1;
    $$->right = $3;
  }
| query1 TOR query2 {
    $$ = mkqnode(QOR);
    $$->left = $1;
    $$->right = $3;
  }

query2:
  TRXP {
    $$ = mkqnode(QRXP);
    $$->regexp = $1;
  }
| TSTR {
    $$ = mkqnode(QIN);
    $$->valstr = $1;
    $$->value_type = QVALSTR;
  }
| TWORD {
    $$ = mkqnode(QIN);
    $$->valstr = $1;
    $$->value_type = QVALSTR;
  }
| fix {
  puts("fix!");
  }
| keyval {
    $$ = $1;
  }
| '(' query ')' {
    $$ = $2;
  }

keyval:
  TWORD cmpop TSTR {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = $2;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD cmpop TWORD {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = $2;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD cmpop fix {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = $2;
    $$->valfix = $3;
    $$->value_type = QVALFIX;
  }
| TWORD cmpop flo {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = $2;
    $$->valfix = $3;
    $$->value_type = QVALFLO;
  }

fix:
  '-' TFIX { $$ = -1 * $2; }
| TFIX { $$ = $1; }

flo:
  '-' TFLO { $$ = (-1.0) * $2; }
| TFLO { $$ = $1; }

cmpop:
  '>' { $$ = QOPGT; }
| '<' { $$ = QOPLT; }
| '=' { $$ = QOPEQ; }
| TLE { $$ = QOPLE; }
| TGE { $$ = QOPGT; }
| TNE { $$ = QOPNE; }

%%
