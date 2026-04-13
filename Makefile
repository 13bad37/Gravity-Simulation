CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -pedantic $(shell pkg-config --cflags sdl2)
LDFLAGS = $(shell pkg-config --libs sdl2) -lm

TARGET = gravity_sim

SRC = main.c
OBJ = main.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)

run: all
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)