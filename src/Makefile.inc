# Copyright 2014-2018, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# src/Makefile.inc -- common Makefile rules for NVM library
#

TOP := $(dir $(lastword $(MAKEFILE_LIST)))..

include $(TOP)/src/common.inc

INCLUDE = $(TOP)/src/include

RPMEM_COMMON = $(TOP)/src/rpmem_common
vpath %.c $(RPMEM_COMMON)

COMMON = $(TOP)/src/common
vpath %.c $(COMMON)

INCS += -I../include -I../common/

# default CFLAGS
DEFAULT_CFLAGS += -Wall
DEFAULT_CFLAGS += -Werror
DEFAULT_CFLAGS += -Wmissing-prototypes
DEFAULT_CFLAGS += -Wpointer-arith
DEFAULT_CFLAGS += -Wunused-macros
DEFAULT_CFLAGS += -Wmissing-field-initializers
DEFAULT_CFLAGS += -Wsign-conversion
DEFAULT_CFLAGS += -Wsign-compare

ifeq ($(call check_Wconversion), y)
DEFAULT_CFLAGS += -Wconversion
endif
ifeq ($(call check_flag, -Wunreachable-code-return), y)
DEFAULT_CFLAGS += -Wunreachable-code-return
endif
ifeq ($(call check_flag, -Wmissing-variable-declarations), y)
DEFAULT_CFLAGS += -Wmissing-variable-declarations
endif

ifeq ($(DEBUG),1)
# Undefine _FORTIFY_SOURCE in case it's set in system-default or
# user-defined CFLAGS as it conflicts with -O0.
DEBUG_CFLAGS += -Wp,-U_FORTIFY_SOURCE
DEBUG_CFLAGS += -O0 -ggdb -DDEBUG
LIB_SUBDIR = /nvml_debug
OBJDIR = debug
else
DEFAULT_CFLAGS += -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
LIB_SUBDIR =
OBJDIR = nondebug
endif

# use defaults, if system or user-defined CFLAGS are not specified
CFLAGS ?= $(DEFAULT_CFLAGS)

CFLAGS += -std=gnu99
CFLAGS += -fno-common
CFLAGS += -pthread
CFLAGS += -DSRCVERSION=\"$(SRCVERSION)\"

ifeq ($(COVERAGE),1)
CFLAGS += $(GCOV_CFLAGS)
LDFLAGS += $(GCOV_LDFLAGS)
LIBS += $(GCOV_LIBS)
endif

CFLAGS += $(EXTRA_CFLAGS)

ifeq ($(DEBUG),1)
CFLAGS += $(EXTRA_CFLAGS_DEBUG) $(DEBUG_CFLAGS)
else
CFLAGS += $(EXTRA_CFLAGS_RELEASE)
endif

LDFLAGS += -Wl,-z,relro -Wl,--fatal-warnings -Wl,--warn-common $(EXTRA_LDFLAGS)

ifneq ($(NORPATH),1)
LDFLAGS += -Wl,-rpath=$(libdir)$(LIB_SUBDIR)
endif

# XXX: required by clock_gettime(), if glibc version < 2.17
# The os_clock_gettime() function is now in OS abstraction layer,
# linked to all the librariess, unit tests and benchmarks.
ifeq ($(call check_librt), n)
LIBS += -lrt
endif

define arch32_error_msg

##################################################
###  32-bit builds of NVML are not supported!  ###
###  Please, use 64-bit platform/compiler.     ###
##################################################

endef

TESTCMD := $(CC) $(CFLAGS) -dM -E -x c /dev/null -o /dev/null
TESTBUILD := $(shell $(TESTCMD) && echo 1 || echo 0)
ifneq ($(TESTBUILD), 1)
$(error "$(TESTCMD)" failed)
endif

LP64 := $(shell $(CC) $(CFLAGS) -dM -E -x c /dev/null | grep -Ec "__SIZEOF_LONG__.+8|__SIZEOF_POINTER__.+8" )
ifneq ($(LP64), 2)
$(error $(arch32_error_msg))
endif

LIBS_DESTDIR = $(DESTDIR)$(libdir)$(LIB_SUBDIR)

DIRNAME = $(shell basename $(CURDIR))

ifeq ($(OBJDIR),$(abspath $(OBJDIR)))
objdir = $(OBJDIR)/$(DIRNAME)
else
objdir = ../$(OBJDIR)/$(DIRNAME)
endif

LIB_OUTDIR ?= $(objdir)/..

ifneq ($(LIB_OUTDIR),)
LDFLAGS += -L$(LIB_OUTDIR)
endif

ifneq ($(SOURCE),)
_OBJS = $(SOURCE:.c=.o)
_OBJS_COMMON = $(patsubst $(COMMON)/%, %, $(_OBJS))
_OBJS_RPMEM_COMMON = $(patsubst $(RPMEM_COMMON)/%, %, $(_OBJS_COMMON))
OBJS += $(addprefix $(objdir)/, $(_OBJS_RPMEM_COMMON))
endif

ifneq ($(HEADERS),)
ifneq ($(filter 1 2, $(CSTYLEON)),)
TMP_HEADERS := $(addsuffix tmp, $(HEADERS))
TMP_HEADERS := $(addprefix $(objdir)/, $(TMP_HEADERS))
endif
endif

ifneq ($(LIBRARY_NAME),)
LIB_NAME = lib$(LIBRARY_NAME)
endif

ifneq ($(LIBRARY_SO_VERSION),)
LIB_MAP = $(LIB_NAME).map
LIB_SONAME = $(LIB_NAME).so.$(LIBRARY_SO_VERSION)
LIB_SO = $(LIB_OUTDIR)/$(LIB_NAME).so

LIB_SO_SONAME = $(LIB_SO).$(LIBRARY_SO_VERSION)

ifneq ($(LIBRARY_VERSION),)
LIB_SO_REAL = $(LIB_SO_SONAME).$(LIBRARY_VERSION)
else
$(error LIBRARY_VERSION not set)
endif

TARGET_LIBS = $(LIB_SO_REAL)
TARGET_LINKS = $(LIB_SO_SONAME) $(LIB_SO)
endif

ifneq ($(LIB_NAME),)
LIB_AR = $(LIB_OUTDIR)/$(LIB_NAME).a
LIB_AR_UNSCOPED = $(objdir)/$(LIB_NAME)_unscoped.o
LIB_AR_ALL = $(objdir)/$(LIB_NAME)_all.o
TARGET_LIBS += $(LIB_AR)
endif

ifneq ($(EXTRA_TARGETS),)
EXTRA_TARGETS_CLEAN = $(EXTRA_TARGETS:=-clean)
EXTRA_TARGETS_CLOBBER = $(EXTRA_TARGETS:=-clobber)
endif

PMEMLOG_PRIV_OBJ=$(LIB_OUTDIR)/libpmemlog/libpmemlog_unscoped.o
PMEMBLK_PRIV_OBJ=$(LIB_OUTDIR)/libpmemblk/libpmemblk_unscoped.o

ifneq ($(LIBPMEMLOG_PRIV_FUNCS),)
OBJS += pmemlog_priv_funcs.o
endif

ifneq ($(LIBPMEMBLK_PRIV_FUNCS),)
OBJS += pmemblk_priv_funcs.o
endif

MAKEFILE_DEPS=../Makefile.inc Makefile $(TOP)/src/common.inc

all: $(objdir) $(LIB_OUTDIR) $(EXTRA_TARGETS) $(LIB_AR) $(LIB_SO_SONAME) $(LIB_SO_REAL) $(LIB_SO) $(TMP_HEADERS)

$(objdir) $(LIB_OUTDIR):
	$(MKDIR) -p $@

$(LIB_SO_REAL): $(OBJS) $(EXTRA_OBJS) $(LIB_MAP) $(MAKEFILE_DEPS)
	$(CC) $(LDFLAGS) -shared -Wl,--version-script=$(LIB_MAP),-soname,$(LIB_SONAME) -o $@ $(OBJS) $(EXTRA_OBJS) $(LIBS)

$(LIB_SO_SONAME): $(LIB_SO_REAL) $(MAKEFILE_DEPS)
	$(LN) -sf $(shell basename $<) $@

$(LIB_SO): $(LIB_SO_SONAME) $(MAKEFILE_DEPS)
	$(LN) -sf $(shell basename $<) $@

$(LIB_AR_UNSCOPED): $(OBJS) $(EXTRA_OBJS) $(MAKEFILE_DEPS)
	$(LD) -o $@ -r $(OBJS) $(EXTRA_OBJS)

ifeq ($(LIB_MAP),)
$(LIB_AR_ALL): $(LIB_AR_UNSCOPED) $(MAKEFILE_DEPS)
	$(OBJCOPY) $< $@
else
$(LIB_AR_ALL): $(LIB_AR_UNSCOPED) $(LIB_MAP) $(MAKEFILE_DEPS)
	$(OBJCOPY) --localize-hidden `sed -n 's/^	*\([a-zA-Z0-9_]*\);$$/-G \1/p' $(LIB_MAP)` $< $@
endif

$(LIB_AR): $(LIB_AR_ALL) $(MAKEFILE_DEPS)
	$(AR) rv $@ $(LIB_AR_ALL)

$(PMEMBLK_PRIV_OBJ):
	$(MAKE) -C $(LIBSDIR) libpmemblk

install: all
ifneq ($(LIBRARY_NAME),)
	$(INSTALL) -d $(LIBS_DESTDIR)
	$(INSTALL) -p -m 0755 $(TARGET_LIBS) $(LIBS_DESTDIR)
	$(CP) -d $(TARGET_LINKS) $(LIBS_DESTDIR)
endif

uninstall:
ifneq ($(LIBRARY_NAME),)
	$(foreach f, $(TARGET_LIBS), $(RM) $(LIBS_DESTDIR)/$(notdir $(f)))
	$(foreach f, $(TARGET_LINKS), $(RM) $(LIBS_DESTDIR)/$(notdir $(f)))
endif

clean: $(EXTRA_TARGETS_CLEAN)
ifneq ($(LIBRARY_NAME),)
	$(RM) $(OBJS) $(TMP_HEADERS)
	$(RM) $(LIB_AR_ALL) $(LIB_AR_UNSCOPED)
endif

clobber: clean $(EXTRA_TARGETS_CLOBBER)
ifneq ($(LIBRARY_NAME),)
	$(RM) $(LIB_AR) $(LIB_SO_SONAME) $(LIB_SO_REAL) $(LIB_SO)
	$(RM) -r $(objdir)/.deps
endif

$(eval $(cstyle-rule))

$(objdir)/%.o: %.c $(MAKEFILE_DEPS)
	$(call check-cstyle, $<)
	@mkdir -p $(objdir)/.deps
	$(CC) -MD -c -o $@ $(CFLAGS) $(INCS) -fPIC $(call coverage-path, $<)
	$(call check-os, $@, $<)
	$(create-deps)

sparse:
	$(if $(SOURCE), $(sparse-c))

$(objdir)/%.htmp: %.h
	$(call check-cstyle, $<, $@)

.PHONY: all clean clobber install uninstall cstyle

-include $(objdir)/.deps/*.P