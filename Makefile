cc=gcc
CFLAGS= -std=gnu99 -Wall
LDLIBS= -lpthread -lm
SRCDIR=./src/

all: mole

mole: main.o my_nftw.o threading.o utils.o
	${cc} -o mole main.o my_nftw.o threading.o utils.o ${CFLAGS} ${LDLIBS} && rm *.o

main.o: ${SRCDIR}main.c ${SRCDIR}my_nftw.h
	${cc} -c ${SRCDIR}main.c ${CFLAGS} ${LDLIBS}

my_nftw.o: ${SRCDIR}my_nftw.c ${SRCDIR}my_nftw.h
	${cc} -c ${SRCDIR}my_nftw.c ${CFLAGS} ${LDLIBS}

threading.o: ${SRCDIR}threading.c ${SRCDIR}threading.h
	${cc} -c ${SRCDIR}threading.c ${CFLAGS} ${LDLIBS}

utils.o: ${SRCDIR}utils.c ${SRCDIR}utils.h
	${cc} -c ${SRCDIR}utils.c ${CFLAGS} ${LDLIBS}

.PHONY: clean all

clean:
	rm -f ${BINDIR}mole