%{

#include <regex.h>
#include <stdio.h>
#include "qryp.h"

%}

%union {
  qnode_t *node;
  long fix;
  double flo;
  char *str;
  regex_t *rxp;
}

%type <node> top query query1 query2 keyval
%type <fix> fix
%type <flo> flo
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

keyval: /* TODO: refactor this into a single operator token.
                 then dispatch on it's value */
  TWORD '=' TSTR {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPEQ;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD '=' TWORD {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPEQ;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD '=' TRXP {
    $$ = mkqnode(QKEY);
  }
| TWORD '=' fix {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPEQ;
    $$->valfix = $3;
    $$->value_type = QVALFIX;
  }
| TWORD '=' flo {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPEQ;
    $$->valfix = $3;
    $$->value_type = QVALFLO;
  }
| TWORD TNE TSTR {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPNE;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD TNE TWORD {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPNE;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD TNE TRXP {
    $$ = mkqnode(QKEY);
  }
| TWORD TNE fix {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPNE;
    $$->valfix = $3;
    $$->value_type = QVALFIX;
  }
| TWORD TNE flo {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPNE;
    $$->valfix = $3;
    $$->value_type = QVALFLO;
  }

| TWORD TGE TSTR {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPGE;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD TGE TWORD {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPGE;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD TGE fix {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPGE;
    $$->valfix = $3;
    $$->value_type = QVALFIX;
  }
| TWORD TGE flo {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPGE;
    $$->valfix = $3;
    $$->value_type = QVALFLO;
  }

| TWORD TLE TSTR {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPLE;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD TLE TWORD {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPLE;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD TLE fix {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPLE;
    $$->valfix = $3;
    $$->value_type = QVALFIX;
  }
| TWORD TLE flo {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPLE;
    $$->valfix = $3;
    $$->value_type = QVALFLO;
  }

| TWORD '<' TSTR {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPLT;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD '<' TWORD {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPLT;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD '<' fix {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPLT;
    $$->valfix = $3;
    $$->value_type = QVALFIX;
  }
| TWORD '<' flo {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPLT;
    $$->valfix = $3;
    $$->value_type = QVALFLO;
  }

| TWORD '>' TSTR {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPGT;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD '>' TWORD {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPGT;
    $$->valstr = $3;
    $$->value_type = QVALSTR;
  }
| TWORD '>' fix {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPGT;
    $$->valfix = $3;
    $$->value_type = QVALFIX;
  }
| TWORD '>' flo {
    $$ = mkqnode(QKEY);
    $$->name = $1;
    $$->keytype = QKEYNAME;
    $$->operator = QOPGT;
    $$->valfix = $3;
    $$->value_type = QVALFLO;
  }

fix:
  '-' TFIX { $$ = -1 * $2; }
| TFIX { $$ = $1; }

flo:
  '-' TFLO { $$ = (-1.0) * $2; }
| TFLO { $$ = $1; }

%%
