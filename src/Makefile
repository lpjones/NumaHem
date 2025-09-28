CC = gcc
# CFLAGS = -g -Wall -O3 -fPIC
CFLAGS = -g3 -Wall -O0 -fPIC
LDFLAGS = -shared
# INCLUDES = -I../../hemem-og/linux/kbuild/usr/include/
# LIBS = -lm -lpthread
# HEMEM_LIBS = $(LIBS) -ldl -lsyscall_intercept -L../Hoard/src -lhoard
LIBS = -lsyscall_intercept -lnuma -lpthread -ldl

pebs_stats ?= 1
CFLAGS += -DPEBS_STATS=$(pebs_stats)

default: libtmem.so

libtmem.so: interpose.o tmem.o pebs.o timer.o logging.o
	$(CC) $(LDFLAGS) -o libtmem.so interpose.o tmem.o pebs.o timer.o logging.o $(LIBS)

clean:
	$(RM) *.o *.so
