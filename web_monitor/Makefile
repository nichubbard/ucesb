all: web_monitor

.PHONY: web_monitor
web_monitor:
	$(MAKE) -C .. USE_INPUTFILTER=1 NO_CERNLIB=1 NO_YACC_WERROR=1 web_monitor

.PHONY: clean
clean:
	$(MAKE) -C .. -j 10 clean

