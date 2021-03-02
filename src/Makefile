CHARM_HOME ?= $(HOME)/charm-paratreet
STRUCTURE_PATH = ../utility/structures
OPTS = -g -Ofast -I$(STRUCTURE_PATH) -DCOUNT_INTERACTIONS=0 -DCMK_LB_USER_DATA -DDEBUG=0 $(MAKE_OPTS)
CHARMC = $(CHARM_HOME)/bin/charmc $(OPTS)

OBJS = Paratreet.o Reader.o Writer.o Particle.o BoundingBox.o Decomposition.o Modularization.o TreeSpec.o
TIPSY_OBJS = NChilReader.o SS.o TipsyFile.o TipsyReader.o hilbert.o

UTILITY_HEADERS = common.h Utility.h $(STRUCTURE_PATH)/Vector3D.h $(STRUCTURE_PATH)/SFC.h
CORE_HEADERS = BoundingBox.h BufferedVec.h CentroidData.h MultiData.h Node.h NodeWrapper.h ParticleComp.h ParticleMsg.h Splitter.h
IMPL_HEADERS = CacheManager.h Configuration.h Driver.h Partition.h Reader.h Resumer.h Splitter.h Subtree.h Traverser.h TreeCanopy.h

all: lib

lib: $(OBJS) PrefixLB DistributedPrefixLB
	ar x $(STRUCTURE_PATH)/libTipsy.a
	ar cr libparatreet.a $(OBJS) $(TIPSY_OBJS)
	rm -f $(TIPSY_OBJS)
	ranlib libparatreet.a

PrefixLB: PrefixLB.C PrefixLB.decl.h
	$(CHARMC) -o libmodulePrefixLB.a PrefixLB.C

PrefixLB.decl.h: PrefixLB.ci
	$(CHARMC) PrefixLB.ci

DistributedPrefixLB: DistributedPrefixLB.C DistributedPrefixLB.decl.h
	$(CHARMC) -o libmoduleDistributedPrefixLB.a DistributedPrefixLB.C

DistributedPrefixLB.decl.h: DistributedPrefixLB.ci
	$(CHARMC) DistributedPrefixLB.ci

paratreet.decl.h: paratreet.ci
	$(CHARMC) $<

Paratreet.o: $(UTILITY_HEADERS) $(CORE_HEADERS) $(IMPL_HEADERS) paratreet.decl.h
	$(CHARMC) -c Paratreet.C

%.o: %.C %.h paratreet.decl.h
	$(CHARMC) -c $<

clean:
	rm -f *.decl.h *.def.h conv-host *.o libmodulePrefixLB.a libmoduleDistributedPrefixLB.a libparatreet.a charmrun
