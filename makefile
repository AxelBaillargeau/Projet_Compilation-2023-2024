# $@ : the current target
# $^ : the current prerequisites
# $< : the first current prerequisite
CC = gcc
CFLAGS = -Wall -g -Iobj -Isrc
PARSER = tpc-bison
LEXER = flex

BIN:= bin
BIN_FILES= $(wildcard $(BIN)/*.*)

OBJ:= obj
OBJ_FILES=$(OBJ)/$(SRCS:%.c=%.o)

SRC:= src
SRC_FILES= $(wildcard $(SRC)/*.*)

OUT := tpcc

FILES:=$(OBJ)/*.o $(BIN)/$(OUT) $(OBJ)/$(PARSER).c $(OBJ)/$(PARSER).h $(OBJ)/$(LEXER).c
FILES_REC:= _anonymous.asm _anonymous


$(BIN)/tpcc: $(OBJ)/$(LEXER).o $(OBJ)/$(PARSER).o $(OBJ)/tree.o $(OBJ)/asm_writer.o $(OBJ)/symbole.o $(OBJ)/errors.o $(OBJ)/tree_value.o
	if [ ! -d $(BIN) ]; then \
		mkdir $(BIN); \
	fi
	$(CC) -g -o $@ $^

$(OBJ)/tree.o: $(SRC)/tree.c $(SRC)/tree.h

$(OBJ)/$(PARSER).o: $(OBJ)/$(PARSER).c $(SRC)/tree.h
$(OBJ)/$(LEXER).o: $(OBJ)/$(LEXER).c $(OBJ)/$(PARSER).h


$(OBJ)/%.o: $(SRC)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJ)/$(LEXER).c: $(SRC)/$(LEXER).lex $(OBJ)/$(PARSER).h
	flex -o $@ $<

$(OBJ)/$(PARSER).c $(OBJ)/$(PARSER).h: $(SRC)/$(PARSER).y
	if [ ! -d $(OBJ) ]; then \
		mkdir $(OBJ); \
	fi
	bison -d -o $(OBJ)/$(PARSER).c $<

all: clean
	make debug

debug:
	make tpcc --debug

clean:
	for file in $(FILES); do \
		if [ -f $$file ]; then \
			rm $$file; \
		fi; \
	done
	for file in $(FILES_REC); do \
		find . -name $$file -delete; \
	done
	if [ -d $(BIN) ]; then \
		rmdir $(BIN); \
	fi
	if [ -d $(OBJ) ]; then \
		rmdir $(OBJ); \
	fi

tests:
	(cd test && ./test.sh)

_anonymous: _anonymous.asm
	nasm -f elf64 -o $(OBJ)/anon.o _anonymous.asm -g
	gcc $(OBJ)/anon.o -o $(BIN)/_anonymous -nostartfiles -no-pie -g 