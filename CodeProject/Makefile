CC := gcc
target ?= AheadFormSDK
.DEFAULT_GOAL := $(target)

GIT_HASH := $(shell git rev-parse --verify HEAD 2>/dev/null || printf 'unknown')
GIT_DIRTY_SUFFIX := $(shell if test -n "$$(git -c core.autocrlf=true status --porcelain --untracked-files=normal -- . ':(top,exclude)$(target)' 2>/dev/null)"; then printf '%s' '-dirty'; fi)
BUILD_TIME := $(shell TZ=Asia/Shanghai date '+%Y-%m-%dT%H:%M:%S%:z' 2>/dev/null || printf 'unknown')
BUILD_USER := $(shell id -un 2>/dev/null || whoami 2>/dev/null || printf 'unknown')

dirs := $(shell find . -type d)
srcs := $(foreach dir,$(dirs),$(wildcard $(dir)/*.c))
objs := $(patsubst %.c,%.o,$(srcs))
incFlags := $(foreach dir,$(dirs),-I$(dir))

PKG_CONFIG ?= pkg-config

ifneq ($(MAKECMDGOALS),clean)
ifeq ($(shell $(PKG_CONFIG) --atleast-version=1.20 gstreamer-1.0 gstreamer-app-1.0 2>/dev/null && printf 'yes'),)
$(error GStreamer >= 1.20 is required; detected gstreamer-1.0/gstreamer-app-1.0 versions: $(shell $(PKG_CONFIG) --modversion gstreamer-1.0 gstreamer-app-1.0 2>/dev/null || printf 'unavailable'))
endif
endif

CYCLONEDDS_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags CycloneDDS)
CYCLONEDDS_LIBS ?= $(shell $(PKG_CONFIG) --libs CycloneDDS)
GSTREAMER_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags gstreamer-app-1.0)
GSTREAMER_LIBS ?= $(shell $(PKG_CONFIG) --libs gstreamer-app-1.0)

CPPFLAGS := -D_GNU_SOURCE $(incFlags) $(CYCLONEDDS_CFLAGS) $(GSTREAMER_CFLAGS) \
	-DSX_GIT_COMMIT_HASH=\"$(GIT_HASH)$(GIT_DIRTY_SUFFIX)\" \
	-DSX_BUILD_TIME=\"$(BUILD_TIME)\" \
	-DSX_BUILD_USER=\"$(BUILD_USER)\"
CFLAGS := -Wall -Werror -pthread
LDFLAGS := -pthread
LDLIBS := -lm -lrt $(CYCLONEDDS_LIBS) $(GSTREAMER_LIBS)

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
