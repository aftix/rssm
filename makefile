CC=gcc
CFLAGS=-Iinclude/ -c
LFLAGS=

OBJ=obj
BIN=bin

OBJS=
EXEC=

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
