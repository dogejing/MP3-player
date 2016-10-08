ifneq ($(KERNELRELEASE),)

obj-m := mini2440_buttons.o

else
	
KDIR := /home/project/mp3/SDK-MP3/kernel/linux-2.6.29
all:
	make -C $(KDIR) M=$(PWD) modules ARCH=arm CROSS_COMPILE=arm-linux-
clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers app-key

endif

