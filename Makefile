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

TEST_DIR := test
TEST_SRCS := $(shell find $(TEST_DIR) -name '*.c')
TESTS := $(TEST_SRCS:.c=.out)

debug: ucc
debug: CFLAGS += -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined

release: ucc

ucc: $(UCC)

$(UCC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: clean test compdb

clean:
	@rm -rf $(BUILD_DIR)

$(TEST_DIR)/%.out: debug
	@$(CC) -o $(TEST_DIR)/$*.pre -E -P -C $(TEST_DIR)/$*.c
	@ASAN_OPTIONS=detect_leaks=0 ./$(UCC) -o $(TEST_DIR)/$*.s $(TEST_DIR)/$*.pre
	@$(CC) -g3 -o $@ $(TEST_DIR)/$*.s -xc $(TEST_DIR)/common

test: $(TESTS)
	@for i in $^; do echo $$i; ./$$i || exit 1; echo; done

compdb: clean
	@bear -- $(MAKE)
	@mv compile_commands.json build
