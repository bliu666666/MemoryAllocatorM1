# Compiler and compiler options
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

# Directory paths
SRC_DIR = src
OBJ_DIR = obj

# Create obj directory if it doesn't exist
$(shell mkdir -p $(OBJ_DIR))

# Source files and object files
SRCS = $(SRC_DIR)/allocator.c main.c
OBJS = $(OBJ_DIR)/allocator.o $(OBJ_DIR)/main.o

# Output executable file
TARGET = my_allocator

# Default target
all: $(TARGET)

# Compile allocator.c
$(OBJ_DIR)/allocator.o: $(SRC_DIR)/allocator.c $(SRC_DIR)/allocator.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compile main.c
$(OBJ_DIR)/main.o: main.c $(SRC_DIR)/allocator.h
	$(CC) $(CFLAGS) -c $< -o $@

# Link all object files to create the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

# Clean up generated files
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

# Run the program
run: $(TARGET)
	./$(TARGET)

# Phony targets
.PHONY: all clean run
