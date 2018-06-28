#include <iostream>
#include <cstdint>
#include <vector>
#include <cstring>
#include "exceptions.h"
#include "token.h"
#include "parsetoken.h"
#include "parsenode.h"
#include "typehandler.h"
#include "statementnode.h"
#include "parser.h"
#include "function.h"

using namespace std;

static uint32_t tokenIndex;
static vector<Token>* tokens;
static SymbolTable* symbolTable;
static vector<char*>* codeLines;

/////////////////////////////////
//////    Access Tokens     /////
/////////////////////////////////

//return current Token and advance
Token* consume() {
  return (Token*) &(tokens->at(tokenIndex++));
}

//examine the current Token, don't advance position
Token* peek() {
  return (Token*) &(tokens->at(tokenIndex));
}

//examine a Token at given index
Token* peekAhead(uint32_t offset) {
  return (Token*) &(tokens->at(tokenIndex+offset));
}

char* getCodeLineBlock(uint32_t startIndex, uint32_t endIndex) {

  //append all lines to a string object
  string str;
  for(uint32_t i = startIndex; i <= endIndex; i++) {
    str.append("\t");
    str.append(codeLines->at(i));
  }
  
  //copy into a char array
  const char* c = str.c_str();
  uint32_t len = strlen(c);
  char* res = new char[len+1];
  res[0] = '\0';
  strcpy(res, c);
  res[len] = '\0';
  
  return res;
}

/////////////////////////////////
//////    Subroutines     ///////
/////////////////////////////////

// variable, number, string, etc.
AbstractExpressionNode* evalLiteralGroup() {

  AbstractExpressionNode* next;
 
  //if grouped expression
  if(peek()->type == LEFT_PAREN) {

    Token* startGroupToken = consume(); //consume left parenthesis
    Token* endGroupToken;
    
    next = evalExpression();

    if(peek()->type != RIGHT_PAREN) {
      //!!! parse syntax error
      throw ParseSyntaxException(startGroupToken->line+1, codeLines->at(startGroupToken->line), "(", "The parenthesized expression is missing a ')'");
    } else {
      endGroupToken = consume(); //consume right parenthesis
    }

    return new GroupedExpressionNode(next, startGroupToken->line, endGroupToken->line);

  } else if(peek()->type == VARIABLE) {
  
    Token* variable = consume();

    //make sure variable is already declared
    if(!symbolTable->isDeclared((char*) variable->lexeme)) {
      throw StaticVariableScopeException(variable->line, variable->lexeme, getCodeLineBlock(variable->line, variable->line), false);
    }

    const char* name = variable->lexeme;
    uint32_t len = strlen(name);
    char* variableName = new char[len+1];
    strcpy(variableName, name);
    variableName[len] = '\0';
    
    return new VariableNode(variableName, symbolTable, variable->line);
    
  } else {

    Token* literal = consume();
    ParseData p = {typeTokenConversion(literal->type), literal->value};
    return new LiteralNode(p, literal->line);
  }
}

//member access
bool isPostfixStructure() {
  return (peek()->type == ADD && peekAhead(1)->type == ADD) || (peek()->type == SUBTRACT && peekAhead(1)->type == SUBTRACT);
}

AbstractExpressionNode* handleMemberAccess(AbstractExpressionNode* head) {

  if(head->evalType != STRING_T) {
      
    //first write message
    const char* arrayAccessErrorMessage = "A value must be of array or string type to be indexed";
    throw StaticTypeException(head->startLine+1, peek()->line+1, getCodeLineBlock(head->startLine, peek()->line), arrayAccessErrorMessage);
  }
  
  Token* leftBracketToken = consume(); //consume [

  //if next value is colon, this is a slice with implicit start = 0
  if(peek()->type == COLON) {

    Token* colonToken = consume(); //consume :
    
    //start = 0
    ParseData startIndex;
    startIndex.type = INT32_T;
    startIndex.value.integer = 0;

    //if next value is ], implicit end = -1
    if(peek()->type == RIGHT_BRACKET) {

      Token* rightBracketToken = consume(); //consume ]
      ParseData endIndex;
      endIndex.type = INT32_T;
      endIndex.value.integer = (int32_t) -1;
      
      return new ArrayAccessNode(head, new LiteralNode(startIndex, leftBracketToken->line), 
                                  new LiteralNode(endIndex, rightBracketToken->line), rightBracketToken->line);

    } else {

      //defined end index
      AbstractExpressionNode* end = evalExpression();
      
      //make sure that the expression evaluates to an integer
      if(!typecheckMemberAccessExpression(end->evalType)) {
        //!!!
        throw StaticTypeException(head->startLine+1, end->endLine+1, getCodeLineBlock(head->startLine, end->endLine), "array indexing", end->evalType);
      }

      if(peek()->type != RIGHT_BRACKET) {
        
        //!!! If [ does not have a corresponding ], throw an error
        throw ParseSyntaxException(leftBracketToken->line+1, codeLines->at(leftBracketToken->line), "[", "Array access operation must end with ']'");
        
      } else {
  
        Token* rightBracketToken = consume(); //consume ]
        return new ArrayAccessNode(head, new LiteralNode(startIndex, leftBracketToken->line), end, rightBracketToken->line);
      }
    }

  } else {

    //defined start index
    AbstractExpressionNode* start = evalExpression();
    
    //make sure that the expression evaluates to an integer
    if(!typecheckMemberAccessExpression(start->evalType)) {
      //!!!
      throw StaticTypeException(head->startLine+1, start->endLine+1, getCodeLineBlock(head->startLine, start->endLine), "array indexing", start->evalType);
    }

    //accessing a slice with explicit start
    if(peek()->type == COLON) {
  
      consume(); //consume :

      //end = -1 implied
      if(peek()->type == RIGHT_BRACKET) {

        Token* rightBracketToken = consume(); //consume ]
        ParseData endIndex;
        endIndex.type = INT32_T;
        endIndex.value.integer = (int32_t) -1;
    
        return new ArrayAccessNode(head, start, new LiteralNode(endIndex, rightBracketToken->line), rightBracketToken->line);

      } else {

        //defined end index
        AbstractExpressionNode* end = evalExpression();
        
        //make sure that the expression evaluates to an integer
        if(!typecheckMemberAccessExpression(end->evalType)) {
          //!!!
          throw StaticTypeException(head->startLine+1, end->endLine+1, getCodeLineBlock(head->startLine, end->endLine), "array indexing", end->evalType);            
        }
          
        if(peek()->type != RIGHT_BRACKET) {
          
          //!!! If [ does not have a corresponding ], throw an error
          throw ParseSyntaxException(leftBracketToken->line+1, codeLines->at(leftBracketToken->line), "[", "Array access operation must end with ']'");

        } else {
          Token* rightBracketToken = consume(); //consume ]
          return new ArrayAccessNode(head, start, end, rightBracketToken->line);
        }
      }

    } else {
      
      //accessing single value
      if(peek()->type != RIGHT_BRACKET) {
        
        //!!! If [ does not have a corresponding ], throw an error
        throw ParseSyntaxException(leftBracketToken->line+1, codeLines->at(leftBracketToken->line), "[", "Array access operation must end with ']'");

      } else {
        Token* rightBracketToken = consume(); //consume ]
        return new ArrayAccessNode(head, start, rightBracketToken->line);
      }
    }
  }

  return NULL;
}

AbstractExpressionNode* evalPostfixMemberAccess() {
  
  AbstractExpressionNode* head = evalLiteralGroup();
  VariableNode* varNode = dynamic_cast<VariableNode*>(head);

  if(varNode && isPostfixStructure()) {

    //make sure variable is declared
    if(!symbolTable->isDeclared(varNode->variable)) {
      throw StaticVariableScopeException(varNode->startLine+1, varNode->variable, getCodeLineBlock(varNode->startLine, varNode->startLine), false);   
    }

    //get variable type
    ParseDataType varType = varNode->evalType;

    consume(); //consume first +
    Token* secondPlus = consume();

    if(secondPlus->type == ADD) {
      //make sure type is correct
      if(!typecheckUnaryExpression(POSTFIX_INC_OP, varType)) {
        throw StaticTypeException(varNode->startLine+1, secondPlus->line+1, getCodeLineBlock(varNode->startLine, secondPlus->line), "postfix increment", varType);
      }
      return new UnaryOperatorNode(POSTFIX_INC_OP, varNode, symbolTable, secondPlus->line);

    } else {
      //make sure type is correct
      if(!typecheckUnaryExpression(POSTFIX_DEC_OP, varType)) {
        throw StaticTypeException(varNode->startLine+1, secondPlus->line+1, getCodeLineBlock(varNode->startLine, secondPlus->line), "postfix decrement", varType);
      }
      return new UnaryOperatorNode(POSTFIX_DEC_OP, varNode, symbolTable, secondPlus->line); 
    }
  }

  while(peek()->type == LEFT_BRACKET) {
    head = handleMemberAccess(head);
  }

  return head;
}


bool isCastStructure() {
  return peek()->type == LEFT_PAREN && isTypeTokenType(peekAhead(1)->type) && peekAhead(2)->type == RIGHT_PAREN;
}

bool isPrefixStructure() {
  return (peek()->type == ADD && peekAhead(1)->type == ADD) || (peek()->type == SUBTRACT && peekAhead(1)->type == SUBTRACT);
}

//sign and bit/logical NOT
AbstractExpressionNode* evalPrefixCastSignNot() {
 
  //check if prefix
  if(isPrefixStructure()) {

    Token* firstPlus = consume();
    consume(); //consume second +

    AbstractExpressionNode* head = evalPostfixMemberAccess();
    VariableNode* varNode = dynamic_cast<VariableNode*>(head);

    //if it's not a variable, throw a syntax error
    if(!varNode) {
      throw ParseSyntaxException(firstPlus->line+1, head->endLine+1, getCodeLineBlock(firstPlus->line, head->endLine), "Prefix operator expects variable argument");
    }

    ParseDataType type = varNode->evalType;
    ParseOperatorType op = (firstPlus->type == ADD) ? PREFIX_INC_OP : PREFIX_DEC_OP;

    //if it's not a number, throw a type error
    if(!typecheckUnaryExpression(op, type)) {
      throw StaticTypeException(firstPlus->line+1, head->endLine+1, getCodeLineBlock(firstPlus->line, head->endLine), (op == PREFIX_INC_OP) ? "prefix increment" : "prefix decrement", type);
    }

    return new UnaryOperatorNode(op, varNode, symbolTable, firstPlus->line);

  } else if(isSignNotTokenType(peek()->type)) {

    Token* signNotToken = consume();
    ParseOperatorType op = unaryTokenConversion(signNotToken->type);
    AbstractExpressionNode* next = evalPrefixCastSignNot();

    if(typecheckUnaryExpression(op, next->evalType)) {
      return new UnaryOperatorNode(op, next, signNotToken->line);
    } else {
      //!!!
      throw StaticTypeException(signNotToken->line+1, next->endLine+1, getCodeLineBlock(signNotToken->line, next->endLine), toWordParseOperatorType(op), next->evalType);
    }

  } else if(isCastStructure()) {   
    
    Token* leftParenToken = consume(); //consume (
    ParseDataType finalType = typeTokenConversion(consume()->type);
    consume(); //consume )
    
    AbstractExpressionNode* next = evalPrefixCastSignNot();
    if(typecheckExplicitCastExpression(next->evalType, finalType)) {
      return new CastNode(next, finalType, leftParenToken->line);
    } else {
      throw StaticCastException(leftParenToken->line+1, next->endLine+1, getCodeLineBlock(leftParenToken->line, next->endLine), next->evalType, finalType, true);
    }
    
  } else {

    return evalPostfixMemberAccess();  
  }
}


// **
AbstractExpressionNode* evalExponent() {

  AbstractExpressionNode* head = evalPrefixCastSignNot();
  AbstractExpressionNode* next;

  while(peek()->type == EXPONENT) {
    consume(); 
    next = evalPrefixCastSignNot();

    if(typecheckArithmeticExpression(EXPONENT_OP, head->evalType, next->evalType)) {
      head = new ArithmeticOperatorNode(EXPONENT_OP, head, next);
    } else {
      //!!!
      throw StaticTypeException(head->startLine+1, next->endLine+1, getCodeLineBlock(head->startLine, next->endLine), "exponentiation", head->evalType, next->evalType);
    }   

  }
  
  return head;
}


// * / %
AbstractExpressionNode* evalMultiplyDivideMod() {

  AbstractExpressionNode* head = evalExponent();
  AbstractExpressionNode* next;

  while(isMultiplyDivideModTokenType(peek()->type)) {
    ParseOperatorType op = binaryTokenConversion(consume()->type);
    next = evalExponent();

    if(typecheckArithmeticExpression(op, head->evalType, next->evalType)) {
      head = new ArithmeticOperatorNode(op, head, next);
    } else {
      //!!!
      throw StaticTypeException(head->startLine+1, next->endLine+1, getCodeLineBlock(head->startLine, next->endLine), toWordParseOperatorType(op), head->evalType, next->evalType);
    }

  }

  return head;
}


// + -
AbstractExpressionNode* evalAddSubtract() {

  AbstractExpressionNode* head = evalMultiplyDivideMod();
  AbstractExpressionNode* next;

  while(isAddSubtractTokenType(peek()->type)) {
    ParseOperatorType op = binaryTokenConversion(consume()->type);
    next = evalMultiplyDivideMod();

    if(typecheckArithmeticExpression(op, head->evalType, next->evalType)) {
      head = new ArithmeticOperatorNode(op, head, next);
    } else {
      throw StaticTypeException(head->startLine+1, next->endLine+1, getCodeLineBlock(head->startLine, next->endLine), toWordParseOperatorType(op), head->evalType, next->evalType);
    }
  }
  
  return head;
}

// << >>
AbstractExpressionNode* evalBitShift() {

  AbstractExpressionNode* head = evalAddSubtract();
  AbstractExpressionNode* next;

  while(isBitShiftTokenType(peek()->type)) {
    ParseOperatorType op = binaryTokenConversion(consume()->type);
    next = evalAddSubtract();

    if(typecheckBitLogicalExpression(op, head->evalType, next->evalType)) {
      head = new BitLogicalOperatorNode(op, head, next);
    } else {
      throw StaticTypeException(head->startLine+1, next->endLine+1, getCodeLineBlock(head->startLine, next->endLine), toWordParseOperatorType(op), head->evalType, next->evalType);
    }
  }
  
  return head;
}


// < <= > >=
AbstractExpressionNode* evalComparison() {

  AbstractExpressionNode* head = evalBitShift();
  AbstractExpressionNode* next;

  while(isInequalityTokenType(peek()->type)) {
    ParseOperatorType op = binaryTokenConversion(consume()->type);
    next = evalBitShift();

    if(typecheckComparisonExpression(op, head->evalType, next->evalType)) {
      head = new ComparisonOperatorNode(op, head, next);
    } else {
      throw StaticTypeException(head->startLine+1, next->endLine+1, getCodeLineBlock(head->startLine, next->endLine), toWordParseOperatorType(op), head->evalType, next->evalType); 
    }
  }
  
  return head;
}

// == and !=
AbstractExpressionNode* evalEquality() {

  AbstractExpressionNode* head = evalComparison();
  AbstractExpressionNode* next;

  while(isEqualityTokenType(peek()->type)) {
    ParseOperatorType op = binaryTokenConversion(consume()->type);
    next = evalComparison();

    if(typecheckComparisonExpression(op, head->evalType, next->evalType)) {
      head = new ComparisonOperatorNode(op, head, next);
    } else {
      throw StaticTypeException(head->startLine+1, next->endLine+1, getCodeLineBlock(head->startLine, next->endLine), toWordParseOperatorType(op), head->evalType, next->evalType);  
    }
  }
 
  return head;
}

// &
AbstractExpressionNode* evalBitAnd() {

  AbstractExpressionNode* head = evalEquality();
  AbstractExpressionNode* next;

  while(peek()->type == BIT_AND) {
    consume();
    next = evalEquality();

    if(typecheckBitLogicalExpression(BIT_AND_OP, head->evalType, next->evalType)) {
      head = new BitLogicalOperatorNode(BIT_AND_OP, head, next);
    } else {
      throw StaticTypeException(head->startLine+1, next->endLine+1, getCodeLineBlock(head->startLine, next->endLine), "bitwise AND", head->evalType, next->evalType); 
    }
  }
 
  return head;
}


// ^
AbstractExpressionNode* evalBitXor() {

  AbstractExpressionNode* head = evalBitAnd();
  AbstractExpressionNode* next;

  while(peek()->type == BIT_XOR) {
    consume();
    next = evalBitAnd();

    if(typecheckBitLogicalExpression(BIT_XOR_OP, head->evalType, next->evalType)) {
      head = new BitLogicalOperatorNode(BIT_XOR_OP, head, next);
    } else {
      throw StaticTypeException(head->startLine+1, next->endLine+1, getCodeLineBlock(head->startLine, next->endLine), "bitwise XOR", head->evalType, next->evalType); 
    }
  }
 
  return head;
}

// |
AbstractExpressionNode* evalBitOr() {

  AbstractExpressionNode* head = evalBitXor();
  AbstractExpressionNode* next;

  while(peek()->type == BIT_OR) {
    consume();
    next = evalBitXor();

    if(typecheckBitLogicalExpression(BIT_OR_OP, head->evalType, next->evalType)) {
      head = new BitLogicalOperatorNode(BIT_OR_OP, head, next);
    } else {
      throw StaticTypeException(head->startLine+1, next->endLine+1, getCodeLineBlock(head->startLine, next->endLine), "bitwise OR", head->evalType, next->evalType);
    }
  }
 
  return head;
}


// &&
AbstractExpressionNode* evalLogicAnd() {

  AbstractExpressionNode* head = evalBitOr();
  AbstractExpressionNode* next;

  while(peek()->type == AND) {
    consume();
    next = evalBitOr();

    if(typecheckBitLogicalExpression(AND_OP, head->evalType, next->evalType)) {
      head = new BitLogicalOperatorNode(AND_OP, head, next);
    } else {
      throw StaticTypeException(head->startLine+1, next->endLine+1, getCodeLineBlock(head->startLine, next->endLine), "logical AND", head->evalType, next->evalType);  
    }
  }
 
  return head;
}


// ^^
AbstractExpressionNode* evalLogicXor() {

  AbstractExpressionNode* head = evalLogicAnd();
  AbstractExpressionNode* next;

  while(peek()->type == XOR) {
    consume();
    next = evalLogicAnd();

    if(typecheckBitLogicalExpression(XOR_OP, head->evalType, next->evalType)) {
      head = new BitLogicalOperatorNode(XOR_OP, head, next);
    } else {
      throw StaticTypeException(head->startLine+1, next->endLine+1, getCodeLineBlock(head->startLine, next->endLine), "logical XOR", head->evalType, next->evalType); 
    }
  }
 
  return head;
}

// ||
AbstractExpressionNode* evalLogicOr() {

  AbstractExpressionNode* head = evalLogicXor();
  AbstractExpressionNode* next;

  while(peek()->type == OR) {
    consume();
    next = evalLogicXor();

    if(typecheckBitLogicalExpression(OR_OP, head->evalType, next->evalType)) {
      head = new BitLogicalOperatorNode(OR_OP, head, next);
    } else {
      throw StaticTypeException(head->startLine+1, next->endLine+1, getCodeLineBlock(head->startLine, next->endLine), "logical OR", head->evalType, next->evalType);  
    }
  }
 
  return head;
}


//  = += -= *= **= /= &= ^= |= <<= >>=
AbstractExpressionNode* evalAssignment() {
  
  AbstractExpressionNode* head = evalLogicOr();
  VariableNode* var = dynamic_cast<VariableNode*>(head);
  
  if(var && isAssignmentOperatorTokenType(peek()->type)) {
  
    //make sure variable is already declared in some scope
    if(!symbolTable->isDeclared(var->variable)) {
      throw StaticVariableScopeException(var->startLine, var->variable, getCodeLineBlock(var->startLine, var->startLine), false);
    }
    
    //get variable's type
    ParseDataType type = var->evalType;
    
    Token* assignmentToken = consume();
    AbstractExpressionNode* next = evalAssignment();
    
    switch(assignmentToken->type) {
      case EQ: break;
      case ADD_EQ: next = new ArithmeticOperatorNode(ADD_OP, var, next); break; 
      case SUBTRACT_EQ: next = new ArithmeticOperatorNode(SUBTRACT_OP, var, next); break;
      case EXPONENT_EQ: next = new ArithmeticOperatorNode(EXPONENT_OP, var, next); break;
      case MULTIPLY_EQ: next = new ArithmeticOperatorNode(MULTIPLY_OP, var, next); break;
      case DIVIDE_EQ: next = new ArithmeticOperatorNode(DIVIDE_OP, var, next); break;
      case AND_EQ: {
        if(type == BOOL_T)
          next = new BitLogicalOperatorNode(AND_OP, var, next);
        else 
          next = new BitLogicalOperatorNode(BIT_AND_OP, var, next);
        break;
      }
      
      case XOR_EQ: {
        if(type == BOOL_T)
          next = new BitLogicalOperatorNode(XOR_OP, var, next);
        else 
          next = new BitLogicalOperatorNode(BIT_XOR_OP, var, next);
        break;
      }
      
      case OR_EQ: {
        if(type == BOOL_T)
          next = new BitLogicalOperatorNode(OR_OP, var, next);
        else 
          next = new BitLogicalOperatorNode(BIT_OR_OP, var, next);
        break;
      }
    }
    
    //now make sure that implicit type cast is valid
    if(!typecheckImplicitCastExpression(next->evalType, type)) {
      //!!!
      throw StaticCastException(var->startLine+1, next->endLine+1, getCodeLineBlock(var->startLine, next->endLine), next->evalType, type, false);
    }
    
    return new AssignmentExpressionNode(var->variable, type, next, symbolTable, var->startLine);
    
  } else {
    return head;
  }
  
}

AbstractExpressionNode* evalExpression() {
  return evalAssignment();
}


AbstractStatementNode* addStatement() {

  Token* t = peek();
  
  //first check for declaration and assignment
  if(isTypeTokenType(t->type)) {
   
    //variable declaration
    ParseDataType type = typeTokenConversion(t->type);
    consume(); //consume type Token    
    
    //get variable name
    Token* variableToken = consume();

    //make sure variable is not already declared
    if(symbolTable->isDeclaredInScope((char*) variableToken->lexeme)) {
      throw StaticVariableScopeException(variableToken->line, variableToken->lexeme, getCodeLineBlock(variableToken->line, variableToken->line), true); 
    }
    
    const char* constVariable = variableToken->lexeme;
    uint32_t len = strlen(constVariable);
    char* variable = new char[len+1];
    strcpy(variable, constVariable);
    variable[len] = '\0';
    
    //if being assigned
    if(peek()->type == EQ) {
      
      consume(); //consume = Token
      AbstractExpressionNode* expression = evalExpression();
      
      //check implicit casting validity
      if(!typecheckImplicitCastExpression(expression->evalType, type)) {
        //!!!
        throw StaticCastException(variableToken->line+1, expression->endLine+1, getCodeLineBlock(variableToken->line, expression->endLine), expression->evalType, type, false); 
      } else {
        AbstractStatementNode* troll =  new NewAssignmentStatementNode(variable, type, expression, symbolTable);
        return troll;
      }
      
    } else {  
      //no initial value
      return new NewAssignmentStatementNode(variable, type, symbolTable);
    }
    
  } else if(t->type == VARIABLE) {
    
    //existing variable assignment
    
    //get variable name
    Token* varToken = consume();
    const char* constVariable = varToken->lexeme;
    uint32_t len = strlen(constVariable);
    char* variable = new char[len+1];
    strcpy(variable, constVariable);
    variable[len] = '\0';
    
    //make sure variable is already declared in some scope
    if(!symbolTable->isDeclared(variable)) {
      throw StaticVariableScopeException(varToken->line, varToken->lexeme, getCodeLineBlock(varToken->line, varToken->line), false);
    }
    
    //get variable type from the symbol table
    ParseDataType type = (symbolTable->get(variable)).type;
    
    Token* assignmentToken;
    if(!isAssignmentOperatorTokenType(peek()->type)) {
      std::cout << "'=' must follow variable name when assigning value" << std::endl;
      return NULL;
    } else {
      assignmentToken = consume(); //consume = 
    }
    
    AbstractExpressionNode* expression = evalExpression();
    switch(assignmentToken->type) {
      case EQ: break;
      case ADD_EQ: expression = new ArithmeticOperatorNode(ADD_OP, new VariableNode(variable, symbolTable, varToken->line), expression); break; 
      case SUBTRACT_EQ: expression = new ArithmeticOperatorNode(SUBTRACT_OP, new VariableNode(variable, symbolTable, varToken->line), expression); break;
      case EXPONENT_EQ: expression = new ArithmeticOperatorNode(EXPONENT_OP, new VariableNode(variable, symbolTable, varToken->line), expression); break;
      case MULTIPLY_EQ: expression = new ArithmeticOperatorNode(MULTIPLY_OP, new VariableNode(variable, symbolTable, varToken->line), expression); break;
      case DIVIDE_EQ: expression = new ArithmeticOperatorNode(DIVIDE_OP, new VariableNode(variable, symbolTable, varToken->line), expression); break;
      case AND_EQ: {
        if(type == BOOL_T)
          expression = new BitLogicalOperatorNode(AND_OP, new VariableNode(variable, symbolTable, varToken->line), expression);
        else 
          expression = new BitLogicalOperatorNode(BIT_AND_OP, new VariableNode(variable, symbolTable, varToken->line), expression);
        break;
      }
      
      case XOR_EQ: {
        if(type == BOOL_T)
          expression = new BitLogicalOperatorNode(XOR_OP, new VariableNode(variable, symbolTable, varToken->line), expression);
        else 
          expression = new BitLogicalOperatorNode(BIT_XOR_OP, new VariableNode(variable, symbolTable, varToken->line), expression);
        break;
      }
      
      case OR_EQ: {
        if(type == BOOL_T)
          expression = new BitLogicalOperatorNode(OR_OP, new VariableNode(variable, symbolTable, varToken->line), expression);
        else 
          expression = new BitLogicalOperatorNode(BIT_OR_OP, new VariableNode(variable, symbolTable, varToken->line), expression);
        break;
      }
    }
    
    //make sure implicit cast is valid
    if(!typecheckImplicitCastExpression(expression->evalType, type)) {
      throw StaticCastException(varToken->line+1, expression->endLine+1, getCodeLineBlock(varToken->line, expression->endLine), expression->evalType, type, false);
    } else {
      return new AssignmentStatementNode(variable, expression, symbolTable);
    }
  }
	
  switch(t->type) {
    
    case PRINTLN: {
      consume();
      return new PrintLineStatementNode(evalExpression(), symbolTable);
    }
    
    case PRINT: {  
      consume();
      return new PrintStatementNode(evalExpression(), symbolTable);
    }
    
    case LEFT_BRACE: {
      consume();
      vector<AbstractStatementNode*>* statements = new vector<AbstractStatementNode*>();
      
      //enter a new scope in symbol table (for static scope-checking)
      symbolTable->enterNewScope();
      
      while(peek()->type != RIGHT_BRACE) {
        statements->push_back(addStatement());  
      }
      
      consume(); //consume right brace
      
      //leave scope
      symbolTable->leaveScope();
      
      return new GroupedStatementNode(statements, symbolTable);
    }
    
    case WHILE: {
    
      consume(); //consume while Token
      AbstractExpressionNode* condition = evalExpression();
      
      if(condition->evalType != BOOL_T) {
        std::cout << "ERROR: expression must evaluate to a bool" << std::endl;
        return NULL;
      }
      
      AbstractStatementNode* body = addStatement();
      
      return new WhileStatementNode(condition, body, symbolTable);
    }
    
    case FOR: {
      
      consume(); //consume for Token
      
      if(peek()->type != LEFT_PAREN) {
        std::cout << "ERROR: ( must follow 'for'" << std::endl;
        return NULL;
      } else {
        consume();
      }
      
      AbstractStatementNode* initialization = addStatement();
      
      if(peek()->type != SEMICOLON) {
        std::cout << "ERROR: ; expected in 'for' construct" << std::endl;
        return NULL;
      } else {
        consume();
      }
      
      AbstractExpressionNode* condition = evalExpression();
      
      if(condition->evalType != BOOL_T) {
        std::cout << "ERROR: expression must evaluate to a bool" << std::endl;
        return NULL;
      }
      
      if(peek()->type != SEMICOLON) {
        std::cout << "ERROR: ; expected in 'for' construct" << std::endl;
        return NULL;
      } else {
        consume();
      }
      
      AbstractStatementNode* update = addStatement();
      
      if(peek()->type != RIGHT_PAREN) {
        std::cout << "ERROR: ) must follow here in 'for' construct" << std::endl;
        return NULL;
      } else {
        consume();
      }
      
      AbstractStatementNode* body = addStatement();
      return new ForStatementNode(initialization, update, body, condition, symbolTable);
    }
    
    case IF: {
    
      vector<AbstractExpressionNode*>* cond = new vector<AbstractExpressionNode*>();
      vector<AbstractStatementNode*>* stat = new vector<AbstractStatementNode*>();
      
      //consume the IF token and add first condition-statement pair
      consume();
      AbstractExpressionNode* e = evalExpression();
      
      if(e->evalType != BOOL_T) {
        cout << "Conditional must take a boolean expression" << endl;
        return NULL;
      } else {
        cond->push_back(e);
        stat->push_back(addStatement());
      }
      
      //now append to the chain as needed
      while(peek()->type == ELIF || peek()->type == ELSE) {
        
        if(peek()->type == ELIF) {
          
          //consume ELIF token and add condition-statement pair
          consume();
          AbstractExpressionNode* exp = evalExpression();
          
          if(exp->evalType != BOOL_T) {
            cout << "Conditional must take a boolean expression" << endl;
            return NULL;
          } else {
            cond->push_back(exp);
            stat->push_back(addStatement());
          }
           
        } else {
          
          //consume ELSE token and add dummy true-statement pair
          Token* elseToken = consume();
          
          ParseData d;
          d.type = BOOL_T; d.value.integer = true;
          LiteralNode* exp = new LiteralNode(d, elseToken->line);
          
          cond->push_back(exp);
          stat->push_back(addStatement());
          
          return new ConditionalStatementNode(cond, stat, symbolTable);
        }  
      }
      
      return new ConditionalStatementNode(cond, stat, symbolTable);
    }

		//TODO error handling
		case FUN: {

			consume(); //consume the FUN token

			//next token should be a variable, the function name
			if(peek()->type != VARIABLE) {
				cout << "ERROR: expected function name after 'fun' keyword" << endl;
				return NULL;
			}

			Token* varToken = consume(); //function name
			const char* funName = varToken->lexeme;
			uint32_t len = strlen(funName);
			char* functionName = new char[len+1];
			functionName[0] = '\0';
			strcpy(functionName, funName);
			functionName[len] = '\0';

			//now create a Function struct
			Function function;

			//now read in arguments enclosed in parentheses
			if(peek()->type != LEFT_PAREN) {
				cout << "ERROR: expected parentheses containing function arguments" << endl;
				return NULL;
			}
			
			consume(); //consume (
			
			//first scan to figure out how many arguments must be allocated in Function
			uint32_t currentIndex = 0;
			uint32_t argCount = 0;

			while(peekAhead(currentIndex)->type != RIGHT_PAREN) {

				//make sure you have a type then variable
				if(!isTypeTokenType(peekAhead(currentIndex)->type)) {
					cout << "ERROR: expected argument type" << endl;
					return NULL;	
				}
				currentIndex++;

				if(peekAhead(currentIndex)->type != VARIABLE) {
					cout << "ERROR: expected argument variable name" << endl;
					return NULL;
				}
				currentIndex++;

				argCount++;

				//if next token is not closing parenthesis, it must be a comma
				TokenType nextToken = peekAhead(currentIndex)->type;
				if(nextToken != RIGHT_PAREN && nextToken != COMMA) {
					cout << "ERROR: expected comma separating arguments or closing parenthesis" << endl;
					return NULL;
				} else if(nextToken == COMMA) {
					currentIndex++; //"consume" COMMA token
				}
			}

			//now allocate memory for argument names and types
			function.numArgs = argCount;
			ParseDataType* argTypes = new ParseDataType[argCount+1];
			char** argNames = (char**) malloc(sizeof(char*) * argCount);

			//now read in arguments for real
			uint32_t argIndex = 0;
			while(peek()->type != RIGHT_PAREN) {

				argTypes[argIndex] = typeTokenConversion(consume()->type);

				const char* aName = consume()->lexeme;
				uint32_t len = strlen(aName);
				char* argName = new char[len+1];
				argName[0] = '\0';
				strcpy(argName, aName);
				argName[len] = '\0';
				argNames[argIndex] = argName;

				argIndex++;
				if(argIndex < argCount)
					consume(); //consume ,
			}
			consume(); //consume ')' token

			function.argTypes = argTypes;
			function.argNames = argNames;

			//now get the return type
			if(peek()->type != RIGHTARROW) {
				cout << "ERROR: expected -> followed by a return type" << endl;
				return NULL;
			}
			consume(); //consume ->

			function.returnType = typeTokenConversion(consume()->type);

			//now create GroupedExpressionNode
			if(peek()->type != LEFT_BRACE) {
				cout << "ERROR: expected braced function body" << endl;
				return NULL;
			}
			consume(); //consume {

      vector<AbstractStatementNode*>* body = new vector<AbstractStatementNode*>();
      
      //enter a new scope in symbol table (for static scope-checking)
      symbolTable->enterNewScope();
      
			//add statements declaring the parameter variables
			for(uint32_t i = 0; i < argCount; i++) {
				body->push_back(new NewAssignmentStatementNode(argNames[i], argTypes[i], symbolTable));
			}

      while(peek()->type != RIGHT_BRACE) {
        body->push_back(addStatement());  
      }
      
      consume(); //consume right brace
      
      //leave scope
      symbolTable->leaveScope();
			
			//create function node

		}
    
    default: return new ExpressionStatementNode(evalExpression(), symbolTable);
    
  }
  
}

//generate Abstract Syntax Tree from list of tokens
vector<AbstractStatementNode*>* parse(vector<Token>* tokenRef, vector<char*>* sourceCodeLines) {

  codeLines = sourceCodeLines;

  //set static variables to correct initial values
  tokenIndex = 0;
  tokens = tokenRef;
  symbolTable = new SymbolTable();
  
  //create empty statement vector
  vector<AbstractStatementNode*>* statements = new vector<AbstractStatementNode*>();
  
  //append statement nodes until END is reached
  while(peek()->type != END) {
    statements->push_back(addStatement());
  }
  
  return statements;
}


