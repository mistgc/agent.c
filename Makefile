# ===== Toolchain =====
CC      		:= gcc
INCLUDE_DIRS 	:= -Ideps/openai-c/include
CFLAGS  		:= -Wall -Wextra -O2 -MMD -MP
CFLAGS  		+= $(INCLUDE_DIRS)
LDFLAGS 		:=
LDLIBS  		:= -lm -lcurl -lcjson
CMAKE   		:= cmake

# ===== Sources (recursively pick up src/ subdirectories) =====
SRCS := $(shell find src -name '*.c' | sort)
OBJS := $(patsubst src/%.c, build/obj/%.o, $(SRCS))
DEPS := $(OBJS:.o=.d)

TARGET    := agent
LIBOPENAI := build/libopenai/libopenai.a

EXAMPLES := examples/stream_output.c
EXAMPLE_BINS := $(patsubst examples/%.c, build/examples/%, $(EXAMPLES))
# 示例需要 src/ 里的模块, 但排除带 main() 的 main.o
EXAMPLE_LIBS := $(filter-out build/obj/main.o, $(OBJS))

all: $(TARGET) $(EXAMPLE_BINS)

$(TARGET): $(OBJS) $(LIBOPENAI)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(LIBOPENAI): $(shell find deps/openai-c/src deps/openai-c/include -name '*.c' -o -name '*.h')
	@mkdir -p $(dir $@)
	$(CMAKE) -B build/libopenai -S deps/openai-c
	$(MAKE) -C build/libopenai -j4

# Compile rule: mirrors src/ tree under build/obj/
build/obj/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Auto-generated header dependencies (.d files produced by -MMD -MP)
-include $(DEPS)

build/examples/%: examples/%.c $(EXAMPLE_LIBS) $(LIBOPENAI)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

.PHONY: all clean distclean

clean:
	rm -rf build/obj $(TARGET) $(EXAMPLE_BINS)

distclean: clean
	rm -rf build
