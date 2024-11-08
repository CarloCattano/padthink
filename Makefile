lib.name = padthink

class.sources = padthink.c

datafiles = padthink-help.pd

# include Makefile.pdlibbuilder from submodule directory 'pd-lib-builder'
PDLIBBUILDER_DIR=../pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder


debug: all
	@echo "Running pd with debug mode"
	@gdb -ex run --args pd padthink-help.pd -j -nrt
