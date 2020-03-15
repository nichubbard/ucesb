# -*- Makefile -*-

$(GENDIR)/gen:
	@echo " GENLINK $@"
	@mkdir -p $(dir $@)
	@ln -s . $(GENDIR)/gen
