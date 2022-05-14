include ../config.mk

CC ?= cc
STRIP ?= strip

APP  = ccd2cue$(EXE)
# ls *.c | sed "s%\.c%.o%g" | tr "\n" " " >> Makefile
OBJS = ccd2cue.o ccd.o convert.o cue.o util.o

#CPPFLAGS += -I..
CFLAGS += -I.. -I../0lib
LIBS   +=

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(APP) $(LDFLAGS) $(LIBS)

strip: $(APP)
	$(STRIP) $(APP)

distclean: clean

clean:
	rm -f *.a *.o *.so *~ $(APP)

install: $(APP)
	mkdir -p $(DESTDIR)$(bindir)
	install -c $(APP) $(DESTDIR)$(bindir)

install-strip: strip install

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(APP)

