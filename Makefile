
CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = student_daemon
SOURCE = student_daemon.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

clean:
	rm -f $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild
