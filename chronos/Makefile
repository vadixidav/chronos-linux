obj-y += set_scheduler.o
obj-y += chronos_seg.o
obj-y += chronos_mutex.o
obj-y += chronos_proc.o
ifeq ($(CONFIG_CHRONOS),y)
obj-m += fifo_ra.o
obj-m += rma.o
obj-m += gfifo.o
obj-m += grma.o
obj-m += abort_shmem.o
endif
