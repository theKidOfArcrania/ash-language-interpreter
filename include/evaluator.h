#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "token.h"
#include "parsenode.h"

#include "arithmeticeval.h"
#include "assignmenteval.h"
#include "bitlogicaleval.h"
#include "comparisoneval.h"
#include "groupeval.h"
#include "literaleval.h"
#include "unaryeval.h"
#include "memberaccesseval.h"
#include "casteval.h"
#include "functioneval.h"
#include "arrayeval.h"

//recursively evaluate AST, start at root
ParseData evaluate(AbstractExpressionNode* node);

#endif
