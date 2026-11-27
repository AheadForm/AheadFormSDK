SDK_ROOT := $(abspath ..)
include $(SDK_ROOT)/deploy/deps.mk
target ?= AheadFormSDK
api_lib := API/libAheadFormSDKApi.a
# Pass base settings without flags added by this Makefile.
api_make_flags := $(foreach var,$(dependency_env),$(var)='$(subst ','"'"',$(AHEADFORM_$(var)))')
.DEFAULT_GOAL := BuildSDK

GIT_HASH := $(shell git rev-parse --verify HEAD 2>/dev/null || printf 'unknown')
GIT_DIRTY_SUFFIX := $(shell if test -n "$$(git -c core.autocrlf=true status --porcelain --untracked-files=normal -- . ':(top,exclude)$(target)' 2>/dev/null)"; then printf '%s' '-dirty'; fi)
BUILD_TIME := $(shell TZ=Asia/Shanghai date '+%Y-%m-%dT%H:%M:%S%:z' 2>/dev/null || printf 'unknown')
BUILD_USER := $(shell id -un 2>/dev/null || whoami 2>/dev/null || printf 'unknown')

dirs := $(shell find . -type d)
c_srcs := $(foreach dir,$(filter-out ./API ./API/%,$(dirs)),$(wildcard $(dir)/*.c))
cpp_srcs := $(foreach dir,$(filter-out ./API ./API/%,$(dirs)),$(wildcard $(dir)/*.cpp))
objs := $(patsubst %.c,%.o,$(c_srcs)) $(patsubst %.cpp,%.o,$(cpp_srcs))
incFlags := $(foreach dir,$(dirs),-I$(dir))

linker := $(CC)
ifneq ($(strip $(cpp_srcs)),)
linker := $(CXX)
endif

CYCLONEDDS_CFLAGS ?= $(dds_cflags)
CYCLONEDDS_LIBS ?= $(dds_libs)
GSTREAMER_CFLAGS ?= $(gstreamer_cflags)
GSTREAMER_LIBS ?= $(gstreamer_libs)

override CPPFLAGS += -D_GNU_SOURCE $(incFlags) $(CYCLONEDDS_CFLAGS) $(GSTREAMER_CFLAGS) \
	-DSX_GIT_COMMIT_HASH=\"$(GIT_HASH)$(GIT_DIRTY_SUFFIX)\" \
	-DSX_BUILD_TIME=\"$(BUILD_TIME)\" \
	-DSX_BUILD_USER=\"$(BUILD_USER)\"
override CFLAGS += -Wall -Werror -pthread
override CXXFLAGS += -Wall -Werror -pthread
override LDFLAGS += -pthread
override LDLIBS += $(CYCLONEDDS_LIBS) $(GSTREAMER_LIBS) -lm -lrt -ldl

dep_files := $(foreach f,$(objs),$(dir $(f)).$(notdir $(f)).d)
-include $(dep_files)

BuildSDK: $(api_lib) $(target)

$(api_lib): FORCE | check-deps
	$(MAKE) -C API -f subMakefile_buildApiLib $(api_make_flags)

$(target): $(objs) $(api_lib)
	$(linker) $(sysroot_flags) $^ -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "CREATE $(target)"
	@echo "./$(target) will run sdk in CodeProject"

$(objs): | check-deps

%.o: %.c
	$(CC) $(sysroot_flags) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(dir $@).$(notdir $@).d -MT $@ -c -o $@ $<

%.o: %.cpp
	$(CXX) $(sysroot_flags) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MF $(dir $@).$(notdir $@).d -MT $@ -c -o $@ $<

clean:
	$(MAKE) -C API -f subMakefile_buildApiLib $(api_make_flags) clean
	find . -type f \( -name '*.o' -o -name '*.o.d' \) ! -path './API/*' -delete
	rm -f $(target)

.PHONY: BuildSDK FORCE clean
