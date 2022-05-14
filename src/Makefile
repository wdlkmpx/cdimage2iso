include ../config.mk

APP  = cdimage2iso
# ls *.c | sed "s%\.c%.o%g" | tr "\n" " " >> Makefile
OBJS = main.o \
	b5i2iso.o \
	bchunk.o \
	bin2iso.o \
	ccd2iso.o \
	cdirip.o  \
	cso2iso.o \
	daxcreator.o \
	dmg2iso.o \
	isz2iso.o \
	mdf2iso.o \
	nrg2iso.o \
	nrg2cue.o \
	pdi2iso.o \
	daa2iso.o

LDFLAGS += 
#CPPFLAGS += -I..
#  zlib: required by cso2iso / daxcreator / uif2iso
#  7z  : required by daa2iso / uif2iso
CFLAGS += -I.. -I../0lib -I../0lib/7z $(ZLIB_CFLAGS)
LIBS   += $(ZLIB_LIBS) $(LWS2_32)

LOCAL_LIBS = uif2iso/uif2iso.a ../0lib/7z/7z.a

all: $(LOCAL_LIBS) $(APP)

../0lib/7z/7z.a:
	$(MAKE) -C ../0lib/7z

uif2iso/uif2iso.a:
	$(MAKE) -C uif2iso

$(APP): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(APP) $(LDFLAGS) $(LOCAL_LIBS) $(LIBS)
#       uif2iso.a must go before -lz and 7z.a otherwise = missing symbols

strip: all
	@if test -f $(APP).exe ; then \
		echo $(STRIP) $(APP).exe; \
		$(STRIP) $(APP).exe; \
	else \
		echo $(STRIP) $(APP); \
		$(STRIP) $(APP); \
	fi;

distclean: clean

clean:
	rm -f *.o *~ $(APP) $(APP).exe \
	../0lib/7z/*.o ../0lib/7z/*.a \
	uif2iso/*.o uif2iso/*.a

install: all
	mkdir -p $(DESTDIR)$(bindir)
	@if test -f $(APP).exe ; then \
		echo install $(APP).exe; \
		install $(APP).exe $(DESTDIR)$(bindir); \
	else \
		echo install $(APP); \
		install $(APP) $(DESTDIR)$(bindir); \
	fi;

install-strip: strip install

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(APP)

