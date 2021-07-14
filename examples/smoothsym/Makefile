CHARM_HOME ?= $(HOME)/charm-paratreet
BASE_PATH=$(shell realpath "$(shell pwd)/../..")
PARATREET_PATH = $(BASE_PATH)/src
STRUCTURE_PATH = $(BASE_PATH)/utility/structures
OPTS = -g -Ofast -I$(STRUCTURE_PATH) -I$(PARATREET_PATH) -DCOUNT_INTERACTIONS=0 -DDEBUG=0 $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L$(PARATREET_PATH) -lparatreet

all: SmoothSym
VISITORS = DensityVisitor.h

Main.decl.h: Main.ci
	$(CHARMC) $<

SmoothSym: Main.decl.h Main.o SmoothSym.o
	$(CHARMC) -language charm++ -module CommonLBs -o SmoothSym SmoothSym.o Main.o $(LD_LIBS)

SmoothSym.o: SmoothSym.C SPHUtils.h NeighborListCollector.h Main.decl.h
	$(CHARMC) -c $<

Main.o: Main.C CentroidData.h $(VISITORS) $(OTHERS) Main.decl.h
	$(CHARMC) -c $<

clean:
	rm -f *.decl.h *.def.h conv-host *.o Gravity SPH Collision charmrun
