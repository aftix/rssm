CC=gcc
CFLAGS=-Iinclude/ -c -ggdb -DVERBOSE=1 -Wall -pedantic
LFLAGS=-ggdb -lm

OBJ=obj
BIN=bin

OBJS=$(OBJ)/main.o $(OBJ)/setting.o $(OBJ)/control.o $(OBJ)/rssmio.o
EXEC=$(BIN)/rssm

all: $(OBJ) $(BIN) $(OBJS)
	$(CC) $(OBJS) $(LFLAGS) -o $(EXEC)

$(OBJ):
	@mkdir -p $(OBJ)
$(BIN):
	@mkdir -p $(BIN)

$(OBJ)/%.o: src/%.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	@rm -rf $(OBJ)
	@rm -rf $(BIN)
