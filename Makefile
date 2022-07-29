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

all: executable

debug: executable
debug: CFLAGS += -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined

executable: $(BUILD_DIR)/$(TARGET_EXEC)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: clean test compdb

clean:
	@rm -rf $(BUILD_DIR)

test: executable
	@ASAN_OPTIONS=detect_leaks=0 ./tests/test.sh

compdb: clean
	@bear -- $(MAKE) && \
	 mv compile_commands.json build
