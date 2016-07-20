/* Copyright(c)  Ryuichiro Nakato <rnakato@iam.u-tokyo.ac.jp>
 * This file is a part of DROMPA sources.
 */
#ifndef _PW_GV_H_
#define _PW_GV_H_

#include <fstream>
#include <boost/format.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/thread.hpp>
#include "seq.h"
#include "util.h"
#include "macro.h"
#include "readdata.h"
#include "statistics.h"

using namespace std;
using namespace boost::program_options;

/* default parameter */
#define NUM_GCOV 5000000

class SeqStats;

void calcFRiP(SeqStats &, const vector<bed>);
void printDist(ofstream &out, const vector<int> v, const string str, const long nread);

class Dist {
  enum {ReadMax=200, FragMax=1000};
  int lenF3;
  int lenF5;
  int eflen;
  vector<int> vlenF3;
  vector<int> vlenF5;
  vector<int> vflen;
  
 public:
 Dist(): lenF3(0), lenF5(0), eflen(0),
    vlenF3(ReadMax,0),
    vlenF5(ReadMax,0),
    vflen(FragMax,0) {}
  void getmaxlenF3() { lenF3 = getmaxi(vlenF3); }
  void getmaxlenF5() { lenF5 = getmaxi(vlenF5); }
  void getmaxeflen() { eflen = getmaxi(vflen); }
  void seteflen(const int len) { eflen = len; }
  int geteflen() const { return eflen; }
  int getlenF3() const { return lenF3; }
  vector<int> getvlenF3() const { return vlenF3; }
  vector<int> getvlenF5() const { return vlenF5; }
  vector<int> getvflen()  const { return vflen; }
  void addF3(const int len)   { ++vlenF3[len]; }
  void addF5(const int len)   { ++vlenF5[len]; }
  void addflen(const int len) { ++vflen[len]; }
};

class Fragment {
public:
  string chr;
  int F3;
  Strand strand;
  int fraglen;
  int readlen_F3;

 Fragment(): fraglen(0), readlen_F3(0) {}
 void addSAM(const vector<string> &v, const bool pair, const int sv) {
   chr = rmchr(v[2]);
   readlen_F3 = v[9].length();
   if(pair) fraglen = abs(stoi(v[8]));
   if(sv&16) {
     strand = STRAND_MINUS;
     F3 = stoi(v[3]) + readlen_F3 -1; //SAM
   } else {
     strand = STRAND_PLUS;
     F3 = stoi(v[3]) -1;  //SAM
   }
 }
 void print() const {
   BPRINT("chr:%1%\tposi:%2%\tstrand:%3%\tfraglen:%4%\treadlen:%5%\n") % chr % F3 % strand % fraglen % readlen_F3;
  }
};

class Read {
  int weight;
  enum {WeightNum=1000};
 public:
  int F3;
  int F5;
  int duplicate;
  int inpeak;
  
 Read(const Fragment &frag): weight(WeightNum), F3(frag.F3), duplicate(0), inpeak(0) {
    if(frag.strand == STRAND_PLUS) F5 = frag.F3 + frag.fraglen;
    else F5 = frag.F3 - frag.fraglen;
  }
  double getWeight() const {
    return weight/static_cast<double>(WeightNum);
  }
  void multiplyWeight(const double w) {
    weight *= w;
  }
};

class strandData {
 public:
  vector<Read> vRead;
  long nread;
  long nread_nonred;
  long nread_red;
  double nread_rpm;
  double nread_afterGC;

 strandData(): nread(0), nread_nonred(0), nread_red(0), nread_rpm(0), nread_afterGC(0) {}
  void setnread() { nread = vRead.size(); }
  void print() const {
    cout << nread << "\t" << nread_nonred << "\t" << nread_red << "\t" << nread_rpm << "\t" << nread_afterGC << endl;
  }
  void printnonred(ofstream &out)  const { printr(out, nread_nonred,  nread); }
  void printred(ofstream &out)     const { printr(out, nread_red,     nread); }
  void printafterGC(ofstream &out) const { printr(out, nread_afterGC, nread); }
  void addReadAfterGC(const double w, boost::mutex &mtx) {
    boost::mutex::scoped_lock lock(mtx);
    nread_afterGC += w;
  }
};

class Wigstats {
  enum{n_mpDist=20, n_wigDist=200};
  long sum;
 public:
  double ave, var, nb_p, nb_n, nb_p0; // pois_p0
  vector<long> mpDist;
  vector<long> wigDist;
  vector<double> pwigDist;

 Wigstats(): sum(0),ave(0), var(0), nb_p(0), nb_n(0), nb_p0(0),
    mpDist(n_mpDist,0), wigDist(n_wigDist,0), pwigDist(n_wigDist,0) {}

  double getPoisson(const int i) const {
    if(ave) return _getPoisson(i, ave);
    else return 0;
  }
  double getNegativeBinomial(const int i) const {
    return _getNegativeBinomial(i, nb_p, nb_n);
  }
  /*  double getZIP(const int i) const {
    return _getZIP(i, ave, pois_p0);
    }*/
  double getZINB(const int i) const {
    if(ave) return _getZINB(i, nb_p, nb_n, nb_p0);
    else return 0;
  }
  int getwigDistthre() const {
    int thre(9);
    long num;
    do{
      ++thre;
      num=0;
      for(int i=0; i<thre; ++i) num += wigDist[i];
    } while(num < sum*0.8 && thre <n_wigDist-1);
#ifdef DEBUG
    BPRINT("\nthre %1%  (%2% / %3%)\n") % thre % num % sum;
#endif
    return thre;
  }
  void estimateParam() {
    int thre = getwigDistthre();
    double par[thre+1];
    par[0] = thre;
    for(int i=0; i<thre; ++i) par[i+1] = pwigDist[i];
    iterateZINB(&par, nb_p, nb_n, nb_p, nb_n, nb_p0);
  }
  void setpWigDist() {
    for(size_t i=0; i<wigDist.size(); ++i) pwigDist[i] = wigDist[i]/static_cast<double>(sum);
  }
  void getWigStats(const vector<int> &wigarray) {
    int num95 = getPercentile(wigarray, 0.95);
    
    int size = wigDist.size();
    vector<int> ar;
    for(auto x: wigarray) {
    if( x<0) cout << sum << "xxx" << x << endl;
    ++sum;
      int v = WIGARRAY2VALUE(x);
      if(v < size) ++wigDist[v];
      if(x >= num95) continue;
      ar.push_back(v);
    }
    setpWigDist();

    moment<int> mm(ar, 0);
    ave = mm.getmean();
    var = mm.getvar();
    nb_p = var ? ave/var : 0;
    if(nb_p>=1) nb_p = 0.9;
    if(nb_p<=0) nb_p = 0.1; 
    nb_n = ave * nb_p /(1 - nb_p);

    //    cout << ave << "\t" << var << "\t" << nb_p << "\t" << nb_n<< endl;
    if(ave) estimateParam();
  }
  void printwigDist(ofstream &out, const int i) const {
    out << boost::format("%1%\t%2%\t") % wigDist[i] % pwigDist[i];
  }
  void addmpDist(const double p) {
    if(!RANGE(p,0,1)) cout << "Warning: mappability " << p << " should be [0,1]" << endl;
    else ++mpDist[(int)(p*n_mpDist)];
  }
  void addWigDist(const Wigstats &x) {
    for(uint i=0; i<wigDist.size(); ++i) wigDist[i] += x.wigDist[i];
    sum += x.sum;
    setpWigDist();
  }
  void printmpDist() const {
    long num = accumulate(mpDist.begin(), mpDist.end(), 0);
    for(size_t i=0; i<mpDist.size(); ++i)
      BPRINT("~%1%%%\t%2%\t%3%\n") % ((i+1)*100/mpDist.size()) % mpDist[i] % (mpDist[i]/static_cast<double>(num)); 
  }
  void printPoispar(ofstream &out) const {
    out << boost::format("%1$.3f\t%2$.3f\t") % ave % var;
  }
  void printZINBpar(ofstream &out) const {
    out << boost::format("%1%\t%2%\t%3%") % nb_p % nb_n % nb_p0;
  }
};

class GenomeCoverage {
  long nbp, ncov, ncovnorm;
  double gcovRaw, gcovNorm;
 public:
 GenomeCoverage(): nbp(0), ncov(0), ncovnorm(0), gcovRaw(0), gcovNorm(0) {}
  void addGcov(const GenomeCoverage &x, boost::mutex &mtx) {
    boost::mutex::scoped_lock lock(mtx);
    nbp      += x.nbp;
    ncov     += x.ncov;
    ncovnorm += x.ncovnorm;
    gcovRaw  = nbp ? ncov / static_cast<double>(nbp): 0;
    gcovNorm = nbp ? ncovnorm / static_cast<double>(nbp): 0;
  }
  void calcGcov(const vector<char> &array) {
    for(size_t i=0; i<array.size(); ++i) {
      if(array[i] >= MAPPABLE)     ++nbp;      // MAPPABLE || COVREAD_ALL || COVREAD_NORM
      if(array[i] >= COVREAD_ALL)  ++ncov;     // COVREAD_ALL || COVREAD_NORM
      if(array[i] == COVREAD_NORM) ++ncovnorm;
    }
    gcovRaw  = nbp ? ncov     / static_cast<double>(nbp): 0;
    gcovNorm = nbp ? ncovnorm / static_cast<double>(nbp): 0;
    //    cout << nbp << ","<< ncov << ","<< ncovnorm << ","<< gcovRaw << ","<< gcovNorm << endl;
  }
  void print(ofstream &out, const bool gv) const {
      if(gv) out << boost::format("%1$.3f\t(%2$.3f)\t") % gcovRaw % gcovNorm;
      else   out << boost::format("%1$.3f\t%2$.3f\t")   % gcovRaw % gcovNorm;
  //  out << boost::format("(%1%/%2%)\t") % p.ncovnorm % p.nbp;
  }
};

class SeqStats {
  bool yeast;
  long len, len_mpbl;
  /* FRiP */
  long nread_inbed;
 public:
  string name;
  int nbin;

  Wigstats ws;
  GenomeCoverage gcov;
  
  strandData seq[STRANDNUM];
  double depth;
  double w;
  
 SeqStats(string s, int l=0): yeast(false), len(l), len_mpbl(l), nread_inbed(0), nbin(0), depth(0), w(0) {
    name = rmchr(s);
  }
  void addNreadInBed(const int i) { nread_inbed += i; }
  long getNreadInbed() const { return nread_inbed; }
  void addlen(const SeqStats &x) {
    len      += x.len;
    len_mpbl += x.len_mpbl;
    nbin     += x.nbin;
  }
  void addfrag(const Fragment &frag) {
    Read r(frag);
    seq[frag.strand].vRead.push_back(r);
  }
  long bothnread ()         const { return seq[STRAND_PLUS].nread         + seq[STRAND_MINUS].nread; }
  long bothnread_nonred ()  const { return seq[STRAND_PLUS].nread_nonred  + seq[STRAND_MINUS].nread_nonred; }
  long bothnread_red ()     const { return seq[STRAND_PLUS].nread_red     + seq[STRAND_MINUS].nread_red; }
  long bothnread_rpm ()     const { return seq[STRAND_PLUS].nread_rpm     + seq[STRAND_MINUS].nread_rpm; }
  long bothnread_afterGC () const { return seq[STRAND_PLUS].nread_afterGC + seq[STRAND_MINUS].nread_afterGC; }

  void addnread(const SeqStats &x) { 
    for(int i=0; i<STRANDNUM; i++) seq[i].nread += x.seq[i].nread;
  }
  void addnread_red(const SeqStats &x) { 
    for(int i=0; i<STRANDNUM; i++) {
      seq[i].nread_nonred += x.seq[i].nread_nonred;
      seq[i].nread_red    += x.seq[i].nread_red;
    }
  }

  long getlen()     const { return len; }
  long getlenmpbl() const { return len_mpbl; }
  double getpmpbl() const { return len/static_cast<double>(len_mpbl); }
  void print() const {
    cout << name << "\t" << len << "\t" << len_mpbl << "\t" << bothnread() << "\t" << bothnread_nonred() << "\t" << bothnread_red() << "\t" << bothnread_rpm() << "\t" << bothnread_afterGC()<< "\t" << depth << endl;
  }
  void calcdepth(const int flen) {
    depth = len_mpbl ? bothnread_nonred() * flen / static_cast<double>(len_mpbl): 0;
  }
  void setF5(int flen) {
    int d;
    for(int strand=0; strand<STRANDNUM; ++strand) {
      if(strand == STRAND_PLUS) d = flen; else d = -flen;
      for(auto &x: seq[strand].vRead) x.F5 = x.F3 + d;
    }
  }
  double getFRiP() const {
    return nread_inbed/static_cast<double>(bothnread_nonred());
  }
  void setWeight(double weight) {
    w = weight;
    for(int i=0; i<STRANDNUM; i++) seq[i].nread_rpm = seq[i].nread_nonred * w;
  }
  
  void yeaston() { yeast = true; }

  bool isautosome() const {
    int chrnum(0);
    try {
      chrnum = stoi(name);
    } catch (std::invalid_argument e) {  // 数値以外
      if(yeast) { 
	if(name=="I")         chrnum = 1;
	else if(name=="II")   chrnum = 2;
	else if(name=="III")  chrnum = 3;
	else if(name=="IV")   chrnum = 4;
	else if(name=="V")    chrnum = 5;
	else if(name=="VI")   chrnum = 6;
	else if(name=="VII")  chrnum = 7;
	else if(name=="VIII") chrnum = 8;
	else if(name=="IX")   chrnum = 9;
	else if(name=="X")    chrnum = 10;
	else if(name=="XI")   chrnum = 11;
	else if(name=="XII")  chrnum = 12;
	else if(name=="XIII") chrnum = 13;
	else if(name=="XIV")  chrnum = 14;
	else if(name=="XV")   chrnum = 15;
	else if(name=="XVI")  chrnum = 16;
      }
      if(name=="2L") chrnum = 1;
      if(name=="2R") chrnum = 2;
      if(name=="3L") chrnum = 3;
      if(name=="3R") chrnum = 4;
    }
    if(chrnum) return true;
    else       return false;
  }
  friend void getMpbl(const string, vector<SeqStats> &chr);
  //  friend void setlenmpbl(SeqStats &x, string str, int l);
};

class Mapfile {
  bool yeast;
  Dist dist;
  string oprefix;
  string obinprefix;
  int flen_def;
  
  // PCR bias
  int thre4filtering;
  int nt_all, nt_nonred, nt_red;
  bool tv, gv;
  double r4cmp;
  vector<bed> vbed;
  vector<Peak> vPeak;

 public:
  SeqStats genome;
  vector<SeqStats> chr;
  vector<SeqStats>::iterator lchr; // longest chromosome
  vector<sepchr> vsepchr;

  // GC bias
  vector<double> GCweight;
  int maxGC;

  // Wigdist
  int nwigdist;
  vector<int> wigDist;

  Mapfile(const variables_map &values);
  void readGenomeTable(const variables_map &values);
  //  void getMpbl(const variables_map &values);
  vector<sepchr> getVsepchr(const int);

  void tvon() { tv = true; }
  void gvon() { gv = true; }
  int isgv() const { return gv; };
  void setthre4filtering(const variables_map &values) {
    if(values["thre_pb"].as<int>()) thre4filtering = values["thre_pb"].as<int>();
    else thre4filtering = max(1, (int)(genome.bothnread() *10/static_cast<double>(genome.getlenmpbl())));
    cout << "Checking redundant reads: redundancy threshold " << thre4filtering << endl;
  }
  int getthre4filtering() const { return thre4filtering; };
  void setr4cmp(const double r) { r4cmp = r; }
  double getr4cmp() const { return r4cmp; }
  void incNtAll() { ++nt_all; }
  void incNtNonred() { ++nt_nonred; }
  void incNtRed() { ++nt_red; }
  void printPeak(const variables_map &values) const {
    string filename = getbinprefix() + ".peak.xls";
    ofstream out(filename);

    vPeak[0].printHead(out);
    for(uint i=0; i<vPeak.size(); ++i) {
      vPeak[i].print(out, i, values["binsize"].as<int>());
    }
  }
  string getprefix() const { return oprefix; }
  string getbinprefix() const { return obinprefix; }
  void setFraglen(const variables_map &values) {
    dist.getmaxlenF3();
    if(values.count("pair")) {
      dist.getmaxlenF5();
      dist.getmaxeflen();
    }
  }
  vector<bed> getvbed() const { return vbed; }
  void printFlen(const variables_map &values, ofstream &out) const {
    if(!values.count("nomodel")) out << "Estimated fragment length: " << dist.geteflen() << endl;
    else out << "Predefined fragment length: " << flen_def << endl;
  }
  void addF5(const int readlen_F5) { dist.addF5(readlen_F5); }
  void addfrag(const Fragment &frag) {
    dist.addF3(frag.readlen_F3);
    dist.addflen(frag.fraglen);
    int on(0);
    for(auto &x:chr) {
      if(x.name == frag.chr) {
	x.addfrag(frag);
	on++;
      }
    }
    if(!on) cerr << "Warning: " << frag.chr << " is not in genometable." << endl;
  }

  void setbed(const string bedfilename) {
    isFile(bedfilename);
    vbed = parseBed<bed>(bedfilename);
    //    printBed(vbed);
  }
  void outputDistFile(const variables_map &values)
  {
    string outputfile = oprefix + ".readlength_dist.csv";
    ofstream out(outputfile);
    printDist(out, dist.getvlenF3(), "F3", genome.bothnread());
    if(values.count("pair")) printDist(out, dist.getvlenF5(), "F5", genome.bothnread());
    out.close();
    
    if(values.count("pair")) {
      outputfile = oprefix + ".fraglen_dist.xls";
      ofstream out(outputfile);
      printDist(out, dist.getvflen(), "Fragmemt", genome.bothnread());
    }
  }

  void calcdepth(const variables_map &values) {
    int flen(getflen(values));
    for (auto &x:chr) x.calcdepth(flen);
    genome.calcdepth(flen);
  }
  void setF5(const variables_map &values) {
    int flen(getflen(values));
    for (auto &x:chr) x.setF5(flen);
  }
  void setnread() {
    for (auto &x:chr) {
      for(int i=0; i<STRANDNUM; i++) x.seq[i].setnread();
      genome.addnread(x);
    }
  }
  void setnread_red() {
    for (auto &x:chr) genome.addnread_red(x);
  }
  void printComplexity(ofstream &out) const {
    if(tv) out << boost::format("Library complexity: (%1$.3f) (%2%/%3%)\n") % complexity() % nt_nonred % nt_all;
    else   out << boost::format("Library complexity: %1$.3f (%2%/%3%)\n")   % complexity() % nt_nonred % nt_all;
  }
  double complexity() const { return nt_nonred/static_cast<double>(nt_all); }
  void printstats() const {
    cout << "name\tlength\tlen_mpbl\tread num\tnonred num\tred num\tnormed\tafterGC\tdepth" << endl;
    genome.print();
    for (auto x:chr) x.print();
  }
  void setFRiP() {
    cout << "calculate FRiP score.." << flush;
    for(auto &c: chr) {
      calcFRiP(c, vbed);
      genome.addNreadInBed(c.getNreadInbed());
    }
    //    genome.FRiP = genome.nread_inbed/static_cast<double>(genome.bothnread_nonred());
    
    cout << "done." << endl;
    return;
  }
  void seteflen(const int len) {
    dist.seteflen(len);
  }
  int getlenF3() const { return dist.getlenF3(); }
  int getflen(const variables_map &values) const {
    int flen;
    if(!values.count("nomodel") || values.count("pair")) flen = dist.geteflen();
    else flen = flen_def;
    return flen;
  }
  void addPeak(const Peak &peak) {
    vPeak.push_back(peak);
  }
  void renewPeak(const int i, const double val, const double p) {
    vPeak[vPeak.size()-1].renew(i, val, p);
  }

  void estimateZINB() {
    int thre = genome.ws.getwigDistthre();
    double par[thre+1];
    par[0] = thre;
    for(int i=0; i<thre; ++i) par[i+1] = genome.ws.pwigDist[i];

    //    iteratePoisson(&par, lchr->ave, genome.ave, genome.pois_p0);
    iterateZINB(&par, lchr->ws.nb_p, lchr->ws.nb_n, genome.ws.nb_p, genome.ws.nb_n, genome.ws.nb_p0);

    return;
  }
};

#endif /* _PW_GV_H_ */
