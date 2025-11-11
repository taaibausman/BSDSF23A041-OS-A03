# Compiler and flags
CC = gcc
CFLAGS = -Wall -g -Iinclude

# Linker flags (includes readline for future versions)
LDFLAGS = -lreadline

# Directories
SRC_DIR = src
BIN_DIR = bin

# Target binary
TARGET = $(BIN_DIR)/myshell

# Source and object files
SRC = $(SRC_DIR)/main.c $(SRC_DIR)/shell.c $(SRC_DIR)/execute.c
OBJ = $(SRC:.c=.o)

# Default target
all: $(BIN_DIR) $(TARGET)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile each .c file into .o
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(SRC_DIR)/*.o $(TARGET)

# Build and run the shell
run: all
	./$(TARGET)
