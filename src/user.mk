include ../config.mk

EXTRA_CFLAGS := $(filter-out -Wframe-larger-than=%,$(EXTRA_CFLAGS))

LCEC_CONF_OBJS = \
	lcrest_main.o \
	lcrest_conf.o \
	lcrest_hal.o \
	lcrest_rest.o \
	lcrest_json.o \

.PHONY: all clean install

all: lcrest

install: lcrest
	mkdir -p $(DESTDIR)$(EMC2_HOME)/bin
	cp lcrest $(DESTDIR)$(EMC2_HOME)/bin/

lcrest: $(LCEC_CONF_OBJS)
	$(CC) -o $@ $(LCEC_CONF_OBJS) -Wl,-rpath,$(LIBDIR) -L$(LIBDIR) -llinuxcnchal -lexpat -lulfius -ljansson

%.o: %.c
	$(CC) -o $@ $(EXTRA_CFLAGS) -URTAPI -U__MODULE__ -DULAPI -Os -c $<

