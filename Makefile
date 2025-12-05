CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_XOPEN_SOURCE_EXTENDED
LIBS = -lncurses -pthread
TARGET = connect4
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean