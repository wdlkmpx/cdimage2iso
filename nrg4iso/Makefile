include ../config.mk

CC ?= cc
STRIP ?= strip

APP  = nrg4iso$(EXE)
# ls *.c | sed "s%\.c%.o%g" | tr "\n" " " >> Makefile
OBJS = crc.o iso9660.o main.o nrg.o 

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

