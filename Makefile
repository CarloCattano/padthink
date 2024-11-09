lib.name = padthink

class.sources = padthink.c

datafiles = padthink-help.pd

EXT_DEST_DIR = $(HOME)/Documents/Pd/externals

# include Makefile.pdlibbuilder from submodule directory 'pd-lib-builder'
PDLIBBUILDER_DIR=../pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder


debug: all
	@echo "Running pd with debug mode"
	@gdb -ex run --args pd padthink-help.pd -j -nrt


test: all
	@echo "copy plugin to destination"
	@cp $(lib.name).pd_linux $(EXT_DEST_DIR)
	@echo "Running pd with test patch"
	@pd padthink-help.pd -nrt -j
