#KERN_DIR ?=/media/newdisk/code/kernel_79 
KERN_DIR ?=/lib/modules/3.13.0-24-generic/build
PROJECT=globalfifo

obj-m += $(PROJECT).o
PWD := $(shell pwd)

default:
	make -C $(KERN_DIR) M=$(PWD)
clean:
	rm -rf *.o *.ko *~core *.order *.symvers .depend .*.cmd *.mod.c *.tmp_versions
