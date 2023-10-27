CC = arm-linux-gcc
CFLAGS = -Wall -Wextra -Wshadow -Wundef -Os -mcpu=cortex-m4
LD = $(CC)
LDFLAGS =

OBJ = blake2s.o gls254-cm4.o curve.o scalar.o
TESTOBJ = test_gls254.o

all: test_gls254

clean:
	-rm -f $(OBJ) $(TESTOBJ) test_gls254 test_gls254.gdb

test_gls254: $(OBJ) $(TESTOBJ)
	$(LD) $(LDFLAGS) -o test_gls254 $(OBJ) $(TESTOBJ)

blake2s.o: blake2s.c blake2.h
	$(CC) $(CFLAGS) -c -o blake2s.o blake2s.c

curve.o: curve.c blake2.h gls254.h inner.h
	$(CC) $(CFLAGS) -c -o curve.o curve.c

gls254-cm4.o: gls254-cm4.s
	$(CC) $(CFLAGS) -c -o gls254-cm4.o gls254-cm4.s

scalar.o: scalar.c gls254.h inner.h
	$(CC) $(CFLAGS) -c -o scalar.o scalar.c

test_gls254.o: test_gls254.c blake2.h gls254.h inner.h
	$(CC) $(CFLAGS) -c -o test_gls254.o test_gls254.c
