#include <vector>
#include <cstdint>
#include "token.h"
#include "parsetoken.h"
#include "parsenode.h"
#include "statementnode.h"
#include "symboltable.h"

class SymbolTable;

/////////////////////////////////
//////     Return Tree     //////
/////////////////////////////////

std::vector<AbstractStatementNode*>* parse(std::vector<Token>* tokens, std::vector<char*>* codeLines);

/////////////////////////////////
//////  Return Statements  //////
/////////////////////////////////

void addStatement(std::vector<AbstractStatementNode*>* statements);

/////////////////////////////////
//////    Access Tokens     /////
/////////////////////////////////

//consume a Token in the list (increment index)
Token* consume();

//backtrack by 1 token in the list
void stepBack();

//examine the current Token, don't advance position
Token* peek();

//examine a Token at given index
Token* peekAhead(uint32_t offset);

char* getCodeLineBlock(uint32_t start, uint32_t end);


/////////////////////////////////
//////       Helpers        /////
/////////////////////////////////

bool isType(Token* potentialType, SymbolTable* symbolTable);

/////////////////////////////////
//////    Subroutines     ///////
/////////////////////////////////

// variable, number, string, etc.
AbstractExpressionNode* evalLiteralGroup();

// x++ x-- x() x->stuff x.stuff
AbstractExpressionNode* evalPostfixFunctionMembers();

// ++x --x + - ! ~
AbstractExpressionNode* evalPrefixNotCast();

// **
AbstractExpressionNode* evalExponent();

// * / %
AbstractExpressionNode* evalMultiplyDivideMod();

// + -
AbstractExpressionNode* evalAddSubtract();

// << >>
AbstractExpressionNode* evalBitShift();

// < <= > >=
AbstractExpressionNode* evalComparison();

// == and !=
AbstractExpressionNode* evalEquality();

// &
AbstractExpressionNode* evalBitAnd();

// ^
AbstractExpressionNode* evalBitXor();

// |
AbstractExpressionNode* evalBitOr();

// &&
AbstractExpressionNode* evalLogicAnd();

// ^^
AbstractExpressionNode* evalLogicXor();

// ||
AbstractExpressionNode* evalLogicOr();

// ? :
AbstractExpressionNode* evalTernary();

//  = += -= *= **= /= &= ^= |= <<= >>=
AbstractExpressionNode* evalAssignment();

AbstractExpressionNode* evalExpression();

