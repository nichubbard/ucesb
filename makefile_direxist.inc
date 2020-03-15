# -*- Makefile -*-

TREE_UCESB_OR_HOME_SRC_DIR = \
  $(realpath $(firstword $(wildcard $(addsuffix /$(1),\
                             $(shell pwd)/.. $(UCESB_BASE_DIR)/.. $(HOME)))))

define CHECK_DIRECTORY_EXIST
DIR_EXISTS:=$(shell test -d '$(1)' && echo exists)
#$$(warning Checking for existence: $(1))
ifeq (,$$(DIR_EXISTS))
$$(error Directory '$(1)' does not exist)
endif
endef

$(eval $(call CHECK_DIRECTORY_EXIST,$(UCESB_BASE_DIR)))

DIR_EXIST_OR_DIE = \
  $(if $(shell test -d '$(1)' && echo Found),$(1),$(error Directory $(1) '$(2)' could not be found))

TREE_UCESB_OR_HOME_SRC_DIR_OR_DIE = \
  $(call DIR_EXIST_OR_DIE,$(call TREE_UCESB_OR_HOME_SRC_DIR,$(1)),$(1))
