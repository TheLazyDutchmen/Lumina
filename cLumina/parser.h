#ifndef cLumina_parser_header
#define cLumina_parser_header

#include <stdbool.h>

#include "lexer.h"
#include "compiler.h"
#include "type.h"
#include "luminaString.h"

typedef enum {
	FLAG_DUMP = 1,
} ParseFlag;

typedef struct {
	int size;
	int maxSize;
	char** files;
} FileList;

FileList *initFileList();
void freeFileList(FileList *list);
void addFile(FileList* list, char* file);

typedef struct {
	Lexer* lexer;
	Token* current;
	Type* lastType;
	ParseFlag flags;

	FileList *files;

	Compiler* compiler;
	FILE* outputFile;

	int numIfs;
	int numElses;
	int numAnds;
	int numOrs;
	int numWhiles;
	int numFuncs;

	StringList *strings;

	bool hadError;
} Parser;

Parser* initParser(char* inputName, char* outputName, ParseFlag flags);
void freeParser(Parser* parser);

void parseError(Parser* parser, Token token, char* message);

void setLastType(Parser* parser, Type* type);

void number(Parser* parser);
void group(Parser* parser);
void character(Parser* parser);
void string(Parser* parser);
void indexArray(Parser* parser);
void property(Parser* parser);
void identifier(Parser* parser);
void unary(Parser* parser);
void binary(Parser* parser);
void typeSize(Parser* parser);
void expression(Parser* parser);
void condition(Parser* parser);
void whileStatement(Parser* parser);
void ifStatement(Parser* parser, uint32_t elseId);
void importStatement(Parser* parser);
void variableDefinition(Parser* parser);
void typeDefinition(Parser* parser);
void block(Parser* parser, Function *func, VariableList *parameters);
void statement(Parser* parser);
void returnStatement(Parser* parser);

void parse(Parser* parser);

#endif
