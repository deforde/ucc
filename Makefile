TARGET_EXEC := ucc

CFLAGS := -g3 -D_FORTIFY_SOURCE=2 -fsanitize=address -Wall -Wextra -Wpedantic -Werror
LDFLAGS := -fsanitize=address

BUILD_DIR := build
SRC_DIRS := src

SRCS := $(shell find $(SRC_DIRS) -name '*.c')

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CFLAGS += $(INC_FLAGS) -MMD -MP

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: clean test compdb

clean:
	@rm -rf $(BUILD_DIR)

test: $(BUILD_DIR)/$(TARGET_EXEC)
	@ASAN_OPTIONS=detect_leaks=0 ./tests/test.sh

compdb: clean
	@bear -- $(MAKE) && \
	 mv compile_commands.json build
