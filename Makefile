builddir = build
srcdir = .
objects = $(builddir)/main.o
options = -lm -lncurses -ltinfo
out = $(builddir)/folderTail

all : $(out)

$(out) : $(objects)
	cc -o $(out) $(objects) $(options)
$(builddir)/main.o : main.c Makefile
	cc -o $(builddir)/main.o -c $(srcdir)/main.c

install : $(out)
	cp $(out) /usr/local/bin

clean :
	-rm build/*
uninstall :
	-rm /usr/local/bin/folderTail
