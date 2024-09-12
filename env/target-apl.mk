# Cycc/Cympile - Shared Build Scripts for Make
# Copyright (C) 2013-2020  Jay Freeman (saurik)

# Zero Clause BSD license {{{
#
# Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
# }}}


pre := lib
dll := dylib
lib := a
exe := 

meson := darwin

lflags += -Wl,-ObjC
lflags += -Wl,-dead_strip

# libtool
qflags += -DPIC
qflags += -fPIC
lflags += -fPIC

signature := /_CodeSignature/CodeResources

isysroot :=  $(shell xcrun --sdk $(sdk) --show-sdk-path)
more += -isysroot $(isysroot)

define _
more/$(1) := -arch $(1)
endef
$(each)

ifeq ($(filter crossndk,$(debug)),)
include $(pwd)/kit-default.mk
else

define _
more/$(1) += -target $(host/$(1))19
endef
$(each)

# XXX: needed for ___isPlatformVersionAtLeast
resource := $(shell clang -print-resource-dir)
lflags += $(resource)/lib/darwin/libclang_rt.$(runtime).a

# XXX: this is obsolete but feels right
#more += -B$(dir $(clang))
more += -fno-strict-return
include $(pwd)/kit-android.mk

include $(pwd)/target-lld.mk

endif

include $(pwd)/target-cxx.mk
lflags += -lc++abi

objc := $(cc)

define _
strip/$(1) := strip
windres/$(1) := false
endef
$(each)

lflags += -lresolv
lflags += -framework Security
