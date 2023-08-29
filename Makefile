FIB_K = 500

CONFIG_MODULE_SIG = n
TARGET_MODULE := fibdrv_new

obj-m += $(TARGET_MODULE).o
$(TARGET_MODULE)-objs := fibdrv.o bn.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied

all: $(GIT_HOOKS) client
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) client out *png scripts/data.txt
load:
	sudo insmod $(TARGET_MODULE).ko
unload:
	sudo rmmod $(TARGET_MODULE) || true >/dev/null

# compiler client with set its MAX_FIB_K to FIB_K
client: client.c
	$(CC) -o $@ $^ -DMAX_FIB_K=$(FIB_K)

time: clean all
	sh ./myperf.sh

PRINTF = env printf
PASS_COLOR = \e[32;01m
FAIL_COLOR = \e[31;01m
NO_COLOR = \e[0m
pass = $(PRINTF) "$(PASS_COLOR)$1 Passed [-]$(NO_COLOR)\n"
fail = $(PRINTF) "$(FAIL_COLOR)$1 Failed [-]$(NO_COLOR)\n"
fib_err = $(PRINTF) "$(FAIL_COLOR)$1 FIB_K must be greater than 500 [-]$(NO_COLOR)\n"

check: clean all
	$(MAKE) unload
	$(MAKE) load
	sudo ./client > out
	$(MAKE) unload
	@if [ $(FIB_K) -gt 500 ]; then \
		scripts/verify.py -k $(FIB_K); \
	else \
		diff -u out scripts/expected.txt && $(call pass) || $(call fail); \
	fi
