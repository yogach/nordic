﻿KERN_DIR = /work/projects/linux-3.4.2   #linux内核目录



all:
	
	make -C $(KERN_DIR) M=`pwd` modules 


clean:
	
	make -C $(KERN_DIR) M=`pwd` modules clean
	
	rm -rf modules.order



obj-m	+= at24cxx_dev.o

obj-m	+= at24cxx_drv.o

