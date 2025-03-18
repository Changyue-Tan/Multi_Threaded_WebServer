# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -fsanitize=address,leak,undefined -O2 -std=c11 -g

# Target executable file
TARGET = WebServer

# Source files
SRCS = WebServer.c

# Default target
all: $(TARGET)

# Build the executable file
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Clean up generated files
clean:
	rm -f $(TARGET)

# Rebuild everything
rebuild: clean all

# Phony targets
.PHONY: all clean rebuild
