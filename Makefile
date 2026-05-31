CC = gcc
CFLAGS = -Wall -Wextra -O2
SRC = src/main.c src/parser.c src/varint.c
OUT = rbda

all:
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

clean:
	rm -f $(OUT)
