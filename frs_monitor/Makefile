all: frs_monitor

.PHONY: frs_monitor
frs_monitor:
	$(MAKE) -C .. USE_INPUTFILTER=0 NO_CERNLIB=1 NO_YACC_WERROR=1 frs_monitor

.PHONY: clean
clean:
	$(MAKE) -C .. -j 10 clean-unp-frs_monitor

