
SRC=xmodem.c
OBJ=xmodem.o
TARGET=xmodem

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) -c -o $@ $^

.PHONY: clean
clean:
	rm -f $(OBJ) $(TARGET)

