CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

# Source files
SRC = src/allocator.c src/main.c
OBJ = $(SRC:.c=.o)

# Output binary name
TARGET = allocator_test

# Default target
all: $(TARGET)

# Link the object files to create the executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean
