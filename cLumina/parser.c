#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "parser.h"

Parser* initParser(char* inputName, char* outputName, ParseFlag flags) {
	Parser* parser = malloc(sizeof(Parser));

	parser->lexer = initLexer(inputName, NULL);
	parser->current = nextToken(parser->lexer);
	parser->lastType = NULL;
	parser->flags = flags;

	parser->files = initFileList();
	addFile(parser->files, inputName);

	parser->outputFile = fopen(outputName, "w");

	parser->compiler = initCompiler(parser->outputFile, NULL);

	parser->numIfs = 0;
	parser->numElses = 0;
	parser->numAnds = 0;
	parser->numOrs = 0;
	parser->numWhiles = 0;

	// defining built-in immediates
	//strdup is used here because these predefined string literals are freed later, because there will be allocated strings in the same place later
	defineType(parser->compiler, strdup("any"), 3, 8, *parser->current, NULL, NULL, false, NULL); 
	Type *intType = defineType(parser->compiler, strdup("int"), 3, 8, *parser->current, NULL, NULL, false, NULL);
	defineType(parser->compiler, strdup("ptr"), 3, 8, *parser->current, NULL, NULL, false, NULL);
	Type *charType = defineType(parser->compiler, strdup("char"), 4, 1, *parser->current, NULL, NULL, false, NULL);
	Type *strType = defineType(parser->compiler, strdup("str"), 3, 8, *parser->current, NULL, NULL, true, charType);
	defineType(parser->compiler, strdup("bool"), 4, 1, *parser->current, NULL, NULL, false, NULL);
	defineType(parser->compiler, strdup("NULL"), 4, 8, *parser->current, NULL, NULL, false, NULL);
	
	// defining sycall built-in
	char *name = strdup("syscall");
	int nameLen = strlen(name);
	int id = 0;
	Type* typeAny = findType(parser->compiler, strdup("any"), 3);
	Type* returnType = typeAny;

	VariableList *parameters = initVariableList();
	int i = 0;
	while (i < 7) {
		addVariable(parameters, strdup(""), i, 0, typeAny);
		i++;
	}

	parser->compiler->currentStackSize++;
	defineVariable(parser->compiler, strdup("argc"), 4, intType);
	parser->compiler->currentStackSize++;
	defineVariable(parser->compiler, strdup("argv"), 4, initType(strdup(""), *parser->current, 8, NULL, NULL, true, strType));

	defineFunction(parser->compiler, name, nameLen, 0, returnType, parameters);

	parser->numFuncs = 1; // this is because there is a predefined built-in syscall function, which has id 0

	parser->strings = initStringList();

	parser->hadError = false;
	
	return parser;
}

void freeParser(Parser* parser) {
	freeLexer(parser->lexer);

	if (parser->current != NULL) {
		freeToken(parser->current);
	}

	freeCompiler(parser->compiler);

	freeFileList(parser->files);

	freeStringList(parser->strings);

	fclose(parser->outputFile);

	free(parser);
}

void setLastType(Parser* parser, Type* type) {
	if (type == NULL) {
		parseError(parser, *parser->current, "type not found");
	}

	parser->lastType = type;
}

FileList *initFileList() {
	FileList *list = malloc(sizeof(FileList));

	list->size = 0;
	list->maxSize = 8;
	list->files = malloc(sizeof(char*) * 8);
	
	return list;
}

void freeFileList(FileList *list) {
	int i = 0;
	while (i < list->size) {
		free(list->files[i]);
		i++;
	}

	free(list->files);
	free(list);
}

void addFile(FileList *list, char* file) {
	list->files[list->size++] = strdup(file);
	
	if (list->size == list->maxSize) {
		list->maxSize *= 2;
		list->files = realloc(list->files, sizeof(char*) * list->maxSize);
	}
}

bool findFile(FileList *list, char* file) {
	int i = 0;
	while (i < list->size) {
		if (strcmp(list->files[i], file) == 0) {
			return true;
		}
		i++;
	}
	return false;
}

Token* next(Parser* parser) {
	if (parser->current != NULL) {
		freeToken(parser->current);
	}

	parser->current = nextToken(parser->lexer);

	if (parser->current->type == TOKEN_END_OF_FILE && parser->lexer->outer != NULL) {
		Lexer *current = parser->lexer;
		parser->lexer = current->outer;
		freeLexer(current);

		parser->current = next(parser);
	}

	return parser->current;
}

typedef void (*ParseFn)(Parser*);

typedef enum {
	PREC_NONE,
	PREC_BLOCK,
	PREC_STATEMENT,
	PREC_TYPE,
	PREC_FUNC,
	PREC_RETURN_STATEMENT,
	PREC_WHILE_STATEMENT,
	PREC_IF_STATEMENT,
	PREC_ASSIGNMENT,
	PREC_ARG,
	PREC_EXPR,
	PREC_AND,
	PREC_OR,
	PREC_COMPARISON,
	PREC_TERM,
	PREC_FACTOR,
	PREC_BITWISE,
	PREC_READ,
	PREC_UNARY,
	PREC_PRIMARY
} Precedence;

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

_Static_assert(TOKEN_TYPES_NUM == 40, "Exhaustive handling of token types in parsing");

ParseRule parseTable[] = {
	[TOKEN_NUMBER] = {number, NULL, PREC_PRIMARY},
	[TOKEN_CHAR] = {character, NULL, PREC_PRIMARY},
	[TOKEN_STR] = {string, NULL, PREC_PRIMARY},
	[TOKEN_PLUS] = {NULL, binary, PREC_TERM},
	[TOKEN_MINUS] = {unary, binary, PREC_UNARY},
	[TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
	[TOKEN_EQUAL] = {NULL, NULL, PREC_ASSIGNMENT},
	[TOKEN_AND] = {NULL, binary, PREC_BITWISE},
	[TOKEN_ANDAND] = {NULL, binary, PREC_AND},
	[TOKEN_PIPE] = {NULL, binary, PREC_BITWISE},
	[TOKEN_PIPEPIPE] = {NULL, binary, PREC_OR},
	[TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
	[TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
	[TOKEN_LESSEQUAL] = {NULL, binary, PREC_COMPARISON},
	[TOKEN_GREATEREQUAL] = {NULL, binary, PREC_COMPARISON},
	[TOKEN_EQUALEQUAL] = {NULL, binary, PREC_COMPARISON},
	[TOKEN_BANG] = {unary, NULL, PREC_OR},
	[TOKEN_BANGEQUAL] = {NULL, binary, PREC_COMPARISON},
	[TOKEN_RARROW] = {NULL, NULL, PREC_NONE},
	[TOKEN_LPAREN] = {group, NULL, PREC_PRIMARY},
	[TOKEN_RPAREN] = {NULL, NULL, PREC_BLOCK},
	[TOKEN_LBRACKET] = {NULL, indexArray, PREC_READ},
	[TOKEN_RBRACKET] = {NULL, NULL, PREC_BLOCK},
	[TOKEN_LBRACE] = {NULL, NULL, PREC_BLOCK},
	[TOKEN_RBRACE] = {NULL, NULL, PREC_BLOCK},
	[TOKEN_SEMICOLON] = {NULL, NULL, PREC_STATEMENT},
	[TOKEN_VAR] = {NULL, NULL, PREC_ASSIGNMENT},
	[TOKEN_IF] = {NULL, NULL, PREC_IF_STATEMENT},
	[TOKEN_ELSE] = {NULL, NULL, PREC_IF_STATEMENT},
	[TOKEN_WHILE] = {NULL, NULL, PREC_WHILE_STATEMENT},
	[TOKEN_FUNC] = {NULL, NULL, PREC_FUNC},
	[TOKEN_TYPE] = {NULL, NULL, PREC_TYPE},
	[TOKEN_IMPORT] = {NULL, NULL, PREC_STATEMENT},
	[TOKEN_SIZEOF] = {typeSize, NULL, PREC_PRIMARY},
	[TOKEN_RETURN] = {NULL, NULL, PREC_RETURN_STATEMENT},
	[TOKEN_COMMA] = {NULL, NULL, PREC_ARG},
	[TOKEN_PERIOD] = {NULL, property, PREC_READ},
	[TOKEN_IDENTIFIER] = {identifier, NULL, PREC_PRIMARY},
	[TOKEN_END_OF_FILE] = {NULL, NULL, PREC_NONE},
	[TOKEN_ERROR] = {NULL, NULL, PREC_NONE}
};

void panic(Parser* parser) {
	Tokentype type = parser->current->type;
	while (type != TOKEN_SEMICOLON && type != TOKEN_RBRACE && type != TOKEN_END_OF_FILE) {
		type = next(parser)->type;
	}

	if (type != TOKEN_END_OF_FILE) {
		next(parser);
	}
}

void parseError(Parser* parser, Token token, char* message) {
	char* word = strndup(token.word, token.wordLen);
	printf("%s:%d ERROR at '%s': ", token.fileName, token.line, word);
	printf("%s", message);
	printf("\n");

	free(word);

	parser->hadError = true;

	panic(parser);
}

Token parsePrecedence(Parser* parser, Precedence precedence) {
	Token token = *parser->current;
	ParseRule rule = parseTable[token.type];

	if (rule.precedence < precedence) {
		parseError(parser, token, "unexpected token");
		next(parser);

		token.type = TOKEN_ERROR;

		return token; 
	}

	if (rule.prefix == NULL) {
		parseError(parser, token, "unexpected token");
		next(parser);

		token.type = TOKEN_ERROR;

		return token;
	}

	rule.prefix(parser);

	token = *parser->current;
	rule = parseTable[token.type];

	while (rule.precedence >= precedence) {
		if (rule.infix == NULL) {
			parseError(parser, token, "unexpected token");
			next(parser);

			token.type = TOKEN_ERROR;
			return token;
		}

		rule.infix(parser);

		token = *parser->current;
		rule = parseTable[token.type];
	}

	return token;
}

Token consumeToken(Parser* parser, Tokentype type, char* message) {
	Token token = *parser->current;
	if (token.type != type) {
		parseError(parser, token, message);

		token.type = TOKEN_ERROR;
		return token;
	}

	next(parser);

	return token;
}

Type *consumeType(Parser* parser, char* message) {
	Token token = consumeToken(parser, TOKEN_IDENTIFIER, message);

	Type *type = findType(parser->compiler, token.word, token.wordLen);

	if (type == NULL) {
		parseError(parser, token, message);
	}

	while (parser->current->type == TOKEN_LBRACKET) {
		next(parser);
		consumeToken(parser, TOKEN_RBRACKET, "expected array definition to end with ']'");

		type = initType(strdup(""), token, 8, NULL, NULL, true, type);
	}

	return type;
}

void group(Parser* parser) {
	next(parser);

	if (parser->current->type == TOKEN_IDENTIFIER) {
		Type *type = findType(parser->compiler, parser->current->word, parser->current->wordLen);
		if (type != NULL) { // type cast
			type = consumeType(parser, "expected type in typecast"); // this also checks for arrays

			consumeToken(parser, TOKEN_RPAREN, "expected closing parenthesis");

			parsePrecedence(parser, PREC_PRIMARY); 

			setLastType(parser, type);

			return;
		}
	}

	expression(parser);

	consumeToken(parser, TOKEN_RPAREN, "expected closing parenthesis");
}

void dumpNumber(Parser* parser, Token value) {
	if (parser->flags & FLAG_DUMP) {
		printf("%s:%d: number '%s'\n", value.fileName, value.line, value.word);
	}
}

void number(Parser* parser) {
	Token value = *parser->current;
	if (value.type != TOKEN_NUMBER) {
		printf("incorrect reference in parseTable: '%s' points to number\n", tokenTypes[value.type]);
	}

	dumpNumber(parser, value);

	char* result;

	int numberValue = strtol(value.word, &result, 10);
	if (result - value.word != value.wordLen) {
		parseError(parser, value, "could not convert string '%s' to int");
		return;
	}

	writeNumber(parser->compiler, numberValue);

	setLastType(parser, findType(parser->compiler, "int", 3));

	next(parser);
}

void dumpCharacter(Parser* parser, Token value) {
	if (parser->flags & FLAG_DUMP) {
		printf("%s:%d: character '%c'\n", value.fileName, value.line, value.word[1]);
	}
}

void character(Parser* parser) {
	Token value = *parser->current;
	if (value.type != TOKEN_CHAR) {
		printf("incorrect reference in parseTable: '%s' points to character\n", tokenTypes[value.type]);
	}

	dumpCharacter(parser, value);

	char *chr = value.word + 1;
	writeCharacter(parser->compiler, &chr);

	setLastType(parser, findType(parser->compiler, "char", 4));

	next(parser);
}

void string(Parser* parser) {
	Token value = *parser->current;
	if (value.type != TOKEN_STR) {
		printf("incorrect reference in parseTable: '%s' points to string\n", tokenTypes[value.type]);
	}

	uint16_t id = parser->strings->size;
	addString(parser->strings, value);

	writeString(parser->compiler, id);

	setLastType(parser, findType(parser->compiler, "str", 3));

	next(parser);
}

void indexArray(Parser* parser) {
	next(parser);

	Type *type = parser->lastType;

	if (!type->isArray) {
		parseError(parser, *parser->current, "cannot index from a type that is not an array");
		return;
	}

	Type *arrayType = type->arrayType;

	if (arrayType == NULL) {
		printf("array type is null");
		return;
	}

	expression(parser);

	if (strcmp(parser->lastType->name, "int") != 0) {
		parseError(parser, *parser->current, "expected index to be of type int");
		return;
	}

	consumeToken(parser, TOKEN_RBRACKET, "expected ']' after index");

	if (parser->current->type == TOKEN_EQUAL) {
		next(parser);

		expression(parser);

		if (strcmp(parser->lastType->name, arrayType->name) != 0) {
			parseError(parser, *parser->current, "wrong type of value to assign to this array");
			return;
		}

		writeWriteIndex(parser->compiler, arrayType->size);
	} else {
		writeReadIndex(parser->compiler, arrayType->size);
	}

	setLastType(parser, arrayType);
}

void property(Parser* parser) {
	next(parser);

	Token name = consumeToken(parser, TOKEN_IDENTIFIER, "expected property name");

	if (name.type == TOKEN_ERROR) { return; }

	Type *type = parser->lastType;

	if (type == NULL) {
		parseError(parser, name, "problem in type when reading property");
		return;
	}

	Property *property = findProperty(type->properties, name.word, name.wordLen);

	if (property == NULL) {
		parseError(parser, name, "cannot find property");
		return;
	}

	Type *newType = type->propertyTypes[property->index];

	if (parser->current->type == TOKEN_EQUAL) {
		next(parser);

		expression(parser);

		writeWriteProperty(parser->compiler, property->offset, newType->size);
	} else {
		writeReadProperty(parser->compiler, property->offset, newType->size);
	}

	setLastType(parser, newType);
}

void typeSize(Parser* parser) {
	if (parser->current->type != TOKEN_SIZEOF) {
		printf("incorrect reference in parseTable: '%s' points to sizeof\n", tokenTypes[parser->current->type]);
	}

	next(parser);

	if (consumeToken(parser, TOKEN_LPAREN, "expected '(' after 'sizeof'").type == TOKEN_ERROR) { return; }
	Type *type = consumeType(parser, "expected type in 'sizeof' expression");
	
	if (type == NULL) { return; }

	int size;
	if (type->properties == NULL) {
		size = type->size;
	} else {
		size = type->properties->totalTypeSize;
	}

	writeNumber(parser->compiler, size);

	setLastType(parser, findType(parser->compiler, "int", 3));

	if (consumeToken(parser, TOKEN_RPAREN, "expected ')' after 'sizeof' expression").type == TOKEN_ERROR) { return; }
}

void identifier(Parser* parser) {
	Token identifier = *parser->current;
	if (identifier.type != TOKEN_IDENTIFIER) {
		printf("incorrect reference in parseTable: '%s' points to identifier\n", tokenTypes[identifier.type]);
	}

	Token nextToken = *next(parser);
	if (nextToken.type == TOKEN_EQUAL) {
		next(parser);

		expression(parser);

		Variable *var = findVariable(parser->compiler, identifier.word, identifier.wordLen);

		if (var == NULL) {
			parseError(parser, identifier, "variable '%s' is undefined");
			return;
		}

		writeAssignment(parser->compiler, var->position, var->functionDepth);
	} else if (nextToken.type == TOKEN_LPAREN) {
		next(parser);
		Function *func = findFunction(parser->compiler, identifier.word, identifier.wordLen);

		if (func == NULL) {
			parseError(parser, identifier, "function is undefined");
			return;
		}

		uint16_t numCalls = func->numCalls++;

		if (func->parameters->size > 0) {
			expression(parser);

			if (strcmp(func->parameters->variables[0]->type->name, parser->lastType->name) != 0 &&
					strcmp(func->parameters->variables[0]->type->name, "any") != 0) { 
				parseError(parser, *parser->current, "incorrect type passed to function");
				return;
			}

			int i = 0;
			while (parser->current->type == TOKEN_COMMA) {
				next(parser);

				i++;
				if (func->parameters->size < i + 1) { 
					parseError( parser, *parser->current, "too many arguments passed to function"); 
					return;
				}

				expression(parser);

				if (strcmp(func->parameters->variables[i]->type->name, parser->lastType->name) != 0 &&
						strcmp(func->parameters->variables[i]->type->name, "any") != 0) { 
					parseError(parser, *parser->current, "incorrect type passed to function"); 
					return;
				}
			}

			if (i + 1 < func->parameters->size) {
				parseError(parser, *parser->current, "too few arguments passed to function");
				return;
			}
		}

		if (consumeToken(parser, TOKEN_RPAREN, "expected ')' after arguments").type == TOKEN_ERROR) { return; }
		parser->compiler->currentStackSize -= func->parameters->size;
		
		writeCall(parser->compiler, func->id, numCalls);

		setLastType(parser, func->returnType);
	} else {
		Variable *var = findVariable(parser->compiler, identifier.word, identifier.wordLen);

		if (var == NULL) {
			parseError(parser, identifier, "variable '%s' is undefined");
			return;
		}

		writeIdentifier(parser->compiler, var->position, var->functionDepth);

		setLastType(parser, var->type);
	}
}

void dumpBinary(Parser* parser, Token operator) {
	if (parser->flags & FLAG_DUMP) {
		printf("%s:%d: binary '%s'\n", operator.fileName, operator.line, tokenTypes[operator.type]);
	}
}

void binary(Parser* parser) {
	Token operator = *parser->current;
	Type value1 = *parser->lastType;

	Precedence precedence;

	switch (operator.type) {
		case TOKEN_PLUS:
		case TOKEN_MINUS:
			precedence = PREC_TERM + 1;
			break;
		case TOKEN_STAR:
			precedence = PREC_FACTOR + 1;
			break;
		case TOKEN_AND:
		case TOKEN_PIPE:
			precedence = PREC_BITWISE + 1;
			break;
		case TOKEN_ANDAND:
			precedence = PREC_AND + 1;
			break;
		case TOKEN_PIPEPIPE:
			precedence = PREC_OR + 1;
			break;
		case TOKEN_LESS:
		case TOKEN_GREATER:
		case TOKEN_LESSEQUAL:
		case TOKEN_GREATEREQUAL:
		case TOKEN_EQUALEQUAL:
		case TOKEN_BANGEQUAL:
			precedence = PREC_COMPARISON + 1;
			break;
		default:
			printf("incorrect reference in parseTable: '%s' points to binary\n", tokenTypes[operator.type]);
	}

	next(parser);

	uint32_t andId, orId;
	if (operator.type == TOKEN_ANDAND) {
		writeCondition(parser->compiler);
		andId = parser->numAnds++;
		writeJumpNotEqual(parser->compiler, "addr_and", andId);
	} else if (operator.type == TOKEN_PIPEPIPE) {
		writeCondition(parser->compiler);
		orId = parser->numOrs++;
		writeJumpEqual(parser->compiler, "addr_or", orId);
	}

	parsePrecedence(parser, precedence);

	switch (operator.type) {
		case TOKEN_PLUS:
			dumpBinary(parser, operator);

			if (strcmp(value1.name, "int") != 0 && strcmp(value1.name, "char") != 0) {
				parseError(parser, value1.token, "can not add something that is not 'int' or 'char'");
				printf("NOTE: left hand side is of type: '%s'\n", value1.name);
				return;
			}
			if (strcmp(parser->lastType->name, "int") != 0 && strcmp(parser->lastType->name, "char") != 0) {
				parseError(parser, parser->lastType->token, "can not add something that is not 'int' or 'char'");
				printf("NOTE: right hand side is of type: '%s'\n", parser->lastType->name);
				return;
			}

			if (strcmp(value1.name, "char") == 0 && strcmp(parser->lastType->name, "char") == 0) {
				parseError(parser, parser->lastType->token, "can not add 2 characters together");
				return;
			}

			writeAdd(parser->compiler);

			if (strcmp(value1.name, "char") == 0 || strcmp(parser->lastType->name, "char") == 0) {
				setLastType(parser, findType(parser->compiler, "char", 4));
			} else {
				setLastType(parser, findType(parser->compiler, "int", 3));
			}

			break;
		case TOKEN_MINUS:
			dumpBinary(parser, operator);

			if (strcmp(value1.name, "int") != 0 && strcmp(value1.name, "char") != 0) {
				parseError(parser, value1.token, "can not subtract something that is not 'int' or 'char'");
				printf("NOTE: left hand side is of type: '%s'\n", value1.name);
				return;
			}
			if (strcmp(parser->lastType->name, "int") != 0 && strcmp(parser->lastType->name, "char") != 0) {
				parseError(parser, parser->lastType->token, "can not subtract something that is not 'int' or 'char'");
				printf("NOTE: right hand side is of type: '%s'\n", parser->lastType->name);
				return;
			}

			if (strcmp(value1.name, "int") == 0 && strcmp(parser->lastType->name, "char") == 0) {
				parseError(parser, parser->lastType->token, "can not subtract a 'char' from an 'int'");
				return;
			}

			writeSubtract(parser->compiler);

			if (strcmp(value1.name, "char") == 0 && strcmp(parser->lastType->name, "int") == 0) {
				setLastType(parser, findType(parser->compiler, "char", 4));
			} else {
				setLastType(parser, findType(parser->compiler, "int", 3));
			}

			break;
		case TOKEN_STAR:
			dumpBinary(parser, operator);

			if (strcmp(value1.name, "int") != 0 || strcmp(parser->lastType->name, "int") != 0) {
				parseError(parser, operator, "can not multiply something that is not an integer");
			}

			writeMult(parser->compiler);

			setLastType(parser, findType(parser->compiler, "int", 3));

			break;
		case TOKEN_AND:
			dumpBinary(parser, operator);

			if (strcmp(value1.name, "int") != 0 || strcmp(parser->lastType->name, "int") != 0) {
				parseError(parser, operator, "can not use bitwise and on a value that is not an integer");
				return;
			}

			writeBitAnd(parser->compiler);

			setLastType(parser, findType(parser->compiler, "int", 3));
			break;
		case TOKEN_PIPE:
			dumpBinary(parser, operator);

			if (strcmp(value1.name, "int") != 0 || strcmp(parser->lastType->name, "int") != 0) {
				parseError(parser, operator, "can not use bitwise or on a value that is not an integer");
				return;
			}

			writeBitOr(parser->compiler);

			setLastType(parser, findType(parser->compiler, "int", 3));
			break;
		case TOKEN_ANDAND:
			dumpBinary(parser, operator);

			if (strcmp(value1.name, "bool") != 0 || strcmp(parser->lastType->name, "bool") != 0) {
				parseError(parser, operator, "can not use logical and on a value that is not boolean");
				return;
			}

			writeJump(parser->compiler, "addr_end_and", andId);
			writeAddress(parser->compiler, "addr_and", andId);

			writeNumber(parser->compiler, 0); // false

			parser->compiler->currentStackSize--; // we add one of two values to the stack, not both

			writeAddress(parser->compiler, "addr_end_and", andId);

			setLastType(parser, findType(parser->compiler, "bool", 4));
			break;
		case TOKEN_PIPEPIPE:
			dumpBinary(parser, operator);

			if (strcmp(value1.name, "bool") != 0 || strcmp(parser->lastType->name, "bool") != 0) {
				parseError(parser, operator, "can not use logical or on a value that is not boolean");
				return;
			}

			writeJump(parser->compiler, "addr_end_or", orId);
			writeAddress(parser->compiler, "addr_or", orId);

			writeNumber(parser->compiler, 1); // true

			parser->compiler->currentStackSize--; // we add one of two values to the stack, not both

			writeAddress(parser->compiler, "addr_end_or", orId);

			setLastType(parser, findType(parser->compiler, "bool", 4));
			break;
		case TOKEN_LESS:
			dumpBinary(parser, operator);

			if (strcmp(value1.name, parser->lastType->name) != 0) {
				parseError(parser, operator, "can not compare two values with different type");
				printf("NOTE: types are: '%s' and '%s'\n", value1.name, parser->lastType->name);
				return;
			}

			writeLess(parser->compiler);

			setLastType(parser, findType(parser->compiler, "bool", 4));
			break;
		case TOKEN_LESSEQUAL:
			dumpBinary(parser, operator);

			if (strcmp(value1.name, parser->lastType->name) != 0) {
				parseError(parser, operator, "can not compare two values with different type");
				printf("NOTE: types are: '%s' and '%s'\n", value1.name, parser->lastType->name);
				return;
			}

			writeLessEqual(parser->compiler);

			setLastType(parser, findType(parser->compiler, "bool", 4));
			break;
		case TOKEN_GREATER:
			dumpBinary(parser, operator);

			if (strcmp(value1.name, parser->lastType->name) != 0) {
				parseError(parser, operator, "can not compare two values with different type");
				printf("NOTE: types are: '%s' and '%s'\n", value1.name, parser->lastType->name);
				return;
			}

			writeGreater(parser->compiler);

			setLastType(parser, findType(parser->compiler, "bool", 4));
			break;
		case TOKEN_GREATEREQUAL:
			dumpBinary(parser, operator);

			if (strcmp(value1.name, parser->lastType->name) != 0) {
				parseError(parser, operator, "can not compare two values with different type");
				printf("NOTE: types are: '%s' and '%s'\n", value1.name, parser->lastType->name);
				return;
			}

			writeGreaterEqual(parser->compiler);

			setLastType(parser, findType(parser->compiler, "bool", 4));
			break;
		case TOKEN_EQUALEQUAL:
			dumpBinary(parser, operator);

			if (strcmp(value1.name, parser->lastType->name) != 0) {
				parseError(parser, operator, "can not compare two values with different type");
				printf("NOTE: types are: '%s' and '%s'\n", value1.name, parser->lastType->name);
				return;
			}

			writeEqual(parser->compiler);

			setLastType(parser, findType(parser->compiler, "bool", 4));
			break;
		case TOKEN_BANGEQUAL:
			dumpBinary(parser, operator);

			if (strcmp(value1.name, parser->lastType->name) != 0) {
				parseError(parser, operator, "can not compare two values with different type");
				printf("NOTE: types are: '%s' and '%s'\n", value1.name, parser->lastType->name);
				return;
			}

			writeNotEqual(parser->compiler);

			setLastType(parser, findType(parser->compiler, "bool", 4));
			break;
	}
}

void dumpUnary(Parser* parser, Token operator) {
	if (parser->flags & FLAG_DUMP) {
		printf("%s:%d: unary '%s'\n", operator.fileName, operator.line, tokenTypes[operator.type]);
	}
}

void unary(Parser* parser) {
	Token operator = *parser->current;

	next(parser);

	parsePrecedence(parser, PREC_UNARY);

	switch (operator.type) {
		case TOKEN_MINUS:
			dumpUnary(parser, operator);

			if (strcmp(parser->lastType->name, "int") != 0) {
				parseError(parser, parser->lastType->token, "cannot take the negative of type '%s'");
				return;
			}

			writeNegative(parser->compiler);

			break;
		case TOKEN_BANG:
			dumpUnary(parser, operator);

			if (strcmp(parser->lastType->name, "int") != 0) {
				parseError(parser, parser->lastType->token, "cannot take the inverse of this type");
				return;
			}

			writeBitNot(parser->compiler);

			break;
		default:
			printf("incorrect reference in parseTable: '%s' points to unary\n", tokenTypes[operator.type]);
	}
}


void expression(Parser* parser) {
	if (parsePrecedence(parser, PREC_EXPR).type == TOKEN_ERROR) {
		return;
	}

	while (parser->current->type != TOKEN_END_OF_FILE) {
		ParseRule rule = parseTable[parser->current->type];

		if (rule.precedence < PREC_EXPR) {
			break;
		}

		if (rule.infix == NULL) {
			parseError(parser, *parser->current, "unexpected token");

			break;
		}

		rule.infix(parser);
	}
}

void dumpIdentifier(Parser* parser, Token token) {
	if (parser->flags & FLAG_DUMP) {
		printf("%s:%d: identifier '%s'\n", token.fileName, token.line, token.word);
	}
}

void variableDefinition(Parser* parser) {
	Token identifier = consumeToken(parser, TOKEN_IDENTIFIER, "expected variable name in definition");

	dumpIdentifier(parser, identifier);

	if (findLocalVariable(parser->compiler, identifier.word, identifier.wordLen) != NULL) { 
		parseError(parser, identifier, "there already exists a variable with this name"); 
		return;
	}

	if (findType(parser->compiler, identifier.word, identifier.wordLen) != NULL) {
		parseError(parser, identifier, "there already exists a type with this name");
		return;
	}

	if (findFunction(parser->compiler, identifier.word, identifier.wordLen) != NULL) { 
		parseError(parser, identifier, "there already exists a function with this name"); 
		return;
	}

	consumeToken(parser, TOKEN_EQUAL, "expected '=' after variable name in definition");

	expression(parser);
	consumeToken(parser, TOKEN_SEMICOLON, "expected ';' after variable definition");

	Type* type = findType(parser->compiler, "NULL", 4);
	if (parser->lastType != NULL) {
		type = parser->lastType;
	}

	defineVariable(parser->compiler, identifier.word, identifier.wordLen, type);
}

void whileStatement(Parser* parser) {
	uint32_t whileId = parser->numWhiles++;

	writeAddress(parser->compiler, "addr_while_condition", whileId);
	consumeToken(parser, TOKEN_LPAREN, "expected '(' after 'while' keyword");

	expression(parser);

	if (parser->lastType == NULL) {
		return;
	}

	if (strcmp(parser->lastType->name, "bool") != 0) {
		parseError(parser, parser->lastType->token, "expected while condition to be of type boolean");
		return;
	}

	writeCondition(parser->compiler);

	consumeToken(parser, TOKEN_RPAREN, "expected ')' after condition");

	writeJumpNotEqual(parser->compiler, "addr_while_end", whileId);

	consumeToken(parser, TOKEN_LBRACE, "expected '{' before 'while' block");

	block(parser, NULL, NULL);

	writeJump(parser->compiler, "addr_while_condition", whileId);
	writeAddress(parser->compiler, "addr_while_end", whileId);
}

void ifStatement(Parser* parser, uint32_t elseId) {
	uint32_t ifId = parser->numIfs++;

	if (consumeToken(parser, TOKEN_LPAREN, "expected '(' after 'if' keyword").type == TOKEN_ERROR) {
		return;
	}

	expression(parser);

	if (parser->lastType == NULL) {
		return;
	}
	
	if (strcmp(parser->lastType->name, "bool") != 0) {
		parseError(parser, parser->lastType->token, "expected if condition to be of type boolean");
		return;
	}

	writeCondition(parser->compiler);
	
	consumeToken(parser, TOKEN_RPAREN, "expected ')' after condition");

	writeJumpNotEqual(parser->compiler, "addr_if", ifId);

	consumeToken(parser, TOKEN_LBRACE, "expected '{' before 'if' block");

	block(parser, NULL, NULL);

	if (parser->current->type == TOKEN_ELSE) {
		next(parser);

		writeJump(parser->compiler, "addr_else", elseId);

		writeAddress(parser->compiler, "addr_if", ifId);

		if (parser->current->type == TOKEN_IF) {
			next(parser);
			ifStatement(parser, elseId);
			return;
		}

		consumeToken(parser, TOKEN_LBRACE, "expected '{' before 'else' block");

		block(parser, NULL, NULL);

		writeAddress(parser->compiler, "addr_else", elseId);
	} else {
		writeAddress(parser->compiler, "addr_if", ifId);
	}
}

void functionDefinition(Parser* parser) {
	uint32_t funcId = parser->numFuncs++;

	Token name = consumeToken(parser, TOKEN_IDENTIFIER, "expected function name");

	if (findVariable(parser->compiler, name.word, name.wordLen) != NULL) {
		parseError(parser, name, "there already exists a variable with this name");
		return;
	}

	if (findType(parser->compiler, name.word, name.wordLen) != NULL) {
		parseError(parser, name, "there already exists a type with this name");
		return;
	}

	if (findLocalFunction(parser->compiler, name.word, name.wordLen) != NULL) {
		parseError(parser, name, "there already exists a type with this name");
		return;
	}

	if (name.type == TOKEN_ERROR) { return;}

	if (consumeToken(parser, TOKEN_LPAREN, "expected '(' after function name").type == TOKEN_ERROR) { return; }

	VariableList *parameters = initVariableList();

	if (parser->current->type != TOKEN_RPAREN) {
		Type *type = consumeType(parser, "expected parameter type");
		Token name = consumeToken(parser, TOKEN_IDENTIFIER, "expected parameter name");

		int i = 0;
		int functionDepth = parser->compiler->functionDepth + 1;

		addVariable(parameters, strndup(name.word, name.wordLen), i, functionDepth, type);

		while (parser->current->type == TOKEN_COMMA) {
			next(parser);
			Type *type = consumeType(parser, "expected parameter type");
			Token name = consumeToken(parser, TOKEN_IDENTIFIER, "expected parameter name");

			if (type == NULL || name.type == TOKEN_ERROR) {
				return;
			}

			addVariable(parameters, strndup(name.word, name.wordLen), ++i, functionDepth, type);
		}	
	}
	
	if (consumeToken(parser, TOKEN_RPAREN, "expected ')' after function arguments").type == TOKEN_ERROR) { return; }

	Type *type;

	if (parser->current->type == TOKEN_RARROW) {
		next(parser);

		type = consumeType(parser, "expected return type");
	} else {
		type = findType(parser->compiler, "NULL", 4);
	}

	defineFunction(parser->compiler, name.word, name.wordLen, funcId, type, parameters);

	consumeToken(parser, TOKEN_LBRACE, "expected '{' before function block");

	writeBeginFunction(parser->compiler, funcId, parameters->size);

	block(parser, findFunction(parser->compiler, name.word, name.wordLen), parameters);

	writeAddress(parser->compiler, "addr_func_end", funcId);
}

void block(Parser* parser, Function *func, VariableList *parameters) {
	Compiler* scopeCompiler = initCompiler(parser->outputFile, parser->compiler);
	
	if (func != NULL) {
		scopeCompiler->function = func;
		scopeCompiler->functionDepth++;
	}

	if (parameters != NULL) {
		int i = 0;
		while (i < parameters->size) {
			scopeCompiler->currentStackSize++;
			Variable *var = parameters->variables[i];
			defineVariable(scopeCompiler, strdup(var->name), strlen(var->name), var->type);

			i++;
		}
	}

	if (func != NULL) {
		scopeCompiler->currentStackSize++;
	}

	parser->compiler = scopeCompiler;

	while (parser->current->type != TOKEN_END_OF_FILE && parser->current->type != TOKEN_RBRACE) {
		statement(parser);
	}

	if (scopeCompiler->function != scopeCompiler->outer->function) {// the outermost block of the function body
		if (func->returnType != findType(parser->compiler, "NULL", 4)) {
			if (!scopeCompiler->hasReturned) {
				parseError(parser, *parser->current, "not al code paths return a value");
				return;
			}
		} else {
			uint16_t numVars = parser->compiler->currentStackSize - func->parameters->size - 1;
			writeReturnEmpty(parser->compiler, numVars, func->parameters->size);
		}
	} else {
		int numLocalVariables = scopeCompiler->variableList->size;
		writePop(scopeCompiler, numLocalVariables);
	}

	consumeToken(parser, TOKEN_RBRACE, "expected '}' after block");

	parser->compiler = scopeCompiler->outer;
	freeCompiler(scopeCompiler);
}

void returnStatement(Parser* parser) {
	Function *func = parser->compiler->function;

	if (func == NULL) {
		parseError(parser, *parser->current, "can only return from a function");
		return;
	}

	//calculate the amount of variables in the function, as they need to be dropped from the data stack
	
	uint16_t numVars = parser->compiler->currentStackSize - func->parameters->size - 1;
	Compiler* currentCompiler = parser->compiler->outer;

	while (currentCompiler->function == func) {
		numVars += currentCompiler->currentStackSize;
		currentCompiler = currentCompiler->outer;
	}

	if (strcmp(func->returnType->name, "NULL") == 0) {
		consumeToken(parser, TOKEN_SEMICOLON, "expected empty return in a 'null' function");

		writeReturnEmpty(parser->compiler, numVars, func->parameters->size);
	} else {
		expression(parser);

		if (strcmp(func->returnType->name, parser->lastType->name) != 0) {
			parseError(parser, *parser->current, "incorrect return type");
			printf("NOTE: expected: '%s', recieved: '%s'\n", func->returnType->name, parser->lastType->name);
		}

		consumeToken(parser, TOKEN_SEMICOLON, "expected ';' after return statement");

		writeReturnValue(parser->compiler, numVars, func->parameters->size);
	}

	parser->compiler->hasReturned = true;
}

void importStatement(Parser* parser) {
	Token name = *parser->current;
	if (name.type != TOKEN_STR) { 
		parseError(parser, name, "expected file name as string"); 
		return;
	}

	char* fileName = strndup(name.word + 1, name.wordLen - 2);

	if (findFile(parser->files, fileName)) {
		next(parser);
		free(fileName);
		return;
	}

	addFile(parser->files, fileName);
	Lexer *importFile = initLexer(fileName, parser->lexer);
	parser->lexer = importFile;
	parser->current = nextToken(importFile);

	free(fileName);
}

void typeDefinition(Parser* parser) {
	Token name = consumeToken(parser, TOKEN_IDENTIFIER, "expected type name in definition");

	if (findVariable(parser->compiler, name.word, name.wordLen) != NULL) {
		parseError(parser, name, "there already exists a variable with this name");
		return;
	}

	if (findType(parser->compiler, name.word, name.wordLen) != NULL) {
		parseError(parser, name, "there already exists a type with this name");
		return;
	}

	if (findFunction(parser->compiler, name.word, name.wordLen) != NULL) {
		parseError(parser, name, "there already exists a function with this name");
		return;
	}

	Type *type = defineType(parser->compiler, name.word, name.wordLen, 8, name, NULL, NULL, false, NULL);

	if (consumeToken(parser, TOKEN_LBRACE, "expected '{' after type name").type == TOKEN_ERROR) { return; }

	int i = 0;
	PropertyList *properties = initPropertyList();
	TypeList *types = initTypeList();

	while (parser->current->type != TOKEN_RBRACE) {
		Type *type = consumeType(parser, "expected type property to start with type");
		Token propertyName = consumeToken(parser, TOKEN_IDENTIFIER, "expected name of property");

		if (type == NULL || propertyName.type == TOKEN_ERROR) { return; }

		addProperty(properties, strndup(propertyName.word, propertyName.wordLen), i, type->size);
		addType(types, type);
		i++;

		consumeToken(parser, TOKEN_SEMICOLON, "expected property to end with ';'");
	}

	type->properties = properties;
	type->propertyTypes = types->types;
	free(types);

	consumeToken(parser, TOKEN_RBRACE, "expected '}' after type definition");
}

void statement(Parser* parser) {
	if (parser->current->type == TOKEN_VAR) {
		next(parser);

		variableDefinition(parser);
	} else if (parser->current->type == TOKEN_WHILE) {
		next(parser);

		whileStatement(parser);
	} else if (parser->current->type == TOKEN_IF) {
		next(parser);

		ifStatement(parser, parser->numElses++);
	} else if (parser->current->type == TOKEN_FUNC) {
		next(parser);

		functionDefinition(parser);
	} else if (parser->current->type == TOKEN_TYPE) {
		next(parser);

		typeDefinition(parser);
	} else if (parser->current->type == TOKEN_RETURN) {
		next(parser);

		returnStatement(parser);
	} else if (parser->current->type == TOKEN_IMPORT) {
		next(parser);

		importStatement(parser);
	} else {
		expression(parser);
		consumeToken(parser, TOKEN_SEMICOLON, "expected ';' after expression");
		writePop(parser->compiler, 1);
	}
}

void parse(Parser* parser) {
	writeHeader(parser->compiler);

	while (parser->current->type != TOKEN_END_OF_FILE) {
		statement(parser);
	}

	writeFooter(parser->compiler, parser->strings);
}
