/* Copyright(c)  Ryuichiro Nakato <rnakato@iam.u-tokyo.ac.jp>
 * All rights reserved.
 */
#ifndef _DD_DRAW_H_
#define _DD_DRAW_H_

#include "dd_class.hpp"
#include "dd_readfile.hpp"
#include "WigStats.hpp"
#include "color.hpp"

class ChrArray {
public:
  int32_t binsize;
  int32_t nbin;
  WigArray array;
  WigStats stats;
  int32_t totalreadnum;
  std::unordered_map<std::string, int32_t> totalreadnum_chr;

  ChrArray(){}
  ChrArray(const DROMPA::Global &p,
	   const std::pair<const std::string, SampleInfo> &x,
	   const chrsize &chr):
    binsize(x.second.getbinsize()), nbin(chr.getlen()/binsize +1),
    array(loadWigData(x.first, x.second, chr)),
    stats(nbin, p.thre.pthre_inter),
    totalreadnum(x.second.gettotalreadnum()),
    totalreadnum_chr(x.second.gettotalreadnum_chr())
  {
    clock_t t1,t2;
    t1 = clock();
    if(p.getSmoothing()) array.Smoothing(p.getSmoothing());
    t2 = clock();
    PrintTime(t1, t2, "Smoothing");
    t1 = clock();
    stats.setWigStats(array);
    t2 = clock();
    PrintTime(t1, t2, "WigStats");
    t1 = clock();
    stats.peakcall(array, chr.getname());
    t2 = clock();
    PrintTime(t1, t2, "peakcall");
  }
};

class ChrArrayList {
  void loadSampleData(const DROMPA::Global &p) {
    std::cout << "Load sample data..";
    clock_t t1,t2;
    for (auto &x: p.vsinfo.getarray()) {
      t1 = clock();
      arrays[x.first] = ChrArray(p, x, chr);
      t2 = clock();
      PrintTime(t1, t2, "ChrArray new");
    }
  }

public:
  const chrsize &chr;
  std::unordered_map<std::string, ChrArray> arrays;

  ChrArrayList(const DROMPA::Global &p, const chrsize &_chr): chr(_chr) {
    loadSampleData(p);
    
#ifdef DEBUG    
    std::cout << "all WigArray:" << std::endl;
    for (auto &x: arrays) {
      std::cout << x.first << ", binsize " << x.second.binsize << std::endl;
    }
    std::cout << "all SamplePair:" << std::endl;
    for (auto &x: p.samplepair) {
	std::cout << x.first.argvChIP << "," << x.first.argvInput
		  << ", binsize " << x.first.getbinsize() << std::endl;
    }
#endif
  }
};

class ReadProfile {
  int32_t stype;
  std::string xlabel;
  std::string Rscriptname;
  std::string Rfigurename;
  
protected:
  int32_t binsize;
  int32_t nbin;
  int32_t width_from_center;
  int32_t binwidth_from_center;

  std::string RDataname;
  std::unordered_map<std::string, std::vector<double>> hprofile;

  std::vector<genedata> get_garray(const GeneDataMap &mp)
  {
    std::vector<genedata> garray;
    for (auto &m: mp) {
      if (m.second.gtype == "nonsense_mediated_decay" ||
	  m.second.gtype == "processed_transcript" ||
	  m.second.gtype == "retained_intron") continue;
      garray.emplace_back(m.second);
    }
    return garray;
  }

  void WriteValAroundPosi(std::ofstream &out, const SamplePairOverlayed &pair, const ChrArrayList &arrays,
			  const int32_t posi, const std::string &strand, const int32_t chrlen)
  {
    if(posi - width_from_center < 0 || posi + width_from_center >= chrlen) return;
    
    int32_t bincenter(posi/binsize);
    int32_t sbin(bincenter - binwidth_from_center);
    int32_t ebin(bincenter + binwidth_from_center);
    if (strand == "+") {
      for (int32_t i=sbin; i<=ebin; ++i) {
	//	printf("%d\t%f\n", i, getSumVal(pair, arrays, i, i));
	out << "\t" << getSumVal(pair, arrays, i, i);
      }
    } else {
      for (int32_t i=ebin; i>=sbin; --i) {
	//	printf("%d\t%f\n", i, getSumVal(pair, arrays, i, i));
	out << "\t" << getSumVal(pair, arrays, i, i);
      }
    }
  }

  double getSumVal(const SamplePairOverlayed &pair, const ChrArrayList &arrays, const int32_t sbin, const int32_t ebin) {
    double sum(0);
    for (int32_t i=sbin; i<=ebin; ++i) sum += arrays.arrays.at(pair.first.argvChIP).array[i];
    return sum/(ebin - sbin + 1);
  }
  
public:
  ReadProfile(const DROMPA::Global &p, const int32_t _nbin=0):
    stype(p.prof.stype),
    width_from_center(p.prof.width_from_center)
  {
    if(p.prof.stype == 0) xlabel = "Distance from TSS (bp)";
    else if(p.prof.stype == 1) xlabel = "Distance from TES (bp)";
    else if(p.prof.stype == 2) xlabel = "% gene length from TSS";
    else if(p.prof.stype == 3) xlabel = "Distance from the peak summit (bp)";
    
    for (auto &x: p.samplepair) binsize = x.first.getbinsize();
    binwidth_from_center = width_from_center / binsize;
    
    if(_nbin) nbin = _nbin;
    else nbin = binwidth_from_center * 2 +1;

    std::vector<double> array(nbin, 0);
    for (auto &x: p.samplepair) hprofile[x.first.argvChIP] = array;
  }

  void setOutputFilename(const DROMPA::Global &p) {
    std::string prefix;
    if (stype==0)      prefix = p.getPrefixName() + ".ChIPread";
    else if (stype==1) prefix = p.getPrefixName() + ".Enrichment";
    Rscriptname = prefix + ".R";
    RDataname   = prefix + ".tsv";
    Rfigurename = prefix + ".pdf";
    for (auto &x: {Rscriptname, RDataname, Rfigurename}) unlink(x.c_str());
  }

  void MakeFigure(const DROMPA::Global &p) {
    std::cout << "\nMake figure.." << std::endl;
    std::ofstream out(Rscriptname);
    
    std::vector<double> vcol({CLR_RED, CLR_BLUE, CLR_GREEN, CLR_LIGHTCORAL, CLR_BLACK, CLR_PURPLE, CLR_GRAY3, CLR_OLIVE, CLR_YELLOW3, CLR_SLATEGRAY, CLR_PINK, CLR_SALMON, CLR_GREEN2, CLR_BLUE3, CLR_PURPLE2, CLR_DARKORANGE});
   
    //    out << "# bsnum_allowed=%d, bsnum_skipped=%d\n", d->ntotal_profile, d->ntotal_skip) << std::endl;

    out << "t <- read.table('"<< RDataname << "', header=F, sep='\\t', quote='')" << std::endl;
    out << "rownames(t) <- t[,1]" << std::endl;
    out << "t <- t[,-1]" << std::endl;
    out << "n <- nrow(t)" << std::endl;

    for (size_t i=1; i<=p.samplepair.size(); ++i) {
      out << boost::format("t%1% <- t[,%2%:%3%]\n") % i % (nbin*(i-1) +1) % (nbin*i);
      out << boost::format("colnames(t%1%) <- t%1%[1,]\n") % i;
      out << boost::format("t%1% <- t%1%[-1,]\n") % i;
      out << boost::format("p%1% <- apply(t%1%,2,mean)\n") % i;
      out << boost::format("sd%1% <- apply(t%1%,2,sd)\n") % i;
      out << boost::format("SE%1% <- 1.96 * sd%1%/sqrt(n)\n") % i;
      out << boost::format("p%1%_upper <- p%1% + SE%1%\n") % i;
      out << boost::format("p%1%_lower <- p%1% - SE%1%\n") % i;
      out << boost::format("x <- as.integer(colnames(t%1%))\n") % i;
    }
    out << "pdf('" << Rfigurename << "',6,6)" << std::endl;
    out << "plot(x,p1,type='l',col=rgb(" << vcol[0] << "," << vcol[1] << "," << vcol[2]
	<< "),xlab='" << xlabel << "',ylab='Read density')" << std::endl;
    out << "polygon(c(x, rev(x)), c(p1_lower, rev(p1_upper)), col=rgb("
	<< vcol[0] << "," << vcol[1] << "," << vcol[2] << ",0.3), border=NA)" << std::endl;

    for (size_t i=1; i<p.samplepair.size(); ++i) {
      std::string colstr = 
	std::to_string(vcol[i*3]) + ","
	+ std::to_string(vcol[i*3 +1]) + ","
	+ std::to_string(vcol[i*3 +2]) + "";
      out << boost::format("lines(x,p%1%,col=rgb(%2%))\n") % (i+1) % colstr;
      out << boost::format("polygon(c(x, rev(x)), c(p%1%_lower, rev(p%1%_upper)), col=rgb(%2%,0.3), border=NA)\n") % (i+1) % colstr;
    } 
    
    out << "legend('bottomleft',c(";
    for (size_t i=0; i<p.samplepair.size(); ++i) {
      out << "'" << p.samplepair[i].first.label << "'";
      if (i < p.samplepair.size()-1) out << ",";
    }
    out << "),col=c(rgb(" << vcol[0] << "," << vcol[1] << "," << vcol[2] <<")";
    for (size_t i=1; i<p.samplepair.size(); ++i) {
      out << ",rgb(" << vcol[i*3] << "," << vcol[i*3 +1] << "," << vcol[i*3 +2] << ")";
    }
    out << "), lwd=1.5)" << std::endl;

    std::string command("R --vanilla < " + Rscriptname);
    system(command.c_str());
  }
  
  virtual void WriteTSV_EachChr(const DROMPA::Global &p, const chrsize &chr)=0;

  void printHead(const DROMPA::Global &p) {
    std::ofstream out(RDataname);

    for (size_t j=0; j<p.samplepair.size(); ++j) {
      for (int32_t i=-binwidth_from_center; i<=binwidth_from_center; ++i) {
	out << "\t" << (i*binsize);
      }
    }
    out << std::endl;
  }
};

class ProfileTSS: public ReadProfile {

public:
  ProfileTSS(const DROMPA::Global &p):
    ReadProfile(p) {}
  
  void WriteTSV_EachChr(const DROMPA::Global &p, const chrsize &chr) {
    DEBUGprint("ProfileTSS:WriteTSV_EachChr");
    if(p.anno.genefile == "") {
      std::cerr << "Please specify --gene." << std::endl;
      exit(1);
    }

    std::string chrname(rmchr(chr.getname()));
    if (p.anno.gmp.find(chrname) == p.anno.gmp.end()) return;

    ChrArrayList arrays(p, chr);
    
    std::ofstream out(RDataname, std::ios::app);
    auto gmp(get_garray(p.anno.gmp.at(chrname)));
    for (auto &gene: gmp) {
      out << gene.gname;
      for (auto &x: p.samplepair) {
	if (p.prof.isPtypeTSS()) {
	  if (gene.strand == "+") WriteValAroundPosi(out, x, arrays, gene.txStart, gene.strand, chr.getlen());
	  else                    WriteValAroundPosi(out, x, arrays, gene.txEnd,   gene.strand, chr.getlen());
	} else if (p.prof.isPtypeTTS()) {
	  if (gene.strand == "+") WriteValAroundPosi(out, x, arrays, gene.txEnd,   gene.strand, chr.getlen());
	  else                    WriteValAroundPosi(out, x, arrays, gene.txStart, gene.strand, chr.getlen());
	}
      }
      out << std::endl;
    }
  }
};

class ProfileGene100: public ReadProfile {
  enum {GENEBLOCKNUM=100};
public:
  ProfileGene100(const DROMPA::Global &p):
    ReadProfile(p, GENEBLOCKNUM * 3)
  {}

  void WriteTSV_EachChr(const DROMPA::Global &p, const chrsize &chr) {
    DEBUGprint("ProfileGene100:WriteTSV_EachChr");
    if(p.anno.genefile == "") {
      std::cerr << "Please specify --gene." << std::endl;
      exit(1);
    }

    std::string chrname(rmchr(chr.getname()));
    if (p.anno.gmp.find(chrname) == p.anno.gmp.end()) return;
    
    ChrArrayList arrays(p, chr);
    std::ofstream out(RDataname, std::ios::app);
        
    auto gmp(get_garray(p.anno.gmp.at(chrname)));
    for (auto &gene: gmp) {
      int32_t s,e;
      int32_t len(gene.length());
      double len100(len / (double)GENEBLOCKNUM);
      if(gene.txEnd + len >= chr.getlen() || gene.txStart - len < 0) continue;

      out << gene.gname;
      for(int32_t i=0; i<nbin; ++i) {
	if (gene.strand == "+") {
	  s = (gene.txStart - len + len100 *i)       / binsize;
	  e = (gene.txStart - len + len100 *(i+1) -1)/ binsize;
	}else{
	  s = (gene.txEnd + len - len100 * (i+1))/ binsize;
	  e = (gene.txEnd + len - len100 * i -1) / binsize;
	}
	for (auto &x: p.samplepair) {
	  //printf("%d\t%d\t%f\n", s, e, getSumVal(x, arrays, s, e));
	  out << "\t" << getSumVal(x, arrays, s, e);
	}
      }
      out << std::endl;
    }
  }

  void printHead(const DROMPA::Global &p) {
    std::ofstream out(RDataname);
    
    for (size_t j=0;j<p.samplepair.size(); ++j) {
      for (int32_t i=-GENEBLOCKNUM; i<GENEBLOCKNUM*2; ++i) {
	out << "\t" << i;
      }
    }
    out << std::endl;
  }
};


class ProfileBedSites: public ReadProfile {
public:
  ProfileBedSites(const DROMPA::Global &p):
    ReadProfile(p) {}

  void WriteTSV_EachChr(const DROMPA::Global &p, const chrsize &chr) {
    DEBUGprint("ProfileBedSites:WriteTSV_EachChr");
    
    if(!p.anno.vbedlist.size()) {
      std::cerr << "Please specify --bed." << std::endl;
      exit(1);
    }
    std::ofstream out(RDataname, std::ios::app);
    
    ChrArrayList arrays(p, chr);
    for (auto &vbed: p.anno.vbedlist) {
      for (auto &bed: vbed.getvBed()) {
	if (bed.chr != chr.getname()) continue;
	out << bed.getSiteStr();
	for (auto &x: p.samplepair) WriteValAroundPosi(out, x, arrays, bed.summit, "+", chr.getlen());
	out << std::endl;
      }
    }
  }
};

class Figure {
  const chrsize &chr;
  ChrArrayList arrays;
  std::vector<SamplePairOverlayed> &vsamplepairoverlayed;
  std::vector<bed> regionBed;
  
  void setSamplePairEachRatio(const DROMPA::Global &p, SamplePairEach &pair, const std::string &chrname)
  {
    DEBUGprint("setSamplePairEachRatio");
    if (pair.argvInput == "") return;
    pair.ratio = 1;
    
    switch (p.getNorm()) {
    case 0:
      pair.ratio = 1;
      break;
    case 1:
      pair.ratio = getratio(arrays.arrays.at(pair.argvChIP).totalreadnum, arrays.arrays.at(pair.argvInput).totalreadnum);
      break;
    case 2:
      pair.ratio = getratio(arrays.arrays.at(pair.argvChIP).totalreadnum_chr.at(chrname), arrays.arrays.at(pair.argvInput).totalreadnum_chr.at(chrname));
      break;
    case 3:
      pair.ratio = 1; // NCIS
      break;
    }
#ifdef DEBUG
    std::cout << "ChIP/Input Ratio for chr " << chrname << ": " << pair.ratio << std::endl;
#endif
  }
  
public:
  Figure(DROMPA::Global &p, const chrsize &_chr):
    chr(_chr),
    arrays(p, chr),
    vsamplepairoverlayed(p.samplepair),
    regionBed(p.drawregion.getRegionBedChr(chr.getname()))
  {

    for (auto &x: vsamplepairoverlayed) {
      setSamplePairEachRatio(p, x.first, chr.getname());
      if (x.OverlayExists()) setSamplePairEachRatio(p, x.second, chr.getname());
    }
  }

  int32_t Draw(DROMPA::Global &p) {
    if (p.drawregion.isRegionBed() && !regionBed.size()) return 0;
    std::cout << "Drawing.." << std::endl;
    DrawData(p);
    return 1;
  }

  void DrawData(DROMPA::Global &p);
};

#endif /* _DD_READFILE_H_ */
