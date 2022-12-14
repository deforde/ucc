TARGET_EXEC := ucc

CFLAGS := -g3 -D_FORTIFY_SOURCE=2 -Wall -Wextra -Wpedantic -Werror
LDFLAGS :=

BUILD_DIR := build
SRC_DIR := src

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIR) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CFLAGS += $(INC_FLAGS) -MMD -MP

STAGE1_DIR := stage1
UCC_STAGE1 := $(STAGE1_DIR)/$(TARGET_EXEC)

STAGE2_DIR := stage2
S2_OBJS := $(SRCS:%=$(STAGE2_DIR)/%.o)
UCC_STAGE2 := $(STAGE2_DIR)/$(TARGET_EXEC)

TEST_DIR := test
TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
TESTS := $(TEST_SRCS:.c=.out)
TESTS_STG2 := $(TEST_SRCS:%=$(STAGE2_DIR)/%.out)
$(info $(TESTS_STG2))

CC := clang

debug: ucc
debug: CFLAGS += -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined

release: ucc

ucc: $(UCC_STAGE1)

$(UCC_STAGE1): $(OBJS)
	mkdir -p $(dir $@)
	$(CC) -g3 $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/$(SRC_DIR)/%.c.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(STAGE2_DIR)/%.c: %.c
	mkdir -p $(dir $@)
	find $(SRC_DIR) -name "*.h" -exec cp {} $(STAGE2_DIR)/$(SRC_DIR) \;
	./stage2.sh $@ $<

$(STAGE2_DIR)/$(SRC_DIR)/%.c.o: $(STAGE2_DIR)/$(SRC_DIR)/%.c $(UCC_STAGE1)
	ASAN_OPTIONS=detect_leaks=0 ./$(UCC_STAGE1) -c -o $@ $<

$(UCC_STAGE2): $(S2_OBJS)
	mkdir -p $(dir $@)
	./$(UCC_STAGE1) $(S2_OBJS) -o $@ $(LDFLAGS)

.PHONY: clean test compdb test-stg2 test-all

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(STAGE1_DIR)
	rm -rf $(STAGE2_DIR)
	rm -f $(TEST_DIR)/*.o $(TEST_DIR)/*.out

$(TEST_DIR)/%.out: debug
	ASAN_OPTIONS=detect_leaks=0 ./$(UCC_STAGE1) -c -o $(TEST_DIR)/$*.o $(TEST_DIR)/$*.c
	$(CC) -g3 -o $@ $(TEST_DIR)/$*.o -xc $(TEST_DIR)/common

test: $(TESTS)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done

$(STAGE2_DIR)/%.out: $(UCC_STAGE2)
	./$(UCC_STAGE2) -c -o $(STAGE2_DIR)/$(*F).o $*
	$(CC) -g3 -o $(STAGE2_DIR)/$(@F) $(STAGE2_DIR)/$(*F).o -xc $(TEST_DIR)/common

test-stg2: $(TESTS_STG2)
	for i in $(^F); do echo $$i; ./$(STAGE2_DIR)/$$i || exit 1; echo; done

test-all: test test-stg2

compdb: clean
	bear -- $(MAKE)
	mv compile_commands.json build

-include $(DEPS)
