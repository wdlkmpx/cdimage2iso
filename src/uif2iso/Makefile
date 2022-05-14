include ../../config.mk

LIB_STATIC = uif2iso.a

# ls *.c | sed "s%\.c%.o%g" | tr "\n" " " >> Makefile
OBJS = 3way.o blowfish.o des.o dunno.o gost.o idea.o loki91.o rc5.o seal.o uif2iso.o

#CPPFLAGS += -I..
#  uif2iso requires zlib and 7z
CFLAGS += -I.. -I../.. -I../../0lib -I../../0lib/7z $(ZLIB_CFLAGS)
LIBS   += $(ZLIB_LIBS)

all: $(LIB_STATIC)

$(LIB_STATIC): $(OBJS)
	$(AR) rcs $(LIB_STATIC) $(OBJS)

strip:

distclean: clean

clean:
	rm -f *.o *~ *.a *.so

install:

install-strip: install

uninstall:
	rm -f $(DESTDIR)$(libdir)/$(LIB_STATIC)*
