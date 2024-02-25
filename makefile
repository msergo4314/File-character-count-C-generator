CC=gcc
CFLAGS=-Wall -g -std=c11

SRCS=A2.c
OBJS=$(SRCS:.c=.o)
EXECUTABLE=A2

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.hist *.o A2