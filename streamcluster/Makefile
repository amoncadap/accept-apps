RUNARGS := 10 20 64 8192 8192 1000 none output.txt 12  # simmedium
TESTARGS := 10 20 128 16384 16384 1000 none output.txt 12  # simlarge
#TESTARGS := 10 20 128 1000000 200000 5000 none output.txt 12  # native

LDFLAGS := -lpthread
CFLAGS := -DENABLE_THREADS
CLEANMETOO := output.txt

ACCEPT_DIR := ../../accept
include $(ACCEPT_DIR)/accept.mk
