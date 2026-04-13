CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -pedantic $(shell pkg-config --cflags sdl2)
LDFLAGS = $(shell pkg-config --libs sdl2) -lm

TARGET = gravity_sim

SRC = main.c simulation.c scenes.c render.c
OBJ = main.o simulation.o scenes.o render.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

run: all
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
