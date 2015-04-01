#ifndef QNODE_H_
#define QNODE_H_

#include <regex.h>

typedef enum qnode_type {
  QNEG,
  QAND,
  QOR,
  QKEY,
  QRXP,
  QIN
} qnode_type_t;

typedef enum qnode_op {
  QOPEQ,
  QOPNE,
  QOPLE,
  QOPGE,
  QOPLT,
  QOPGT
} qnode_op_t;

typedef enum qnode_keytype {
  QKEYNAME,
  QKEYPOS
} qnode_keytype_t;

typedef enum qnode_value {
  QVALFIX,
  QVALFLO,
  QVALSTR
} qnode_value_t;

typedef struct qnode {
  struct qnode *left;
  struct qnode *right;
  regex_t *regexp;

  /* Type of this query node */
  qnode_type_t type;

  /* Sought after value for QKEY or QIN queries */
  qnode_value_t value_type;
  union {
    char *valstr;
    long valfix;
    double valflo;
  };

  /* Key query type. */
  qnode_keytype_t keytype;
  union {
    int position;
    char *name;
  };

  /* type of operator for relevant queries */
  qnode_op_t operator;
} qnode_t;

qnode_t *mkqnode(qnode_type_t type);

#endif
