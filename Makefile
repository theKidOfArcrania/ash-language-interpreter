main: main.cpp token.h token.cpp lexer.h lexer.cpp parsetoken.h parsetoken.cpp \
	parsenode.h parsenode.cpp evaluator.h evaluator.cpp parser.h parser.cpp
	
	g++ -std=c++11 -o main main.cpp token.cpp lexer.cpp parsetoken.cpp \
	parsenode.cpp evaluator.cpp parser.cpp
      