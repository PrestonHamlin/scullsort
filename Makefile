# Comment/uncomment the following line to disable/enable debugging
#DEBUG = y

# Add your debugging flag (or not) to EXTRA_CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DSCULL_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)
#EXTRA_CFLAGS += -Wall -Wextra -Wconversion -Wshadow
EXTRA_CFLAGS += -I$(LDDINC)

ifneq ($(KERNELRELEASE),)
# call from kernel build system

scull-objs := main.o pipe.o access.o sort.o

obj-m	:= scull.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) LDDINC=$(PWD)/../include modules

endif



tests: sculltest writestuff readstuff nonblock

sculltest: sculltest.c
	gcc -Wall sculltest.c -o sculltest

writestuff: writestuff.c
	gcc -Wall writestuff.c -o writestuff

readstuff: readstuff.c
	gcc -Wall readstuff.c -o readstuff

nonblock: nonblock.c
	gcc -Wall nonblock.c -o nonblock

#writemore: writemore.c
#	gcc -Wall writemore.c -o writemore

#readmore: readmore.c
#	gcc -Wall readmore.c -o readmore









clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions Module.symvers modules.order sculltest writestuff readstuff nonblock

depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
