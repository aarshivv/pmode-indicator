KERNEL_BUILD ?= /lib/modules/$(shell uname -r)/build
MODULE := pmode_indicator.ko

.PHONY: all clean verify

all:
	$(MAKE) -C $(KERNEL_BUILD) M=$(CURDIR) modules
	@$(MAKE) --no-print-directory verify

# Fail the build if the module could ever write to the EC.
# Source checks skip comment lines; the nm check covers the built module.
CODE = grep -vE '^\s*(/\*|\*|//)' src/pmode_indicator.c

verify:
	@if $(CODE) | grep -nE 'ec_write|ec_transaction'; then \
		echo "verify: FAIL - write call in source"; exit 1; fi
	@if nm -u $(MODULE) | grep -wE 'ec_write|ec_transaction'; then \
		echo "verify: FAIL - module imports an EC write symbol"; exit 1; fi
	@if $(CODE) | grep -nE '0644|0200|0600|_store|DEVICE_ATTR_RW|DEVICE_ATTR_WO|\.write'; then \
		echo "verify: FAIL - writable attribute in source"; exit 1; fi
	@echo "verify: OK - EC symbols imported: $$(nm -u $(MODULE) | awk '/ ec_/{print $$2}' | tr '\n' ' ')"

clean:
	$(MAKE) -C $(KERNEL_BUILD) M=$(CURDIR) clean
