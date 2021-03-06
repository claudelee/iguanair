#
# Template for building a lirc userspace driver out of tree.
# Requires that lirc is installed in system locations, in
# particular that the /usr/lib[64]/pkgconfig/lirc-driver.pc
# is in place (/usr/local/lib/pkgconfig/... is also OK).
#
# The doc installation needs to be completed, typically as root
# using something like
#
#     # cd $(pkg-config --variable=plugindocs lirc-driver); make
#
# This updates the list of installed plugins in /var/lib/lirc,
# which is included from the main docs.


driver          = iguanair

all:  $(driver).so

# OS X requires an additional linker flag  TODO: is it really OS X or clang?
LDFLAGS         += -liguanaIR -L../usb_ir/build/result
ifeq ("$(shell uname)", "Darwin")
  LDFLAGS       += -Wl,-undefined,dynamic_lookup
endif

KERNLIRC=-DHAVE_KERNEL_LIRC_H
ifeq ("$(wildcard /usr/include/linux/lirc.h)","")
  KERNLIRC=
endif

# check with leamas how -DHAVE_KERNEL_LIRC_H should be handled
CFLAGS          += -I../usb_ir ${KERNLIRC}
MODPROBE_CONF   = 60-blacklist-kernel-iguanair.conf


CFLAGS          += $(shell pkg-config --cflags lirc-driver)
LDFLAGS         += $(shell pkg-config --libs lirc-driver)
PLUGINDIR       ?= $(shell pkg-config --variable=plugindir lirc-driver)
CONFIGDIR       ?= $(shell pkg-config --variable=configdir lirc-driver)
PLUGINDOCS      ?= $(shell pkg-config --variable=plugindocs lirc-driver)

$(driver).o: $(driver).c

$(driver).so: $(driver).o
	gcc --shared -fpic -o $@ $< $(LDFLAGS)

install: $(driver).so
	install -D $< $(DESTDIR)$(PLUGINDIR)/$<
	install -Dm 644 $(driver).conf $(DESTDIR)$(CONFIGDIR)/$(driver).conf
	install -Dm 644 $(driver).html $(DESTDIR)$(PLUGINDOCS)/$(driver).html
	install -Dm 644 $(MODPROBE_CONF) \
	    $(DESTDIR)/etc/modprobe.d/$(MODPROBE_CONF)

clean:
	rm -f *.o *.so
