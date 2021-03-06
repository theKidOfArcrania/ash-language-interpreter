#include <iostream>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <vector>

#include "errors.h"
#include "token.h"
#include "lexer.h"

using namespace std;

static vector<char*>* codeLines;
static uint32_t line = 0;

//Convert string to uint64_t
uint64_t stringToInt(char* str, uint32_t start, uint32_t end) {

  uint64_t ans = 0;
  for(uint32_t i = start; i <= end; i++) {
    ans = ans*10 + (str[i] - '0');
  }  

  return ans;
}

//Convert string to double
double stringToDouble(char* str, uint32_t start, uint32_t end) {

  double whole = 0;
  double decimal = 0;
  double exp = 0.1;
  bool isDecimalMode = false;

  for(uint32_t i = start; i <= end; i++) {

    if(str[i] == '.') {
      isDecimalMode = true;
    } else if(isDecimalMode) {
      decimal += exp*(str[i]-'0');
      exp *= 0.1;      
    } else {
      whole = whole*10 + (str[i]-'0');      
    }
  }

  return whole + decimal;
}


//Lexes source code into array of Tokens
vector<Token> lex(char* code, vector<char*>* sourceCodeLines) {
  
  codeLines = sourceCodeLines;
  
  //store code length
  uint32_t codeLength = strlen(code);
 
  vector<Token> tokens;
  uint32_t index = 0;
  line = 0;  

  while(code[index] != 0) {

    //first parse fixed-size tokens
    switch(code[index]) {
  
      case '(': {
        tokens.push_back(makeToken(LEFT_PAREN, line, "("));
        index++; break;
      }  

      case ')': {
        tokens.push_back(makeToken(RIGHT_PAREN, line, ")"));
        index++; break;
      }

      case '[': {
        tokens.push_back(makeToken(LEFT_BRACKET, line, "["));
        index++; break;
      }  
  
      case ']': {
        tokens.push_back(makeToken(RIGHT_BRACKET, line,  "]"));
        index++; break;
      }  

      case '{': {
        tokens.push_back(makeToken(LEFT_BRACE, line, "{"));
        index++; break;
      }  

      case '}': {
        tokens.push_back(makeToken(RIGHT_BRACE, line, "}"));
        index++; break;
      }  

      case '.': {
        if(!isdigit(code[index+1])) {
          tokens.push_back(makeToken(PERIOD, line, "."));
          index++; 
        }
        break;
      }

      case '?': {
        tokens.push_back(makeToken(QUESTION, line, "?"));
        index++; break;
      }

      case ':': {
        tokens.push_back(makeToken(COLON, line, ":"));
        index++; break;
      }

      case ';': {
        tokens.push_back(makeToken(SEMICOLON, line, ";"));
        index++; break;
      }

      case ',': {
        tokens.push_back(makeToken(COMMA, line, ","));
        index++; break;
      }

			//single char literal
			case '\'': {

				uint32_t currentIndex = index+1;
				char charVal = code[currentIndex];
				char literalValue = 0;

				if(charVal == '\\') {

					//escaped sequences
					currentIndex++;
					charVal = code[currentIndex];

					switch(charVal) {
						case 'a': literalValue = '\a'; break;
						case 'b': literalValue = '\b'; break;
						case 'f': literalValue = '\f'; break;
						case 'n': literalValue = '\n'; break;
						case 'r': literalValue = '\r'; break;
						case 't': literalValue = '\t'; break;
						case 'v': literalValue = '\v'; break;
						case '\'': literalValue = '\''; break;
						case '\"': literalValue = '\"'; break;
						case '?': literalValue = '?'; break;
						case '\\': literalValue = '\\'; break;
						default: {
							char* lexeme = new char[2];
							lexeme[0] = charVal;
							lexeme[1] = 0;
							throw LexerError(line+1, codeLines->at(line), lexeme, "Invalid escaped character");
						}
					}

					currentIndex++;
					
					//make sure literal is terminated
					if(code[currentIndex] != '\'') {

						char* lexeme = new char[currentIndex-index+1];
						for(int32_t i = index; i < currentIndex; i++) {
              lexeme[i-index] = code[i];
            }
            lexeme[currentIndex-index] = '\0';

						throw LexerError(line+1, codeLines->at(line), lexeme, "Expected ' to terminate character literal");
					} else {
						//consume second '
						currentIndex++;
					}

				} else if(charVal == '\'') {

					literalValue = '\0';
					currentIndex++;

				} else {

					literalValue = charVal;
					currentIndex++;

					//make sure literal is terminated
					if(code[currentIndex] != '\'') {

						char* lexeme = new char[currentIndex-index+1];
						for(int32_t i = index; i < currentIndex; i++) {
              lexeme[i-index] = code[i];
            }
            lexeme[currentIndex-index] = '\0';
            
						throw LexerError(line+1, codeLines->at(line), lexeme, "Expected ' to terminate character literal");
					} else {
						//consume second '
						currentIndex++;
					}
				}

				//append to list of Tokens
				char lexeme[2] = {literalValue, 0};
				Data d; d.integer = (uint8_t) literalValue;
				tokens.push_back(makeToken(CHAR, line, lexeme, d));
				index = currentIndex;

				break;
			}

      //more complex fixed-length tokens
      case '+': {
  
        if(code[index+1]) {
          
          switch(code[index+1]) {
  
            // case '+': {
            //   tokens.push_back(makeToken(INC, line, (char*) "++"));
            //   index += 2; break;
            // }

            case '=': {
              tokens.push_back(makeToken(ADD_EQ, line, "+="));
              index += 2; break;
            }
    
            default: {
              tokens.push_back(makeToken(ADD, line, "+"));
              index++; break;
            }
          }

        } else {
          tokens.push_back(makeToken(ADD, line, "+"));
          index++;
        }

        break;
      }

      case '-': {
  
        if(code[index+1]) {
          
          switch(code[index+1]) {
  
            // case '-': {
            //   tokens.push_back(makeToken(DEC, line, (char*) "--"));
            //   index += 2; break;
            // }

						case '>': {
							tokens.push_back(makeToken(RIGHTARROW, line, "->"));
							index += 2; break;
						}

            case '=': {
              tokens.push_back(makeToken(SUBTRACT_EQ, line, "-="));
              index += 2; break;
            }
    
            default: {
              tokens.push_back(makeToken(SUBTRACT, line, "-"));
              index++; break;
            }
          }

        } else {
          tokens.push_back(makeToken(SUBTRACT, line, "-"));
          index++;
        }

        break;
      }

      case '*': {
  
        if(code[index+1] == '*') {
          
          if(code[index+2] == '=') {
            tokens.push_back(makeToken(EXPONENT_EQ, line, "**="));
            index+=3; 
          } else { 
            tokens.push_back(makeToken(EXPONENT, line, "**"));
            index+=2;
          }          

        } else if(code[index+1] == '=') {
          tokens.push_back(makeToken(MULTIPLY_EQ, line, "*="));
          index+=2;
        } else {
          tokens.push_back(makeToken(MULTIPLY, line, "*"));
          index++;
        }

        break;
      }

      case '/': {
 
        if(code[index+1] == '/') {

          //comment out code until end of line
          while(code[index] != '\n' && code[index] != 0) {
            index++;
          }

          if(code[index] != 0)
            index++;

        } else if(code[index+1] == '*') {
  
          uint32_t depth = 1;
          index += 2;
          
          while(depth > 0) {
            if(code[index] == '*' && code[index+1] == '/') {
              depth--;
              index += 2;
            } else if(code[index] == '/' && code[index+1] == '*') {
              depth++;
              index += 2;
            } else {
              index++;
            }
          }   
        
        } else if(code[index+1] == '=') {
          
          tokens.push_back(makeToken(DIVIDE_EQ, line, "/="));
          index += 2;

        } else {
          tokens.push_back(makeToken(DIVIDE, line, "/"));
          index++;
        }

        break;
      }

      case '%': {
  
        if(code[index+1] == '=') {

          tokens.push_back(makeToken(MOD_EQ, line, "%="));
          index += 2;

        } else {
          tokens.push_back(makeToken(MOD, line, "%"));
          index++;
        }
        
        break;
      }

      case '!': {
  
        if(code[index+1] == '=') {
  
          tokens.push_back(makeToken(NOT_EQ, line, "!="));
          index += 2;

        } else {
          tokens.push_back(makeToken(NOT, line, "!"));
          index++;
        }

        break;
      }
  
      case '~': {
        tokens.push_back(makeToken(BIT_NOT, line, "~"));
        index++; break;
      }

      case '<': {
  
        if(code[index+1] == '<') {
  
          if(code[index+2] == '=') {
            tokens.push_back(makeToken(BIT_LEFT_EQ, line, "<<="));
            index += 3;
          } else {
            tokens.push_back(makeToken(BIT_LEFT, line, "<<"));
            index += 2;
          }
         
        } else if(code[index+1] == '=') {
          tokens.push_back(makeToken(LESS_EQ, line, "<="));
          index += 2;

        } else {
          tokens.push_back(makeToken(LESS, line, "<"));
          index++;
        }

        break;
      }

      case '>': {
  
        if(code[index+1] == '>') {
  
          if(code[index+2] == '=') {
            tokens.push_back(makeToken(BIT_RIGHT_EQ, line, ">>="));
            index += 3;
          } else {
            tokens.push_back(makeToken(BIT_RIGHT, line, ">>"));
            index += 2;
          }
         
        } else if(code[index+1] == '=') {
          tokens.push_back(makeToken(GREATER_EQ, line, ">="));
          index += 2;
        } else {
          tokens.push_back(makeToken(GREATER, line, ">"));
          index++;
        }

        break;
      }

      case '=': {
        if(code[index+1] == '=') {
          tokens.push_back(makeToken(EQ_EQ, line, "=="));
          index += 2;
        } else {
          tokens.push_back(makeToken(EQ, line, "="));
          index++;
        }
        break;  
      }

      case '&': {
        if(code[index+1] == '&') {
          tokens.push_back(makeToken(AND, line, "&&"));
          index += 2;
        } else if(code[index+1] == '=') {
          tokens.push_back(makeToken(AND_EQ, line, "&="));
          index += 2;
        } else {
          tokens.push_back(makeToken(BIT_AND, line, "&"));
          index++;
        }
        break;
      } 

      case '^': {
        if(code[index+1] == '^') {
          tokens.push_back(makeToken(XOR, line, "^^"));
          index += 2;
        } else if(code[index+1] == '=') {
          tokens.push_back(makeToken(XOR_EQ, line, "^="));
          index += 2;
        } else {
          tokens.push_back(makeToken(BIT_XOR, line, "^"));
          index++;
        }
        break;
      } 

      case '|': {
        if(code[index+1] == '|') {
          tokens.push_back(makeToken(OR, line, "||"));
          index += 2;
        } else if(code[index+1] == '=') {
          tokens.push_back(makeToken(OR_EQ, line, "|="));
          index += 2;
        } else {
          tokens.push_back(makeToken(BIT_OR, line, "|"));
          index++;
        }
        break;
      } 
    }

    //if it's in double quotes, it's a String
    if(code[index] == '"') {

      uint32_t currentIndex = index+1;
      uint32_t startLine = line;
      
      while(code[currentIndex] != '"' && currentIndex < codeLength) {

        if(code[currentIndex] == '\n') {
          line++;
        }

        currentIndex++;
      }
      
      
      //allocate memory for the lexeme and copy it
      char* lexeme = new char[currentIndex-index];
      for(uint32_t i = index+1; i < currentIndex; i++)
        lexeme[i-index-1] = code[i];
      lexeme[currentIndex-index-1] = 0;
      
      //if closing quotation mark is not found, throw an error
      if(currentIndex == codeLength) {
        throw LexerError(startLine+1, codeLines->at(startLine), lexeme, "String literal not terminated with \"");         
      }

      Data tokenVal;
      char* c = new char[currentIndex-index];
      std::strcpy(c, lexeme);
      tokenVal.allocated = (void*) c;

      tokens.push_back(makeToken(STRING, startLine, lexeme, tokenVal));
      index = currentIndex+1;

    } else if(isdigit(code[index]) || code[index] == '.') {
 
      //parse integers
      uint32_t decimalCount = 0;
      uint32_t currentIndex = index;
    
      while(isdigit(code[currentIndex]) || code[currentIndex] == '.' || code[currentIndex] == '_') {

        if(code[currentIndex] == '.') {
          if(decimalCount > 0) {
            
            //first create lexeme
            char* errorLex = new char[currentIndex-index+2];
            for(uint32_t i = index; i <= currentIndex; i++) {
              errorLex[i-index] = code[i];
            }
            errorLex[currentIndex-index+1] = '\0';   
            throw LexerError(line+1, codeLines->at(line), errorLex, "Number literal can have at most 1 decimal point"); 
            
          } else {
            decimalCount++;
          }
        }
        
        currentIndex++;
      }

      
      //make sure this token actually ends here
      if(isEndOfToken(code[currentIndex])) {
 
        //Copy numerical value to add to Token
        char* lexeme = new char[currentIndex-index+1];
        strncpy(lexeme, code+index, currentIndex-index); 
        lexeme[currentIndex-index] = 0;
        
        //union to store parsed numbers
        Data tokenVal;

        if(decimalCount == 1) {
          tokenVal.floatingPoint = stringToDouble(code, index, currentIndex-1);
          tokens.push_back(makeToken(DOUBLE, line, lexeme, tokenVal)); 
        } else {
          tokenVal.integer = stringToInt(code, index, currentIndex-1);
          tokens.push_back(makeToken(INT32, line, lexeme, tokenVal)); 
        }
        
      } else {
        
       // !!! Otherwise throw an exception
       //first copy error lexeme, including bad character
       char* errorLexeme = new char[currentIndex-index+2];
       strncpy(errorLexeme, code+index, currentIndex-index+1);
       errorLexeme[currentIndex-index+1] = '\0';
       
       throw LexerError(line+1, codeLines->at(line), errorLexeme, "Number literal can only contain digits and <= 1 decimal point"); 
      }

      index = currentIndex;

    } else if(code[index] == '_' || isalpha(code[index])) {
      
      uint32_t currentIndex = index;
      while(code[currentIndex] == '_' || isalpha(code[currentIndex]) || isdigit(code[currentIndex])) {
        currentIndex++;
      }

      //if identifier contains invalid characters, throw an error
      if(!isspace(code[currentIndex]) && !isEndOfToken(code[currentIndex])) {
        
        char* errorLexeme = new char[currentIndex-index+2];
        strncpy(errorLexeme, code+index, currentIndex-index+1);
        errorLexeme[currentIndex-index+1] = 0;
        
        throw LexerError(line+1, codeLines->at(line), errorLexeme, "Identifier can only contain digits, letters, and underscores");
      } 
      
      char* lexeme = new char[currentIndex-index+1];
      strncpy(lexeme, code+index, currentIndex-index);
      lexeme[currentIndex-index] = 0;

      //determine if keyword, if not then identifier
      TokenType tt = varOrKeywordTokenType(lexeme);
      if(tt == TRUE) {
        
        Data d; d.integer = 1;
        tokens.push_back(makeToken(BOOL, line, lexeme, d));
      
      } else if(tt == FALSE) {
        
        Data d; d.integer = 0;
        tokens.push_back(makeToken(BOOL, line, lexeme, d));
        
      } else {
        tokens.push_back(makeToken(varOrKeywordTokenType(lexeme), line, lexeme));  
      }
      
      index = currentIndex;
    }
    
    //consume white space
    while(isspace(code[index])) {
      if(code[index] == '\n') {
        line++;  
      }
      index++;
    }
  }
  

  tokens.push_back(makeToken(END, tokens[tokens.size()-1].line, "END"));

  return tokens;
}
