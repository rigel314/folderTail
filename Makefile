builddir = build
srcdir = .
objects = $(builddir)/main.o
options = -lm -lncurses
out = $(builddir)/folderTail

all : $(out)

$(out) : $(objects)
	cc -o $(out) $(objects) $(options)
$(builddir)/main.o :
	cc -o $(builddir)/main.o -c $(srcdir)/main.c

install : $(out)
	cp $(out) /usr/local/bin
