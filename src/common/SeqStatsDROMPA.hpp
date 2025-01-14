/* Copyright(c)  Ryuichiro Nakato <rnakato@iqb.u-tokyo.ac.jp>
 * All rights reserved.
 */
#ifndef _SEQSTATSDROMPA_HPP_
#define _SEQSTATSDROMPA_HPP_

#include "../submodules/SSP/src/SeqStats.hpp"
#include "../submodules/SSP/src/MThread.hpp"
#include "../submodules/SSP/src/Mapfile.hpp"

class bed;

class AnnotationSeqStatsGenome {
  uint64_t nread_inbed;
  int32_t cov_of_peakregion;
  double sizefactor;
  SeqStats &chr;

  public:
  AnnotationSeqStatsGenome(SeqStats &_chr):
    nread_inbed(0), cov_of_peakregion(0), sizefactor(0), chr(_chr)
  {}

  uint64_t getnread_inbed() const { return nread_inbed; }
  int32_t get_cov_of_peakregion() const { return cov_of_peakregion; }
  double getsizefactor() const { return sizefactor; }

//  void setFRiP(const std::vector<bed> &vbed, const uint64_t len, const std::string &name, strandData *seq);
  void setFRiP(const std::vector<bed> &vbed, strandData *seq);

  void setsizefactor(const double w) {
    sizefactor = w;
    for (auto strand: {Strand::FWD, Strand::REV})
      chr.seq[strand].nread_rpm = chr.seq[strand].nread_nonred * sizefactor;
  }

  double getFRiP() const {
    return getratio(getnread_inbed(), getnread_nonred(Strand::BOTH));
  }

  uint64_t getlen()       const { return chr.getlen(); }
  uint64_t getlenmpbl()   const { return chr.getlenmpbl(); }
  double   getpmpbl()     const { return chr.getpmpbl(); }
  double   getdepth()     const { return chr.getdepth(); }
  uint64_t getnread (const Strand::Strand strand) const { return chr.getnread(strand);}
  uint64_t getnread_nonred (const Strand::Strand strand) const { return chr.getnread_nonred(strand); }
  uint64_t getnread_red (const Strand::Strand strand) const { return chr.getnread_red(strand); }
  uint64_t getnread_rpm (const Strand::Strand strand) const { return chr.getnread_rpm(strand); }
  uint64_t getnread_afterGC (const Strand::Strand strand) const { return chr.getnread_afterGC(strand); }
  const std::string & getname() const { return chr.getname(); }
};

class SeqStatsGenome : public SeqStatsGenomeSSP {
  double sizefactor;
  std::vector<AnnotationSeqStatsGenome> annoChr;

 public:

  SeqStatsGenome():
    SeqStatsGenomeSSP(),
    sizefactor(0)
  {}

  void initannoChr() {
    for(size_t i=0; i<chr.size(); ++i) annoChr.emplace_back(chr[i]);
  }

  void setsizefactor(const double w, const int32_t i) { annoChr[i].setsizefactor(w); }
  void setsizefactor(const double w) { sizefactor = w; }

  void setFRiP(const std::vector<bed> &vbed) {
    for(size_t i=0; i<annoChr.size(); ++i) annoChr[i].setFRiP(vbed, chr[i].seq);
  }

  uint64_t getnread_inbed() const {
    uint64_t nread(0);
    for(auto &x: annoChr) nread += x.getnread_inbed();
    return nread;
  }
  uint64_t getnread_inbed(const int32_t i) const {
    return annoChr[i].getnread_inbed();
  }

  int32_t get_cov_of_peakregion() const {
    int32_t nread(0);
    for(auto &x: annoChr) nread += x.get_cov_of_peakregion();
    return nread;
  }
  int32_t get_cov_of_peakregion(const int32_t i) const {
    return annoChr[i].get_cov_of_peakregion();
  }

  const AnnotationSeqStatsGenome &getannochr(const int32_t i) const { return annoChr[i];}


  double getFRiP() const {
    return getratio(getnread_inbed(), getnread_nonred(Strand::BOTH));
  }

  double getsizefactor() const { return sizefactor; }
  double getsizefactor(const int32_t i) const { return annoChr[i].getsizefactor(); }

  void strShiftProfile(SSPstats &sspst, const std::string &head, const bool isallchr, const bool isverbose);

};


#endif /* _SEQSTATSDROMPA_HPP_ */
