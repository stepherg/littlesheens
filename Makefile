# Compiler and tools
CC = gcc
AR = ar
ARFLAGS = rcs

# Compiler flags
CFLAGS = -Wall -std=c99 -fno-asynchronous-unwind-tables -ffunction-sections -Wl,--gc-sections -I. -fPIC
# Debug flags (commented out): CFLAGS = -Wall -std=c99 -fno-omit-frame-pointer -fno-inline -fvar-tracking -O0 -g3 -ggdb3 -I. -fPIC
LDFLAGS = -lm -ldl

# Duktape configuration
DUKVERSION = duktape-2.7.0
DUK ?= $(DUKVERSION)
DUK_SRC = $(DUK)/src
DUK_EXTRAS = $(DUK)/extras/print-alert

# Directories
LIB_DIR = lib
OUT_DIR = $(LIB_DIR)
JS_DIR = js
SPEC_DIR = specs

# Source and output files
LIB_SRCS = $(wildcard $(LIB_DIR)/*.c)
LIB_SOS = $(patsubst $(LIB_DIR)/%.c,$(OUT_DIR)/%.so,$(LIB_SRCS))
LIB_SCRIPTS = $(wildcard $(LIB_DIR)/*.js)
SPEC_YAMLS = $(wildcard $(SPEC_DIR)/*.yaml)
SPEC_JSS = $(patsubst $(SPEC_DIR)/%.yaml,$(SPEC_DIR)/%.js,$(SPEC_YAMLS))

# Main targets
EXECUTABLES = demo sheensio driver register_test

# Default target
all: $(DUK) $(LIB_SOS) $(EXECUTABLES)

# --- Duktape Download and Setup ---
$(DUK):
	wget http://duktape.org/$(DUKVERSION).tar.xz
	tar xf $(DUKVERSION).tar.xz

duk: $(DUK)

# --- Library Rules ---
libduktape.a: $(DUK_SRC)/duktape.c $(DUK_EXTRAS)/duk_print_alert.c
	$(CC) $(CFLAGS) -c -I$(DUK_SRC) $(DUK_SRC)/duktape.c
	$(CC) $(CFLAGS) -c -I$(DUK_SRC) -I$(DUK_EXTRAS) $(DUK_EXTRAS)/duk_print_alert.c
	$(AR) $(ARFLAGS) $@ duktape.o duk_print_alert.o

libduktape.so: libduktape.a
	$(CC) $(CFLAGS) -shared -o $@ -Wl,--whole-archive $< -Wl,--no-whole-archive -lm

libmachines.a: machines.c machines_js.c
	$(CC) $(CFLAGS) -c -I$(DUK_SRC) -I$(DUK_EXTRAS) machines.c machines_js.c
	$(AR) $(ARFLAGS) $@ machines.o machines_js.o

libmachines.so: libmachines.a
	$(CC) $(CFLAGS) -shared -o $@ -Wl,--whole-archive $< -Wl,--no-whole-archive

$(OUT_DIR)/%.so: $(LIB_DIR)/%.c libduktape.so
	$(CC) $(CFLAGS) -I$(DUK_SRC) -shared -L. -l:libduktape.so $< -o $@

# --- JavaScript and Embedded Files ---
machines.js: $(JS_DIR)/match.js $(JS_DIR)/sandbox.js $(JS_DIR)/step.js $(JS_DIR)/prof.js driver.js $(LIB_SCRIPTS)
	cat $^ > $@

machines_js.c: machines.js
	minify $< > machines.js.terminated
	truncate -s +1 machines.js.terminated
	./embedstr.sh mach_machines_js machines.js.terminated $@
	rm machines.js.terminated

$(SPEC_DIR)/%.js: $(SPEC_DIR)/%.yaml
	cat $< | yaml2json | jq . > $@

# --- Executable Rules ---
sheensio: sheensio.c libmachines.a libduktape.a
	$(CC) $(CFLAGS) -I$(DUK_SRC) $< -L. -l:libmachines.a -lduktape $(LDFLAGS) -o $@

demo: demo.c util.c libmachines.a libduktape.a $(SPEC_DIR)/double.js
	$(CC) $(CFLAGS) -I$(DUK_SRC) demo.c util.c -L. -l:libmachines.a -lduktape $(LDFLAGS) -o $@

register_test: register_test.c util.c libmachines.so libduktape.so $(SPEC_DIR)/double.js
	$(CC) $(CFLAGS) -I$(DUK_SRC) $< util.c -L. -l:libmachines.a -lduktape $(LDFLAGS) -o $@

driver: driver.c util.c libmachines.a libduktape.a
	$(CC) $(CFLAGS) -I$(DUK_SRC) $< util.c -L. -l:libmachines.a -lduktape $(LDFLAGS) -o $@

# --- Test Rules ---
matchtest: driver match_test.js
	./driver match_test.js | tee match_test.results.json | jq -r '.[]|select(.happy == false)|"\(.n): \(.case.title); wanted: \(.case.w) got: \(.got)"'
	cat match_test.results.json | jq -r '.[]|"\(.n): elapsed \(.bench.elapsed)ms (\(.bench.rounds) rounds) \(.case.title)"'

test: demo sheensio matchtest
	valgrind --leak-check=full --error-exitcode=1 ./demo

# --- Utility Rules ---
nodejs:
	./nodemodify.sh

clean:
	rm -f *.a *.o *.so machines.js machines_js.c $(EXECUTABLES) $(LIB_SOS)

distclean: clean
	rm -f $(DUKVERSION).tar.xz
	rm -rf $(DUKVERSION)

tags:
	etags *.c *.h $(JS_DIR)/*.js demo.js driver.js

# --- Phony Targets ---
.PHONY: all duk clean distclean test matchtest nodejs tags