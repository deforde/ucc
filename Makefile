TARGET_EXEC := ucc

CFLAGS := -g3 -D_FORTIFY_SOURCE=2 -Wall -Wextra -Wpedantic -Werror
LDFLAGS :=

BUILD_DIR := build
SRC_DIRS := src

SRCS := $(shell find $(SRC_DIRS) -name '*.c')

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CFLAGS += $(INC_FLAGS) -MMD -MP

UCC := $(BUILD_DIR)/$(TARGET_EXEC)

TEST_SRCS := $(shell find tests -name '*.c')
TESTS := $(TEST_SRCS:.c=.out)

all: ucc

debug: ucc
debug: CFLAGS += -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined

ucc: $(UCC)

$(UCC): $(OBJS)
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@) && \
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: clean test compdb clean_test

clean: clean_test
	@rm -rf $(BUILD_DIR)

clean_test:
	@rm -rf tests/*.pre tests/*.out tests/*.pre tests/*.s

tests/%.out: clean_test ucc
	@$(CC) -o tests/$*.pre -E -P -C tests/$*.c && \
	ASAN_OPTIONS=detect_leaks=0 ./$(UCC) -o tests/$*.s tests/$*.pre && \
	$(CC) -o $@ tests/$*.s -xc tests/common

test: $(TESTS)
	@for i in $^; do echo $$i; ./$$i; echo '\n'; done

compdb: clean
	@bear -- $(MAKE) && \
	mv compile_commands.json build
