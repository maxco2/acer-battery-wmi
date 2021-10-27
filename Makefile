obj-m := acer_battery_wmi.o

KVER  ?= $(shell uname -r)

KDIR ?= /lib/modules/$(KVER)/build
MDIR ?= /lib/modules/$(KVER)/drivers/platform/x86

PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	install -d $(MDIR)
	install -m 644 -c $(obj-m:.o=.ko) $(MDIR)
	depmod -a

