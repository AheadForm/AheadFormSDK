SDK_ROOT := $(abspath ..)
# Prefer the environment prepared by deployment; explicit PYTHON still takes precedence.
PYTHON ?= $(if $(wildcard .venv/bin/python),$(abspath .venv/bin/python),python3)
include $(SDK_ROOT)/deploy/deps.mk
.DEFAULT_GOAL := all

sdk_api_lib := $(SDK_ROOT)/CodeProject/API/libAheadFormSDKApi.so
# Pass base settings without flags added by this Makefile.
api_make_flags := $(foreach var,$(dependency_env),$(var)='$(subst ','"'"',$(AHEADFORM_$(var)))')

# Clean does not require Python packages, headers or a prepared DDS cache.
ifneq ($(MAKECMDGOALS),clean)
python_config := $(shell $(PYTHON) build_bindings.py configure --cross "$(or $(DDS_CROSS),0)" --python-config "$(PYTHON_CONFIG)")
ifeq ($(strip $(python_config)),)
$(error Python configuration failed; see the message above)
endif
include $(python_config)
endif

build_dir := .build/$(DDS_BUILD_ID)-$(python_abi)
api_extension := $(build_dir)/_aheadform_api$(python_suffix)
user_extension := $(build_dir)/_aheadform_user$(python_suffix)
api_obj := $(build_dir)/_aheadform_api.o
user_binding_obj := $(build_dir)/_aheadform_user.o
api_dirs := $(shell find ../CodeProject/API -type d)
user_dirs := $(shell find ../CodeProject/User -type d)
user_c_srcs := $(filter-out ../CodeProject/User/main.c ../CodeProject/User/HandleUserInCmd.c,$(foreach dir,$(user_dirs),$(wildcard $(dir)/*.c)))
user_cpp_srcs := $(foreach dir,$(user_dirs),$(wildcard $(dir)/*.cpp))
user_objs := $(patsubst ../CodeProject/%.c,$(build_dir)/%.o,$(user_c_srcs)) $(patsubst ../CodeProject/%.cpp,$(build_dir)/%.o,$(user_cpp_srcs))
incFlags := $(foreach dir,$(api_dirs) $(user_dirs),-I$(dir))

user_linker := $(CC)
ifneq ($(strip $(user_cpp_srcs)),)
user_linker := $(CXX)
endif

override CPPFLAGS += -D_GNU_SOURCE $(incFlags) $(dds_cflags) $(gstreamer_cflags)
override CFLAGS += -Wall -Werror -pthread -fPIC
override CXXFLAGS += -Wall -Werror -pthread -fPIC
override LDFLAGS += -pthread
libs := $(gstreamer_libs) -lm -lrt -ldl $(LDLIBS)
rpath := -Wl,-rpath,'$$ORIGIN:$$ORIGIN/../../../CodeProject/API'

ifeq ($(DDS_CROSS),1)
run_python := python3
run_title := 运行方法（目标设备）：
else
run_python := $(patsubst $(CURDIR)/%,%,$(PYTHON))
run_title := 运行方法：
endif

# Detect flag changes without forcing a compile on an unchanged make.
export AF_BUILD_CPPFLAGS := $(CPPFLAGS)
export AF_BUILD_CFLAGS := $(CFLAGS)
export AF_BUILD_CXXFLAGS := $(CXXFLAGS)
export AF_BUILD_LDFLAGS := $(LDFLAGS)
export AF_BUILD_LIBS := $(libs)

dep_files := $(api_obj:.o=.d) $(user_binding_obj:.o=.d) $(user_objs:.o=.d)
-include $(dep_files)

all: user

api: $(api_extension)
	@$(PYTHON) build_bindings.py activate $(python_config) $(build_dir)
	@printf '\nPython 扩展构建完成：\n  %s\n$(run_title) %s demo_api.py\n' \
		"$(abspath $(api_extension))" "$(run_python)"

user: $(user_extension)
	@$(PYTHON) build_bindings.py activate $(python_config) $(build_dir)
	@printf '\nPython 扩展构建完成：\n  %s\n  %s\n$(run_title) %s demo_user.py\n' \
		"$(abspath $(api_extension))" "$(abspath $(user_extension))" "$(run_python)"

$(sdk_api_lib): FORCE
	@$(MAKE) --no-print-directory -C $(SDK_ROOT)/CodeProject/API -f subMakefile_buildApiLib $(api_make_flags) api_so

# CPython resolves its own symbols when importing; -z defs is unsuitable here.
$(api_extension): $(api_obj) $(sdk_api_lib)
	$(CC) $(sysroot_flags) -shared $(api_obj) $(sdk_api_lib) -o $@ $(LDFLAGS) $(python_ldflags) $(libs) $(rpath)

$(user_extension): $(user_binding_obj) $(user_objs) $(api_extension) $(sdk_api_lib) $(build_dir)/user-sources
	$(user_linker) $(sysroot_flags) -shared $(user_binding_obj) $(user_objs) $(sdk_api_lib) -o $@ $(LDFLAGS) $(python_ldflags) $(libs) $(rpath)

$(build_dir)/_aheadform_api.c: bindings.py build_bindings.py $(python_config)
	$(PYTHON) build_bindings.py generate api $@

$(build_dir)/_aheadform_user.c: bindings.py build_bindings.py $(python_config)
	$(PYTHON) build_bindings.py generate user $@

# Removing a User source must also relink, without retaining old objects.
$(build_dir)/user-sources: FORCE | check-deps
	@mkdir -p $(dir $@)
	@printf '%s\n' $(user_c_srcs) $(user_cpp_srcs) > $@.tmp
	@cmp -s $@.tmp $@ && rm -f $@.tmp || mv -f $@.tmp $@

$(build_dir)/build-settings: FORCE | check-deps
	@$(PYTHON) build_bindings.py stamp $@

$(user_objs): $(build_dir)/user-sources

$(api_obj) $(user_binding_obj) $(user_objs): Makefile $(python_config) $(build_dir)/build-settings | check-deps

$(build_dir)/_aheadform_%.o: $(build_dir)/_aheadform_%.c
	$(CC) $(sysroot_flags) $(CPPFLAGS) $(python_includes) $(CFLAGS) -MMD -MP -c -o $@ $<

$(build_dir)/%.o: ../CodeProject/%.c
	@mkdir -p $(dir $@)
	$(CC) $(sysroot_flags) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

$(build_dir)/%.o: ../CodeProject/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(sysroot_flags) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c -o $@ $<

clean:
	@$(MAKE) --no-print-directory -C $(SDK_ROOT)/CodeProject/API -f subMakefile_buildApiLib $(api_make_flags) clean
	rm -rf .build

.SECONDARY: $(build_dir)/_aheadform_api.c $(build_dir)/_aheadform_user.c
.DELETE_ON_ERROR:
.PHONY: all api user clean FORCE
