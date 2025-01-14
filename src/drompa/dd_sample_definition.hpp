/* Copyright(c) Ryuichiro Nakato <rnakato@iqb.u-tokyo.ac.jp>
 * All rights reserved.
 */
#ifndef _DD_SAMPLE_DEFINITION_H_
#define _DD_SAMPLE_DEFINITION_H_

#include <unordered_map>
#include "WigStats.hpp"
#include "extendBedFormat.hpp"
#include "util.hpp"

class vChrArray;

class SampleInfo {
  WigType iftype;
  int32_t binsize;
  int32_t totalreadnum;
  std::unordered_map<std::string, int32_t> totalreadnum_chr;
  std::string prefix;

  void setbinsize(std::string &v, const int32_t b);

public:
  std::vector<int> data;

  SampleInfo() {}
  SampleInfo(const std::string &filename,
             const std::vector<chrsize> &gt,
             const int32_t b,
             const WigType &type);

  void scanStatsFile(const std::string &filename);
  void gettotalreadnum(const std::string &filename, const std::vector<chrsize> &gt);
  int32_t getbinsize() const { return binsize; }
  WigType getiftype() const { return iftype; }

  int32_t gettotalreadnum() const { return totalreadnum; }
  const std::unordered_map<std::string, int32_t>& gettotalreadnum_chr() const & {
    return totalreadnum_chr;
  }
};

class vSampleInfo {
  std::unordered_map<std::string, SampleInfo> vsinfo;

public:
  vSampleInfo(){}

  void addSampleInfo(const std::string &str, const std::vector<chrsize> &gt, const WigType iftype);
  bool Exists(const std::string &str) const {
    return vsinfo.find(str) != vsinfo.end();
  }
  int32_t getbinsize(const std::string &str) const {
    return vsinfo.at(str).getbinsize();
  }
  const std::unordered_map<std::string, SampleInfo> &getarray() const & {
    return vsinfo;
  }
  const SampleInfo& operator[](const std::string &str) const& {
    return vsinfo.at(str);
  }
};

class SamplePairEach {
  FILE* File;
  std::string genwig_filename;
  std::string genwig_tmpfile;
  WigType oftype;
  int32_t ofvaluetype;

  int32_t binsize;

  std::unordered_map<std::string, std::vector<bed>> vbedregions;
  std::unordered_map<std::string, std::vector<Peak>> vPeak;

  class yScale {
  public:
    double tag;
    double ratio;
    double pvalue;
    yScale(): tag(0), ratio(0), pvalue(0) {}
  };

public:
  std::string argvChIP, argvInput;
  std::string peak_argv;
  std::string label;
  double ratio;
  yScale scale;

  SamplePairEach():
    genwig_filename(""), oftype(WigType::BEDGRAPH), binsize(0),
    argvChIP(""), argvInput(""), peak_argv(""),
    label(""), ratio(1)
  {}
  SamplePairEach(const std::string &str, const vSampleInfo &vsinfo);

  void setScalingFactor(const int32_t normtype, const vChrArray &vReadArray, const std::string &chrname);

  void genEnrichWig(const vChrArray &vReadArray, const std::string &chrname, const int32_t chrlen);

  void peakcall_withInput(const vChrArray &vReadArray, const std::string &chrname,
                          const double pthre_inter, const double pthre_enrich,
                          const double ethre, const double ipm);
  void peakcall_onlyChIP(const vChrArray &vReadArray, const std::string &chrname, const double pthre_inter, const double ipm);

  void printPeak(const std::string &prefix) const {
    std::string baselabel = basename(label);
    std::string filename = prefix + "." + baselabel + ".peak.tsv";
    std::string filenameBED = prefix + "." + baselabel + ".peak.bed";

    DEBUGprint("Peakfile name: " << filename);
    std::ofstream out(filename);

    Peak v;
    v.printHead(out);
    int32_t num(0);
    for (auto &x: vPeak) {
      for (auto &peak: x.second) peak.print(out, num++);
    }
    out.close();

    // make BED file from TSV file
    std::string command("grep -v \\# " + filename + " | grep -v chromosome | cut -f1-3 > " + filenameBED);
    if(system(command.c_str())) std::cerr << "Warning: peak BED file cannot be generated.";
  }

  std::vector<bed> getBedChr(const std::string &chrname) const {
    if (vbedregions.find(rmchr(chrname)) != vbedregions.end()) return vbedregions.at(rmchr(chrname));
    else return std::vector<bed>();
  }
  std::vector<Peak> getPeakChr(const std::string &chrname) const {
    if (vPeak.find(chrname) != vPeak.end()) return vPeak.at(chrname);
    else return std::vector<Peak>();
  }
  void print() const;
  int32_t getbinsize() const { return binsize; }
  bool BedExists() const { return peak_argv != ""; }
  bool InputExists() const { return argvInput != ""; }

  void genwig_openfilestream(const std::string &prefix, WigType _oftype, int32_t _ofvaluetype);
  void sort_bedGraph(const std::string &filename);
  void genwig_closefilestream(const std::string &genometablefilename);

};

class SamplePairOverlayed {
  bool overlay;

 public:
  SamplePairEach first;
  SamplePairEach second;

  SamplePairOverlayed(const std::string &str, const vSampleInfo &vsinfo):
    overlay(false), first(str, vsinfo)
  {}

  void setSecondSample(const std::string &str, const vSampleInfo &vsinfo) {
    second = SamplePairEach(str, vsinfo);
    overlay = true;
  }

  void print() const {
    first.print();
    if (overlay) {
      printf("Overlay ");
      second.print();
    }
  }
  bool OverlayExists() const { return overlay; }
};

#endif // _DD_SAMPLE_DEFINITION_H_
