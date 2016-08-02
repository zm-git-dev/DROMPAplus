#include "pw_shiftprofile.h"
#include "pw_shiftprofile_p.h"
#include "macro.h"
#include <map>
#include <boost/thread.hpp>

void addmp(std::map<int, double> &mpto, const std::map<int, double> &mpfrom, double w)
{
  for(auto itr = mpfrom.begin(); itr != mpfrom.end(); ++itr) {
    mpto[itr->first] += itr->second * w;
  }
}

double getJaccard(int step, int width, int xysum, const std::vector<char> &fwd, const std::vector<char> &rev)
{
  int xy(0);
  for(int j=mp_from; j<width-ng_to; ++j) if(fwd[j] * rev[j+step]) xy += std::max(fwd[j], rev[j+step]);
  return (xy/static_cast<double>(xysum-xy));
}

void genThreadJacVec(ReadShiftProfile &chr, int xysum, const std::vector<char> &fwd, const std::vector<char> &rev, int s, int e, boost::mutex &mtx)
{
  for(int step=s; step<e; ++step) {
    chr.setmp(step, getJaccard(step, chr.width, xysum, fwd, rev), mtx);
  }
}

void shiftJacVec::setDist(ReadShiftProfile &chr, const std::vector<char> &fwd, const std::vector<char> &rev)
{
  int xx = accumulate(fwd.begin(), fwd.end(), 0);
  int yy = accumulate(rev.begin(), rev.end(), 0);

  boost::thread_group agroup;
  boost::mutex mtx;
  for(uint i=0; i<seprange.size(); i++) {
    agroup.create_thread(bind(&genThreadJacVec, boost::ref(chr), xx+yy, boost::cref(fwd), boost::cref(rev), seprange[i].start, seprange[i].end, boost::ref(mtx)));
  }
  agroup.join_all();

  for(int step=ng_from; step<ng_to; step+=ng_step) {
    chr.nc[step] = getJaccard(step, chr.width-ng_to, xx+yy, fwd, rev);
  }
}

void genThreadCcp(ReadShiftProfile &chr, const std::vector<char> &fwd, const std::vector<char> &rev, double mx, double my, const int s, const int e, boost::mutex &mtx)
{
  for(int step=s; step<e; ++step) {
    double xy(0);
    for(int j=mp_from; j<chr.width-ng_to; ++j) xy += (fwd[j] - mx) * (rev[j+step] - my);
    chr.setmp(step, xy, mtx);
  }
}

void shiftCcp::setDist(ReadShiftProfile &chr, const std::vector<char> &fwd, const std::vector<char> &rev)
{
  moment<char> x(fwd, mp_from, chr.width-ng_to);
  moment<char> y(rev, mp_from, chr.width-ng_to);

  boost::thread_group agroup;
  boost::mutex mtx;
  for(uint i=0; i<seprange.size(); i++) {
    agroup.create_thread(bind(genThreadCcp, boost::ref(chr), boost::cref(fwd), boost::cref(rev), x.getmean(), y.getmean(), seprange[i].start, seprange[i].end, boost::ref(mtx)));
  }
  agroup.join_all();

  for(int step=ng_from; step<ng_to; step+=ng_step) {
    double xy(0);
    for(int j=mp_from; j<chr.width-ng_to; ++j) xy += (fwd[j] - x.getmean()) * (rev[j+step] - y.getmean());
    chr.nc[step] = xy;
  }

  double val = 1/(x.getsd() * y.getsd() * (chr.width-ng_to - mp_from - 1));
  for(auto itr = chr.mp.begin(); itr != chr.mp.end(); ++itr) itr->second *= val;
  for(auto itr = chr.nc.begin(); itr != chr.nc.end(); ++itr) itr->second *= val;
}

void shiftJacBit::setDist(ReadShiftProfile &chr, const boost::dynamic_bitset<> &fwd, boost::dynamic_bitset<> &rev)
{
  int xysum(fwd.count() + rev.count());

  rev <<= mp_from;
  for(int step=-mp_from; step<mp_to; ++step) {
    rev >>= 1;
    int xy((fwd & rev).count());
    //    chr.mp[step] = xy;
    chr.mp[step] = xy/static_cast<double>(xysum-xy);
  }
  rev >>= (ng_from - mp_to);

  for(int step=ng_from; step<ng_to; step+=ng_step) {
    rev >>= ng_step;
    int xy((fwd & rev).count());
    chr.nc[step] = xy/static_cast<double>(xysum-xy);
  }
}

void shiftHamming::setDist(ReadShiftProfile &chr, const boost::dynamic_bitset<> &fwd, boost::dynamic_bitset<> &rev)
{
  rev <<= mp_from;
  for(int step=-mp_from; step<mp_to; ++step) {
    rev >>= 1;
    chr.mp[step] = (fwd ^ rev).count();
  }
  rev >>= (ng_from - mp_to);
  
  for(int step=ng_from; step<ng_to; step+=ng_step) {
    rev >>= ng_step;
    chr.nc[step] = (fwd ^ rev).count();
  }
}

std::vector<char> genVector(const strandData &seq, int start, int end)
{
  std::vector<char> array(end-start, 0);
  for (auto x: seq.vRead) {
    if(!x.duplicate && RANGE(x.F3, start, end-1))
      ++array[x.F3 - start];
  }
  return array;
}

std::vector<char> genVector4FixedReadsNum(const strandData &seq, int start, int end, const int numRead4fvp, const long nread)
{
  double r(1);
  static int on(0);
  if(numRead4fvp) r = numRead4fvp/static_cast<double>(nread);
  if(r>1){
    if(!on) {
      std::cerr << "\nWarning: number of reads for Fragment variability is < "<< (int)(numRead4fvp/NUM_1M) <<" million.\n";
      //    p.lackOfRead4FragmentVar_on();
      on=1;
    }
  }
  double r4cmp = r*RAND_MAX;
  std::vector<char> array(end-start, 0);
  for (auto x: seq.vRead) {
    if(!x.duplicate && RANGE(x.F3, start, end-1)){
      if(rand() >= r4cmp) continue;
      ++array[x.F3 - start];
    }
  }
  return array;
}

boost::dynamic_bitset<> genBitset(const strandData &seq, int start, int end)
{
  boost::dynamic_bitset<> array(end-start);
  for (auto x: seq.vRead) {
    if(!x.duplicate && RANGE(x.F3, start, end-1))
      array.set(x.F3 - start);
  }
  return array;
}

template <class T>
void genThread(T &dist, const Mapfile &p, uint chr_s, uint chr_e, std::string typestr, int numRead4fvp) {
  for(uint i=chr_s; i<=chr_e; ++i) {
    std::cout << p.genome.chr[i].name << ".." << std::flush;

    dist.execchr(p, i, numRead4fvp);
    dist.chr[i].setflen();
    
    std::string filename = p.getprefix() + "." + typestr + "." + p.genome.chr[i].name + ".csv";
    dist.outputmpChr(filename, i);
  }
}

template <class T>
void makeProfile(Mapfile &p, const std::string &typestr, const int numthreads, int numRead4fvp=0)
{
  T dist(p, numthreads);
  dist.printStartMessage();

  boost::thread_group agroup;
  boost::mutex mtx;

  if(typestr == "hdp" || typestr == "jaccard") {
    agroup.create_thread(bind(genThread<T>, boost::ref(dist), boost::cref(p), 0, 0, typestr, numRead4fvp));
      /*    for(uint i=0; i<p.genome.vsepchr.size(); i++) {
      agroup.create_thread(bind(genThread<T>, boost::ref(dist), boost::cref(p), p.genome.vsepchr[i].s, p.genome.vsepchr[i].e, typestr, numRead4fvp));
      }*/
    agroup.join_all();
  } else {
    genThread(dist, p, 0, p.genome.chr.size()-1, typestr, numRead4fvp);
    //genThread(dist, p, 0, 0, typestr, numRead4fvp);

    if(typestr == "fvp") {
      std::string filename = p.getprefix() + ".mpfv.csv";
      dist.printmpfv(filename);
    }
  }

  //  GaussianSmoothing(p.dist.hd);

  // set fragment length;
  for(uint i=0; i<p.genome.chr.size(); ++i) {
    if(p.genome.chr[i].isautosome()) dist.addmp2genome(i);
  }

  dist.setflen();
  p.seteflen(dist.getnsci());

  std::string filename = p.getprefix() + "." + typestr + ".csv";
  dist.outputmpGenome(filename);

  return;
}

void strShiftProfile(const MyOpt::Variables &values, Mapfile &p, std::string typestr)
{
  int numthreads(values["threads"].as<int>());
  if(typestr=="exjaccard")    makeProfile<shiftJacVec>(p, typestr, numthreads);
  else if(typestr=="jaccard") makeProfile<shiftJacBit>(p, typestr, numthreads);
  else if(typestr=="ccp")     makeProfile<shiftCcp>(p, typestr, numthreads);
  else if(typestr=="hdp")     makeProfile<shiftHamming>(p, typestr, numthreads);
  else if(typestr=="fvp")     makeProfile<shiftFragVar>(p, typestr, numthreads, values["nfvp"].as<int>());
  
  return;
}

void genThreadFragVar(ReadShiftProfile &chr, std::map<int, FragmentVariability> &mpfv, const std::vector<char> &fwd, const std::vector<char> &rev, const FragmentVariability &fvback, const int s, const int e, boost::mutex &mtx)
{
  for(int step=s; step<e; ++step) {
    FragmentVariability fv;
    fv.setVariability(step, chr.start, chr.end, chr.width, fwd, rev);

    double diffMax(0);
    for(size_t k=0; k<sizeOfvDistOfDistaneOfFrag; ++k) {
      //      std::cout << fv.getAccuOfDistanceOfFragment(k) << "\t" << fvback.getAccuOfDistanceOfFragment(k) << std::endl;
      diffMax = std::max(diffMax, fv.getAccuOfDistanceOfFragment(k) - fvback.getAccuOfDistanceOfFragment(k));
    }
    chr.setmp(step, diffMax, mtx);
    mpfv[step].add2genome(fv, mtx);
  }
}

void shiftFragVar::setDist(ReadShiftProfile &chr, const std::vector<char> &fwd, const std::vector<char> &rev)
{
   FragmentVariability fvback;
  fvback.setVariability(ng_from, chr.start, chr.end, chr.width, fwd, rev);

#ifdef DEBUG
  FragmentVariability fv1;
  fv1.setVariability(50, chr.start, chr.end, chr.width, fwd, rev);
  FragmentVariability fv2;
  fv2.setVariability(150, chr.start, chr.end, chr.width, fwd, rev);
  FragmentVariability fv3;
  fv3.setVariability(500, chr.start, chr.end, chr.width, fwd, rev);
  FragmentVariability fv4;
  fv4.setVariability(1000, chr.start, chr.end, chr.width, fwd, rev);
  FragmentVariability fv5;
  fv5.setVariability(2000, chr.start, chr.end, chr.width, fwd, rev);
  FragmentVariability fv6;
  fv6.setVariability(3000, chr.start, chr.end, chr.width, fwd, rev);

  std::cout << "\n\tlen50\tlen150\tlen500\tlen1000\tlen2000\tlen3000\tlen4000\tlen50\tlen150\tlen500\tlen1000\tlen2000\tlen3000\tlen4000" << std::endl;
  for(size_t k=0; k<sizeOfvDistOfDistaneOfFrag; ++k) {
    std::cout << k << "\t"
	      << fv1.getAccuOfDistanceOfFragment(k) << "\t"
	      << fv2.getAccuOfDistanceOfFragment(k) << "\t"
	      << fv3.getAccuOfDistanceOfFragment(k) << "\t"
	      << fv4.getAccuOfDistanceOfFragment(k) << "\t"
	      << fv5.getAccuOfDistanceOfFragment(k) << "\t"
	      << fv6.getAccuOfDistanceOfFragment(k) << "\t"
	      << fvback.getAccuOfDistanceOfFragment(k) << "\t"
	      << fv1.getDistOfDistanceOfFragment(k) << "\t"
	      << fv2.getDistOfDistanceOfFragment(k) << "\t"
	      << fv3.getDistOfDistanceOfFragment(k) << "\t"
	      << fv4.getDistOfDistanceOfFragment(k) << "\t"
	      << fv5.getDistOfDistanceOfFragment(k) << "\t"
	      << fv6.getDistOfDistanceOfFragment(k) << "\t"
	      << fvback.getDistOfDistanceOfFragment(k) << "\t"
	      << std::endl;
  }
#endif
  
  boost::thread_group agroup;
  boost::mutex mtx;
  for(uint i=0; i<seprange.size(); i++) {
    agroup.create_thread(bind(&genThreadFragVar, boost::ref(chr), boost::ref(mpfv), boost::cref(fwd), boost::cref(rev), boost::cref(fvback), seprange[i].start, seprange[i].end, boost::ref(mtx)));
  }
  agroup.join_all();

  return;
}
