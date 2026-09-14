# The stack, the shim that sits it on this kernel, and the interface that
# carries its frames. Brought in only when the board is told to have a network
PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../

LWIP := $(PROJ_ROOT)/lib/lwip

CFLAGS += -I$(PROJ_ROOT)/net/lwip
CFLAGS += -I$(LWIP)/src/include

SRC += $(wildcard $(LWIP)/src/core/*.c)
SRC += $(wildcard $(LWIP)/src/core/ipv4/*.c)
SRC += $(wildcard $(LWIP)/src/api/*.c)
SRC += $(LWIP)/src/netif/ethernet.c

SRC += $(PROJ_ROOT)/net/lwip/sys_arch.c
SRC += $(PROJ_ROOT)/net/net_task.c
SRC += $(PROJ_ROOT)/drivers/net/ethernetif.c
