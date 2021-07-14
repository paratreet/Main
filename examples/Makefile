CHARM_HOME ?= $(HOME)/charm-paratreet
BASE_PATH=$(shell realpath "$(shell pwd)/..")
PARATREET_PATH = $(BASE_PATH)/src
STRUCTURE_PATH = $(BASE_PATH)/utility/structures
OPTS = -g -O0 -I$(STRUCTURE_PATH) -I$(PARATREET_PATH) -DCOUNT_INTERACTIONS=0 -DDEBUG=0 -DHEXADECAPOLE $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)
LD_LIBS = -L$(PARATREET_PATH) -lparatreet

all: Gravity SPH Collision
VISITORS = DensityVisitor.h PressureVisitor.h GravityVisitor.h CollisionVisitor.h
OTHERS = CountManager.h

Main.decl.h: Main.ci
	$(CHARMC) $<

Gravity: Main.decl.h Main.o Gravity.o moments.o
	$(CHARMC) -language charm++ -module CommonLBs -o Gravity Gravity.o moments.o Main.o $(LD_LIBS)

Collision: Main.decl.h Main.o Collision.o moments.o
	$(CHARMC) -language charm++ -module CommonLBs -o Collision Collision.o Main.o moments.o $(LD_LIBS)

SPH: Main.decl.h Main.o SPH.o moments.o
	$(CHARMC) -language charm++ -module CommonLBs -o SPH SPH.o Main.o moments.o $(LD_LIBS)

SmoothSym: Main.decl.h Main.o SmoothSym.o moments.o
	$(CHARMC) -language charm++ -module CommonLBs -o SmoothSym SmoothSym.o Main.o moments.o $(LD_LIBS)

Gravity.o: Gravity.C Main.decl.h
	$(CHARMC) -c $<

Collision.o: Collision.C Main.decl.h
	$(CHARMC) -c $<

SPH.o: SPH.C SPHUtils.h NeighborListCollector.h Main.decl.h
	$(CHARMC) -c $<

SmoothSym.o: SmoothSym.C NeighborListCollectorDen.h Main.decl.h
	$(CHARMC) -c $<

Main.o: Main.C CentroidData.h moments.h MultipoleMoments.h $(VISITORS) $(OTHERS) Main.decl.h
	$(CHARMC) -c $<

moments.o: moments.C
	$(CHARMC) -c $<

test: all
	./charmrun ./Gravity -f $(BASE_PATH)/inputgen/100k.tipsy -d sfc +p3 ++ppn 3 +pemap 1-3 +commap 0 ++local

clean:
	rm -f *.decl.h *.def.h conv-host *.o Gravity SPH Collision charmrun
