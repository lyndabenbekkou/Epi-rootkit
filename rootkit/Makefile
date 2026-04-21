KERNELDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m += epirootkit.o

epirootkit-objs := epirootkit_main.o network.o authentification.o rootkitShell.o load.o hide.o persistence.o encrypt.o

all:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean

load: all
	sudo insmod epirootkit.ko

unload:
	sudo rmmod epirootkit
	dmesg | tail