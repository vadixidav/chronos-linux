obj-y += set_scheduler.o
obj-y += chronos_seg.o
obj-y += chronos_mutex.o
obj-y += chronos_proc.o
ifeq ($(CONFIG_CHRONOS),y)
obj-m += fifo_ra.o
obj-m += rma.o
obj-m += edf.o
obj-m += hvdf.o
obj-m += rma_icpp.o
obj-m += rma_ocpp.o
obj-m += gfifo.o
obj-m += grma.o
obj-m += abort_shmem.o
endif
