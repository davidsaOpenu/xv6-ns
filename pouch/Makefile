TOP_SRCDIR = ..

UPROGS= _pouch_start


all: $(UPROGS)

ULIB = $(TOP_SRCDIR)/ulib.o $(TOP_SRCDIR)/usys.o $(TOP_SRCDIR)/printf.o $(TOP_SRCDIR)/umalloc.o

# Cancel the .c -> output implicit rule
%: %.c

%.o: %.c
	$(CC) $(CFLAGS) -I$(TOP_SRCDIR)/ -c -o $@ $^

$(UPROGS): pouch_start.o
	$(LD) $(LDFLAGS) -N -Ttext 0 -o $@ $^ $(ULIB)

clean:
	rm -f $(UPROGS) *.asm *.o *.sym *.d
	rm -f ../$(UPROGS)
