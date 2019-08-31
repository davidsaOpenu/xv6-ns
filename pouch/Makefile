TOP_SRCDIR = ..

UPROGS=\
	 _pouch_start\


all: $(UPROGS)

ULIB = $(TOP_SRCDIR)/ulib.o $(TOP_SRCDIR)/usys.o $(TOP_SRCDIR)/printf.o $(TOP_SRCDIR)/umalloc.o $(TOP_SRCDIR)/tty.o

# Cancel the .c -> output implicit rule
%: %.c

%.o: %.c
	$(CC) $(CFLAGS) -I$(TOP_SRCDIR)/ -c -o $@ $^

_%: %.o
	$(LD) $(LDFLAGS) -N -Ttext 0 -o $@ $^ $(ULIB) 
	#$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^ $(ULIB)
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym


#$(UPROGS): pouch_start.o tty.o
#	$(LD) $(LDFLAGS) -N -Ttext 0 -o $@ $^ $(ULIB)

clean:
	rm -f $(UPROGS) *.asm *.o *.sym *.d
	rm -f ../$(UPROGS)
