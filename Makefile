CC = gcc
CFLAGS = -Wall -Werror -g
LIBS = -lncurses

SRCS = main.c
OBJS = $(SRCS:.c=.o)
TARGET = atmsim

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
