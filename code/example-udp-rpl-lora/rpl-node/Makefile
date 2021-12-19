CONTIKI_PROJECT = rpl-node
all: $(CONTIKI_PROJECT)
CONTIKI = $(HOME)/contiki-ng

MODULES += os/services/loramacv2
MODULES += os/services/orchestra

MAKE_MAC = MAKE_MAC_TSCH
MAKE_NET = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_RPL_LITE

include $(CONTIKI)/Makefile.include
