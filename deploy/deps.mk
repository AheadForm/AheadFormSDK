# Optional project settings: uncomment/edit, then rerun deploy/deploy.sh.
# CC/CXX use := to replace GNU Make's built-in defaults.
# CC := gcc
# CXX := g++
# SYSROOT ?= /opt/target/sysroot
# CFLAGS ?=
# CXXFLAGS ?=
# PKG_CONFIG_LIBDIR ?= /opt/target/sysroot/usr/lib/pkgconfig
# More settings: dependency_env below. Saved values: .deps/<id>/config.mk (generated).
PYTHON ?= python3
-include $(SDK_ROOT)/.deps/active.mk
-include $(SDK_ROOT)/.deps/$(DDS_BUILD_ID)/config.mk

dependency_env := CC CXX AR RANLIB CROSS_COMPILE SYSROOT CPPFLAGS CFLAGS CXXFLAGS LDFLAGS LDLIBS PKG_CONFIG PKG_CONFIG_PATH PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR
# Read explicit project/environment settings before applying saved defaults.
export AHEADFORM_CONFIG_INPUTS := $(foreach var,$(dependency_env),$(if $(filter-out undefined default,$(origin $(var))),$(var)))

ifneq ($(filter undefined default,$(origin CC)),)
CC := $(or $(DDS_DEFAULT_CC),cc)
endif
ifneq ($(filter undefined default,$(origin CXX)),)
CXX := $(or $(DDS_DEFAULT_CXX),c++)
endif
ifneq ($(filter undefined default,$(origin AR)),)
AR := $(or $(DDS_DEFAULT_AR),ar)
endif
RANLIB ?= $(or $(DDS_DEFAULT_RANLIB),ranlib)
CROSS_COMPILE ?= $(DDS_DEFAULT_CROSS_COMPILE)
SYSROOT ?= $(DDS_DEFAULT_SYSROOT)
CPPFLAGS ?= $(DDS_DEFAULT_CPPFLAGS)
CFLAGS ?= $(DDS_DEFAULT_CFLAGS)
CXXFLAGS ?= $(DDS_DEFAULT_CXXFLAGS)
LDFLAGS ?= $(DDS_DEFAULT_LDFLAGS)
LDLIBS ?= $(DDS_DEFAULT_LDLIBS)
PKG_CONFIG ?= $(or $(DDS_DEFAULT_PKG_CONFIG),pkg-config)
PKG_CONFIG_PATH ?= $(DDS_DEFAULT_PKG_CONFIG_PATH)
PKG_CONFIG_LIBDIR ?= $(DDS_DEFAULT_PKG_CONFIG_LIBDIR)
PKG_CONFIG_SYSROOT_DIR ?= $(DDS_DEFAULT_PKG_CONFIG_SYSROOT_DIR)

# Capture customer settings before each Makefile adds SDK-only options.
$(foreach var,$(dependency_env),$(eval export AHEADFORM_$(var) := $($(var))))
dependency_query := $(PYTHON) "$(SDK_ROOT)/deploy/dds.py"
sysroot_flags := $(if $(SYSROOT),--sysroot="$(SYSROOT)")

ifneq ($(_AHEADFORM_CONFIG_ONLY),1)
ifneq ($(MAKECMDGOALS),clean)
ifeq ($(strip $(DDS_BUILD_ID)),)
$(error DDS is not prepared. Run bash "$(SDK_ROOT)/deploy/deploy.sh" --build-only)
endif
dds_cflags := $(shell $(dependency_query) query dds-cflags --id "$(DDS_BUILD_ID)")
dds_libs := $(shell $(dependency_query) query dds-libs --id "$(DDS_BUILD_ID)")
gstreamer_cflags := $(shell $(dependency_query) query gst-cflags --id "$(DDS_BUILD_ID)")
gstreamer_libs := $(shell $(dependency_query) query gst-libs --id "$(DDS_BUILD_ID)")
endif

.PHONY: check-deps
check-deps:
	@$(dependency_query) check --id "$(DDS_BUILD_ID)"
endif
