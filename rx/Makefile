# Generated automatically from Makefile.in by configure.
#
# A simplified Makefile for use with Mutt.
# 
# Michael Elkins <me@cs.hmc.edu>
# 1/23/97
#
OBJS=	hashrexp.o rx.o rxanal.o rxbasic.o rxbitset.o rxcset.o \
	rxgnucomp.o rxhash.o rxnfa.o rxnode.o rxposix.o rxsimp.o \
	rxspencer.o rxstr.o rxsuper.o rxunfa.o



all: all-no

all-yes: librx.a

all-no: 
install:

librx.a: $(OBJS)
	ar rc librx.a $(OBJS)
	-ranlib librx.a

clean:
	rm -f *.o librx.a *~
