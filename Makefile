# Compiler and flags
CC = gcc
# remember -g for debugging
CFLAGS = -Wall -Wextra -std=c17 -Ofast -march=native -flto -I./include

# Directories
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include

# Automatically get all .c and .h files
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES = $(SRC_FILES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Executable name
TARGET = $(BUILD_DIR)/web_server

# Default target: build the executable
all: $(TARGET)

# Rule to link object files into the final executable
$(TARGET): $(OBJ_FILES)
	$(CC) $(OBJ_FILES) -o $(TARGET)

# Rule to compile .c files into .o object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)  # Create the build directory if it doesn't exist
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target: remove object files and the executable
clean:
	rm -rf $(BUILD_DIR)

# Phony targets (not actual files)
.PHONY: all clean
