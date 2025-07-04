# The goal of this Makefile is to be a single entry point for non-phpt tests.

PHP ?= $(CURDIR)/../../sapi/cli/php
# "Windows NT", "Linux", "FreeBSD", "Darwin", ...
OS ?= $(shell $(PHP) -r "echo php_uname('s');")

INT_SIZE ?= $(shell $(PHP) -r "echo PHP_INT_SIZE;")
ifndef CPU
	CPU ?= $(shell $(PHP) -r "echo php_uname('m');")
ifeq ($(filter $(CPU),AMD64 amd64),$(CPU))
	CPU := x86_64
else ifeq ($(filter $(CPU),i386 i686),$(CPU))
	CPU := x86
endif
ifeq ($(CPU)_$(INT_SIZE),x86_64_4)
	CPU := x86
endif
endif # ifndef CPU

ZTS ?= $(shell $(PHP) -r "echo (int)PHP_ZTS;")
ASAN ?= 0
MSAN ?= 0

all: print-env test

print-env:
	@echo "PHP:  $(PHP)"
	@echo "OS:   $(OS)"
	@echo "ZTS:  $(ZTS)"
	@echo "ASAN: $(ASAN)"
	@echo "MSAN: $(MSAN)"

test:
ifeq ($(filter $(OS),Linux Darwin FreeBSD),$(OS))
ifeq ($(ZTS),1)
	$(MAKE) -f Makefile.tls PHP=$(PHP) OS=$(OS) ZTS=$(ZTS) ASAN=$(ASAN) MSAN=$(MSAN) CPU=$(CPU)
endif
endif
