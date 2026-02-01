# Makefile for HP Chromebook Snappy Audio Fix
#
# Builds the patched snd_soc_sof_da7219 kernel module with BXT/APL support
#

obj-m += snd-soc-sof_da7219.o
snd-soc-sof_da7219-objs := sof_da7219.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod *.mod.c .*.cmd Module.symvers modules.order

install:
	@echo "Installing module..."
	sudo mkdir -p /lib/modules/$(shell uname -r)/updates
	sudo cp snd-soc-sof_da7219.ko /lib/modules/$(shell uname -r)/updates/
	sudo depmod -a
	@echo "Module installed. Please reboot."

uninstall:
	@echo "Removing module..."
	sudo rm -f /lib/modules/$(shell uname -r)/updates/snd-soc-sof_da7219.ko
	sudo depmod -a
	@echo "Module removed."

.PHONY: all clean install uninstall
