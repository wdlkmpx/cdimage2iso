include ../../config.mk

LIB_STATIC = libz.a

# ls *.c | sed "s%\.c%.o%g" | tr "\n" " " >> Makefile
OBJS = adler32.o inffast.o inflate.o inftrees.o zutil.o uncompr.o deflate.o trees.o compress.o

#CPPFLAGS += -I..
CFLAGS += -I.. -I../..
LIBS   += 

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
