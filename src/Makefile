# Makefile for building libtmem.so
# Override variables from the command line if desired:
CC      := clang
CFLAGS  := -g3 -Wall -O0 -fPIC
# CFLAGS  := -Wall -O3 -fPIC
LDFLAGS := -shared
LIBS    := -lsyscall_intercept -lnuma -lpthread -ldl

# knobs
pebs_stats ?= 1
cluster_algo ?= 0
hem_algo ?= 0
lru_algo ?= 0
bfs_algo ?= 0
dfs_algo ?= 0
his_size ?= 16
pred_depth ?= 16
dec_down ?= 0.0001
dec_up ?= 0.01
max_neighbors ?= 8
page_size ?= 2097152
# dram_size ?= 2147483648
dram_size ?= 0
dram_buffer ?= 4294967296
sample_period ?= 100
record ?= 1

CFLAGS += -DPEBS_STATS=$(pebs_stats)
CFLAGS += -DCLUSTER_ALGO=$(cluster_algo)
CFLAGS += -DHEM_ALGO=$(hem_algo)
CFLAGS += -DBFS_ALGO=$(bfs_algo)
CFLAGS += -DDFS_ALGO=$(dfs_algo)

CFLAGS += -DHISTORY_SIZE=$(his_size)
CFLAGS += -DMAX_PRED_DEPTH=$(pred_depth)
CFLAGS += -DDEC_DOWN=$(dec_down)
CFLAGS += -DDEC_UP=$(dec_up)
CFLAGS += -DMAX_NEIGHBORS=$(max_neighbors)
CFLAGS += -DPAGE_SIZE=$(page_size)
CFLAGS += -DDRAM_SIZE=$(dram_size)
CFLAGS += -DDRAM_BUFFER=$(dram_buffer)
CFLAGS += -DLRU_ALGO=$(lru_algo)
CFLAGS += -DSAMPLE_PERIOD=$(sample_period)
CFLAGS += -DRECORD=$(record)

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

