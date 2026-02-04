CC = cc
CFLAGS = -Wall -Wextra $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs)

TARGET = bin/ping
SRC = src/main.c

$(TARGET): $(SRC) | bin
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

bin:
	mkdir -p bin

clean:
	rm -rf bin

.PHONY: clean
