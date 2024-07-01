ifneq ($(shell pkg-config --exists libconfig && echo 0), 0)
$(error "libconfig is not installed")
endif

ifneq ($(shell pkg-config --exists libdpdk && echo 0), 0)
$(error "DPDK is not installed")
endif

CFLAGS = $(shell pkg-config --cflags libconfig libdpdk)
LDFLAGS = $(shell pkg-config --libs-only-L libconfig libdpdk)
LDLIBS = $(shell pkg-config --libs-only-l libconfig libdpdk)

CC=gcc
CFLAGS += -Wall -Werror -Wno-stringop-truncation -O3
INCLUDES = -I. -I./test/unity
LDFLAGS += -libverbs
LIBS=-pthread

# SRCS=main.c client.c config.c ib.c server.c setup_ib.c sock.c

TEST_DIR=test
UNITY_DIR=test/unity
BIN_DIR=bin

SRC_FILES = $(wildcard *.c)
TEST_FILES = $(wildcard $(TEST_DIR)/*.c)
UNITY_FILES = $(UNITY_DIR)/unity.c

SRC_OBJS=$(SRC_FILES:.c=.o)
TEST_OBJS=$(TEST_FILES:.c=.o)
UNITY_OBJS=$(UNITY_FILES:.c=.o)

PROG=$(BIN_DIR)/rdma-bench
TEST_EXEC=$(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/%,$(TEST_FILES))

all: $(PROG) $(TEST_EXEC)

debug: CFLAGS +=-g -DDEBUG
debug: clean
	@if command -v bear >/dev/null 2>&1; then \
		echo "Bear is installed, generating compile_commands.json"; \
		bear make all; \
	else \
		echo "Bear is not installed, skipping generation of compile_commands.json"; \
		make all; \
	fi

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(PROG): $(SRC_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SRC_OBJS) $(LDFLAGS) $(LIBS) $(LDLIBS)

$(TEST_EXEC): $(filter-out main.o, $(SRC_OBJS)) $(TEST_OBJS) $(UNITY_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS) $(LIBS) $(LDLIBS)


.PHONY: clean
clean:
	$(RM) *.o $(TEST_DIR)/*.o $(UNITY_DIR)/*.o *~ $(PROG) $(BIN_DIR)/* compile_commands.json
