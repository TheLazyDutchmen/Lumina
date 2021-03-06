#ifndef cLumina_compiler_header
#define cLumina_compiler_header

#include <stdio.h>
#include <stdbool.h>

#include "variable.h"
#include "function.h"
#include "luminaString.h"

#define CALLSTACKSIZE 2048

typedef struct Compiler{
	FILE* output;
	size_t currentStackSize;
	VariableList* variableList;
	TypeList* typeList;
	FunctionList* functionList;
	Function *function;
	bool hasReturned;
	uint16_t functionDepth;
	struct Compiler* outer;
} Compiler;

Compiler* initCompiler(FILE* output, Compiler* outer);
void freeCompiler(Compiler* compiler);

void defineVariable(Compiler* compiler, char* name, int nameLen, Type *type);
Variable *findVariable(Compiler* compiler, char* name, int nameLen);
Type *findVariableType(Compiler* compiler, char* name, int nameLen);
Variable *findLocalVariable(Compiler* compiler, char* name, int nameLen);

Type *defineType(Compiler* compiler, char* name, int nameLen, size_t size, Token token, PropertyList *properties, Type **propertyTypes, bool isArray, Type *arrayType);
Type *findType(Compiler* compiler, char* name, int nameLen);

void defineFunction(Compiler* compiler, char* name, int nameLen, int id, Type *type, VariableList *parameters);
int16_t findFunctionId(Compiler* compiler, char* name, int nameLen);
Function *findFunction(Compiler* compiler, char* name, int nameLen);
Function *findLocalFunction(Compiler* compiler, char* name, int nameLen);

void writeHeader(Compiler* compiler);
void writeFooter(Compiler* compiler, StringList *string);
void writePop(Compiler* compiler, int amount);

void writeAddress(Compiler* compiler, char* address, uint32_t id);
void writeBeginFunction(Compiler* compiler, uint32_t id, size_t numParameters);
void writeCall(Compiler* compiler, uint32_t id, uint16_t numCalls);
void writeReturnEmpty(Compiler* compiler, uint16_t numVars, uint16_t numParameters);
void writeReturnValue(Compiler* compiler, uint16_t numVars, uint16_t numParameters);
void writeCompare(Compiler* compiler);

void writeBitAnd(Compiler* compiler);
void writeBitOr(Compiler* compiler);
void writeBitNot(Compiler* compiler);

void writeLess(Compiler* compiler);
void writeLessEqual(Compiler* compiler);
void writeGreater(Compiler* compiler);
void writeGreaterEqual(Compiler* compiler);
void writeEqual(Compiler* compiler);
void writeNotEqual(Compiler* compiler);
void writeCondition(Compiler* compiler);

void writeJump(Compiler* compiler, char* address, uint32_t id);
void writeJumpEqual(Compiler* compiler, char* address, uint32_t id);
void writeJumpNotEqual(Compiler* compiler, char* address, uint32_t id);

void writeNumber(Compiler* compiler, int value);
void writeCharacter(Compiler* compiler, char* *value);
void writeString(Compiler* compiler, int id);
void writeReadIndex(Compiler* compiler, int size);
void writeWriteIndex(Compiler* compiler, int size);
void writeReadProperty(Compiler* compiler, int offset, int size);
void writeWriteProperty(Compiler* compiler, int offset, int size);
void writeIdentifier(Compiler* compiler, int offset, int currentDepth);
void writeAssignment(Compiler* compiler, int offset, int currentDepth);

void writeAdd(Compiler* compiler);
void writeSubtract(Compiler* compiler);
void writeNegative(Compiler* compiler);
void writeMult(Compiler* compiler);

void writePrint(Compiler* compiler); //temporary print function

#endif
