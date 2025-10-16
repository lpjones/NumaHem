# Makefile for building libtmem.so
# Override variables from the command line if desired:
CC      := clang
CFLAGS  := -g3 -Wall -O0 -fPIC
LDFLAGS := -shared
LIBS    := -lsyscall_intercept -lnuma -lpthread -ldl

# Optional feature switches
pebs_stats ?= 1
CFLAGS += -DPEBS_STATS=$(pebs_stats)

# Sources / Objects
SRCS := interpose.c tmem.c pebs.c timer.c logging.c spsc-ring.c fifo.c algorithm.c
OBJS := $(SRCS:.c=.o)

# Dependency files (generated)
DEPS := $(OBJS:.o=.d)

# Output
TARGET := libtmem.so

.PHONY: all default clean distclean help

default: all

all: $(TARGET)

# Link shared object
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Compile .c -> .o and generate dependency files (-MMD -MP)
# -MMD: generate .d files for dependencies (excluding system headers)
# -MP: add phony targets to avoid errors when headers are removed
# Use -fPIC already in CFLAGS (kept from your snippet)
%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Include dependency files if present
-include $(DEPS)

# Convenience targets
clean:
	$(RM) $(OBJS) $(TARGET) $(DEPS)

distclean: clean
	# Add any extra files to remove for a full clean here

help:
	@echo "Usage:"
	@echo "  make            # build $(TARGET)"
	@echo "  make CC=clang   # override compiler"
	@echo "  make CFLAGS='-O2 -fPIC'  # override flags"
	@echo "  make pebs_stats=0  # disable PEBS_STATS define"
	@echo "  make clean      # remove objects and target"

