CC := gcc
target ?= AheadFormSDK
.DEFAULT_GOAL := $(target)

GIT_HASH := $(shell git rev-parse --verify HEAD 2>/dev/null || printf 'unknown')
GIT_DIRTY_SUFFIX := $(shell if test -n "$$(git -c core.autocrlf=true status --porcelain --untracked-files=normal -- . ':(top,exclude)$(target)' 2>/dev/null)"; then printf '%s' '-dirty'; fi)
BUILD_TIME := $(shell TZ=Asia/Shanghai date '+%Y-%m-%dT%H:%M:%S%:z' 2>/dev/null || printf 'unknown')
BUILD_USER := $(shell id -un 2>/dev/null || whoami 2>/dev/null || printf 'unknown')

src_dirs := $(shell find . -type d -name src)
srcs := $(foreach dir,$(src_dirs),$(wildcard $(dir)/*.c))
objs := $(patsubst %.c,%.o,$(srcs))
inc_dirs := $(shell find . -type d -name inc)
incFlags := $(foreach dir,$(inc_dirs),-I$(dir))

CPPFLAGS := -D_GNU_SOURCE $(incFlags) \
	-DSX_GIT_COMMIT_HASH=\"$(GIT_HASH)$(GIT_DIRTY_SUFFIX)\" \
	-DSX_BUILD_TIME=\"$(BUILD_TIME)\" \
	-DSX_BUILD_USER=\"$(BUILD_USER)\"
CFLAGS := -Wall -Werror -pthread
LDFLAGS := -pthread
PKG_CONFIG ?= pkg-config
GSTREAMER_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags CycloneDDS gstreamer-app-1.0)
GSTREAMER_LIBS ?= $(shell $(PKG_CONFIG) --libs CycloneDDS gstreamer-app-1.0)
CPPFLAGS += $(GSTREAMER_CFLAGS)
LDLIBS := -lm -lrt -lddsc $(GSTREAMER_LIBS)

dep_files := $(foreach f,$(objs),$(dir $(f)).$(notdir $(f)).d)
-include $(dep_files)

$(target): $(objs)
	$(CC) $^ -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "CREATE $(target)"

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(dir $@).$(notdir $@).d -MT $@ -c -o $@ $<

clean:
	find . -type f \( -name '*.o' -o -name '*.o.d' \) -delete
	rm -f $(target)

.PHONY: clean
