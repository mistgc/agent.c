# ===== Toolchain =====
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -MMD -MP
CPPFLAGS := -Ideps/openai-c/include
LDFLAGS :=
LDLIBS  := -lm -lcurl -lcjson
CMAKE   := cmake

# ===== Sources (recursively pick up src/ subdirectories) =====
SRCS := $(shell find src -name '*.c' | sort)
OBJS := $(patsubst src/%.c, build/obj/%.o, $(SRCS))
DEPS := $(OBJS:.o=.d)

TARGET    := agent
LIBOPENAI := build/libopenai/libopenai.a

all: $(TARGET)

$(TARGET): $(OBJS) $(LIBOPENAI)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(LIBOPENAI): $(shell find deps/openai-c/src deps/openai-c/include -name '*.c' -o -name '*.h')
	mkdir -p $(dir $@)
	$(CMAKE) -B build/libopenai -S deps/openai-c
	$(MAKE) -C build/libopenai -j4

# Compile rule: mirrors src/ tree under build/obj/
build/obj/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Auto-generated header dependencies (.d files produced by -MMD -MP)
-include $(DEPS)

.PHONY: all clean distclean

clean:
	rm -rf build/obj $(TARGET)

distclean: clean
	rm -rf build
