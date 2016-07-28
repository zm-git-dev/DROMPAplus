CC = g++
CFLAGS += -std=c++11 -O2
#WFLAGS += -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Werror -Wno-unused # Warninig
LIBS += -lboost_program_options -lboost_system -lboost_filesystem -lboost_iostreams
LIBS_DP += -lz -lgsl -lgslcblas -lboost_system -lboost_thread
TARGET = alglib.o parse2wig+ drompa+ gtf2refFlat compare_bed2tss peak_occurance multibed2gene
ALGLBDIR = alglib-3.10.0/src

ifdef DEBUG
CFLAGS += -DDEBUG
endif

OBJS_UTIL = readdata.o util.o
OBJS_ALGLIB = alglib.o specialfunctions.o ap.o alglibinternal.o
OBJS_GTF = gtf2refFlat.o
OBJS_COM = compare_bed2tss.o gene_bed.o
OBJS_PO = peak_occurance.o gene_bed.o
OBJS_MG = multibed2gene.o gene_bed.o
OBJS_PW = pw_main.o pw_readmapfile.o pw_makefile.o pw_gc.o pw_shiftprofile.o statistics.o
OBJS_DD = dd_main.o dd_readfile.o

HEADS_UTIL = util.h readdata.h macro.h seq.h
HEADS_PW = pw_gv.h pw_readmapfile.h statistics.h $(HEADS_UTIL)
HEADS_DD = dd_gv.h dd_readfile.h $(HEADS_UTIL)

.PHONY: all

all: $(TARGET)

gtf2refFlat: $(OBJS_GTF) $(OBJS_UTIL)
	$(CC) -o $@ $^ $(LIBS)
compare_bed2tss: $(OBJS_COM) $(OBJS_UTIL)
	$(CC) -o $@ $^ $(LIBS)
peak_occurance: $(OBJS_PO) $(OBJS_UTIL)
	$(CC) -o $@ $^ $(OBJS_ALGLIB) $(LIBS)
multibed2gene: $(OBJS_MG) $(OBJS_UTIL)
	$(CC) -o $@ $^ $(LIBS)
parse2wig+: $(OBJS_PW) $(OBJS_UTIL)
	$(CC) -o $@ $^ $(OBJS_ALGLIB) $(LIBS) $(LIBS_DP)

drompa+: $(OBJS_DD) $(OBJS_UTIL)
	$(CC) -o $@ $^ $(LIBS) $(LIBS_DP)

alglib.o: alglib.cpp
	$(CC) -c $< $(ALGLBDIR)/specialfunctions.cpp $(ALGLBDIR)/ap.cpp $(ALGLBDIR)/alglibinternal.cpp $(CFLAGS)
.cpp.o:
	$(CC) -c $< $(CFLAGS) $(WFLAGS)

clean:
	rm gtf2refFlat.o $(OBJS_COM) peak_occurance.o multibed2gene.o $(OBJS_PW) $(OBJS_DD) $(OBJS_UTIL) $(TARGET)

alglib.o: Makefile alglib.h
dd_main.o: dd_opt.h
pw_main.o: pw_makefile.h pw_gc.h
pw_readmapfile.o: pw_shiftprofile.h
pw_makefile.o: pw_makefile.h
pw_gc.o: pw_gc.h
pw_shiftprofile.o: Makefile pw_shiftprofile_p.h pw_shiftprofile.h
$(OBJS_UTIL): Makefile $(HEADS_UTIL)
$(OBJS_PW): Makefile $(HEADS_PW)
$(OBJS_DD): Makefile $(HEADS_DD)
$(OBJS_GTF): Makefile $(HEADS_UTIL)
$(OBJS_COM): Makefile $(HEADS_UTIL) gene_bed.h
$(OBJS_PO):  Makefile $(HEADS_UTIL) gene_bed.h
$(OBJS_MG):  Makefile $(HEADS_UTIL) gene_bed.h
