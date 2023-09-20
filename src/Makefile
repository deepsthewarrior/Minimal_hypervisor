hyper-objs := main.o utils.o
obj-m += hyper.o
build:
	make -C /lib/modules/`uname -r`/build M=`pwd` modules EXTRA_CFLAGS="-g -DDEBUG"
clean:
	make -C /lib/modules/`uname -r`/build M=`pwd` clean
