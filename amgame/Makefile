NAME := amgame
SRCS := $(shell find -L ./src/ -name "*.c")
export TASK    := L0
export AM_HOME := /home/fasia/workspace/manual/projs/nexus-am
ifeq ($(ARCH),)
export ARCH := x86-qemu
endif

include $(AM_HOME)/Makefile.app
include ../Makefile.lab
image: git
