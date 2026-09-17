# Makefile fallback: builds core, CLI, tests and plugins without CMake.
# The Qt GUI requires CMake + Qt (see README.md).

CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -fPIC -D_POSIX_C_SOURCE=200809L
LDLIBS   = -lm -ldl -lpthread

CORE_SRC = \
	core/src/matrix.c \
	core/src/sys.c \
	core/src/netlist.c \
	core/src/registry.c \
	core/src/builtin.c \
	core/src/results.c \
	core/src/solver_dc.c \
	core/src/solver_ac.c \
	core/src/solver_tr.c

BUILD = build

.PHONY: all core cli tests plugins demo clean check

all: core cli tests plugins

core: $(BUILD)/libelsim-core.so

$(BUILD)/libelsim-core.so: $(CORE_SRC)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -Icore/include -shared $^ -o $@ $(LDLIBS)

cli: $(BUILD)/elsim

$(BUILD)/elsim: core/cli/main.c $(BUILD)/libelsim-core.so
	$(CC) $(CFLAGS) -Icore/include $< -L$(BUILD) -lelsim-core -o $@ $(LDLIBS) -Wl,-rpath,'$$ORIGIN'

tests: $(BUILD)/elsim_tests

$(BUILD)/elsim_tests: core/tests/test_all.c $(BUILD)/libelsim-core.so
	$(CC) $(CFLAGS) -Icore/include $< -L$(BUILD) -lelsim-core -o $@ $(LDLIBS) -Wl,-rpath,'$$ORIGIN'

plugins: $(BUILD)/plugins/led.so $(BUILD)/plugins/switch.so

$(BUILD)/plugins/%.so: plugins/%.c $(BUILD)/libelsim-core.so
	@mkdir -p $(BUILD)/plugins
	$(CC) $(CFLAGS) -DEC_PLUGIN_BUILD -Icore/include $< -L$(BUILD) -lelsim-core -shared -o $@ -Wl,-rpath,'$$ORIGIN/..'

check: tests
	$(BUILD)/elsim_tests

demo: cli plugins
	cd $(BUILD) && ./elsim ../examples/divider.cir && \
	./elsim ../examples/rc_tran.cir && \
	./elsim ../examples/rc_ac.cir && \
	./elsim ../examples/diode.cir && \
	./elsim ../examples/led_demo.cir

clean:
	rm -rf $(BUILD)
