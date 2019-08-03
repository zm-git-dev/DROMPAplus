/* Copyright(c)  Ryuichiro Nakato <rnakato@iam.u-tokyo.ac.jp>
 * All rights reserved.
 */
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <boost/filesystem.hpp>
#include "../submodules/SSP/src/ParseMapfile.hpp"
#include "../submodules/SSP/common/BoostOptions.hpp"
#include "pw_makefile.hpp"
#include "version.hpp"
#include "pw_gv.hpp"

void getOpts(Mapfile &p, int32_t argc, char* argv[]);
void setOpts(MyOpt::Opts &);
void init_dump(const Mapfile &p, const MyOpt::Variables &);
void output_stats(const Mapfile &p);
void output_wigstats(const Mapfile &p);

void printVersion()
{
  std::cerr << "parse2wig+ version " << VERSION << std::endl;
  exit(0);
}

void help_global()
{
  auto helpmsg = R"(
===============

Usage: parse2wig+ [option] -i <inputfile> -o <output> --gt <genome_table>)";

  std::cerr << "\nparse2wig v" << VERSION << helpmsg << std::endl;
  return;
}

template <class T>
void CalcDepth(T &obj, const int32_t flen)
{
  uint64_t lenmpbl(obj.getlenmpbl());
  double d = getratio(obj.getnread_nonred(Strand::BOTH) * flen, lenmpbl);
  obj.setdepth(d);
}

void DefineFragmentLength(Mapfile &p)
{
  if (!p.genome.isPaired() && !p.genome.dflen.isnomodel()) {
    strShiftProfile(p.sspst, p.genome, p.getprefix(), "drompa");
  }
  for (auto &x: p.genome.chr) {
    std::cout << x.getname() << "\t" << p.genome.dflen.getflen() << std::endl;
    x.setF5ToRead(p.genome.dflen.getflen());
    x.printvRead();
  }
}

int main(int32_t argc, char* argv[])
{
  Mapfile p;
  getOpts(p, argc, argv);

  clock_t t1,t2;
  t1 = clock();
  read_mapfile(p.genome);
  t2 = clock();
  PrintTime(t1, t2, "read_mapfile");

  t1 = clock();
  p.complexity.checkRedundantReads(p.genome);
  t2 = clock();
  PrintTime(t1, t2, "checkRedundantReads");

  t1 = clock();
  DefineFragmentLength(p);
  t2 = clock();
  PrintTime(t1, t2, "ShiftProfile");

  for (auto &x: p.genome.chr) CalcDepth(x, p.genome.dflen.getflen());
  CalcDepth(p.genome, p.genome.dflen.getflen());

  p.setFRiP();

#ifdef DEBUG
  p.genome.printReadstats();
#endif

  p.calcGenomeCoverage();
  p.normalizeByGCcontents();

  t1 = clock();
  generate_wigfile(p);
  t2 = clock();
  std::cout << "generate_wigfile: " << static_cast<double>(t2 - t1) / CLOCKS_PER_SEC << "sec.\n";

  /*  t1 = clock();
  p.wsGenome.estimateZINB(p.getIdLongestChr());
  t2 = clock();
  std::cout << "estimateZINB: " << static_cast<double>(t2 - t1) / CLOCKS_PER_SEC << "sec.\n";*/

  // p.wsGenome.printPeak(p.getbinprefix());

  if(p.isverbose()) {
    output_wigstats(p);
    p.genome.dflen.outputDistFile(p.getprefix(), p.genome.getnread(Strand::BOTH));
  }
  output_stats(p);

  return 0;
}

void getOpts(Mapfile &p, int32_t argc, char* argv[])
{
  DEBUGprint("setOpts...");

  MyOpt::Opts allopts("Options");
  p.setOpts(allopts);
  setOpts(allopts);

  MyOpt::Variables values;

  DEBUGprint("getOpts...");

  try {
    boost::program_options::parsed_options parsed = parse_command_line(argc, argv, allopts);
    store(parsed, values);
  }
  catch(const boost::program_options::error_with_option_name& e) {
    std::cout << e.what() << std::endl;
    exit(0);
  }
  if (values.count("version")) printVersion();
  if (argc ==1) {
    help_global();
    std::cerr << "Use --help option for more information on the other options\n\n";
    exit(0);
  }
  if (values.count("help")) {
    help_global();
    std::cout << "\n" << allopts << std::endl;
    exit(0);
  }
  std::vector<std::string> opts = {"input", "output", "gt"};
  for (auto x: opts) {
    if (!values.count(x)) PRINTERR("specify --" << x << " option.");
  }

  try {
    notify(values);
    p.setValues(values);

    boost::filesystem::path dir(MyOpt::getVal<std::string>(values, "odir"));
    boost::filesystem::create_directory(dir);

    init_dump(p, values);
  } catch(const boost::bad_any_cast& e) {
    std::cout << e.what() << std::endl;
    exit(0);
  }

  DEBUGprint("getOpts done.");
  return;
}

void setOpts(MyOpt::Opts &allopts)
{
  MyOpt::setOptIO(allopts, "parse2wigdir+");
  MyOpt::setOptPair(allopts);
  MyOpt::setOptOther(allopts);
  return;
}

void init_dump(const Mapfile &p, const MyOpt::Variables &values)
{
  std::cout << boost::format("\n======================================\n");
  std::cout << boost::format("parse2wig+ version %1%\n\n") % VERSION;

  MyOpt::dumpIO(values);
  p.wsGenome.dump();
  MyOpt::dumpGenomeTable(values);

  MyOpt::dumpFragmentLengthDist(values);
  MyOpt::dumpPair(values);
  MyOpt::dumpLibComp(values);

  p.rpm.dump();
  p.dump();

  MyOpt::dumpOther(values);
  printf("======================================\n");
  return;
}

template <class T, class S>
void print_SeqStats(std::ofstream &out, const T &p, const S &gcov, const Mapfile &mapfile)
{
  /* genome data */
  out << p.getname() << "\t" << p.getlen()  << "\t" << p.getlenmpbl() << "\t" << p.getpmpbl() << "\t";
  /* total reads*/
  out << boost::format("%1%\t%2%\t%3%\t%4$.1f%%\t")
    % p.getnread(Strand::BOTH) % p.getnread(Strand::FWD) % p.getnread(Strand::REV)
    % getpercent(p.getnread(Strand::BOTH), mapfile.genome.getnread(Strand::BOTH));

  std::vector<Strand::Strand> vstr = {Strand::BOTH, Strand::FWD, Strand::REV};
  for (auto strand: vstr) printNumandPer(out, p.getnread_nonred(strand), p.getnread(strand));
  for (auto strand: vstr) printNumandPer(out, p.getnread_red(strand),    p.getnread(strand));

  /* reads after GCnorm */
  if(mapfile.gc.isGcNormOn()) {
    for (auto strand: vstr) printNumandPer(out, p.getnread_afterGC(strand), p.getnread(strand));
  }
  out << boost::format("%1$.3f\t") % p.getdepth();
  if(p.getsizefactor()) out << boost::format("%1$.3f\t") % p.getsizefactor();
  else                  out << " - \t";
  if(mapfile.rpm.getType() == "NONE") out << p.getnread_nonred(Strand::BOTH) << "\t";
  else out << p.getnread_rpm(Strand::BOTH) << "\t";

  gcov.printstats(out);

  if(mapfile.isBedOn()) out << boost::format("%1%\t%2$.3f\t") % p.getnread_inbed() % p.getFRiP();

  return;
}

void output_stats(const Mapfile &p)
{
  std::string filename = p.getbinprefix() + ".tsv";
  std::ofstream out(filename);

  out << "parse2wig version " << VERSION << std::endl;
  out << "Input file: \"" << p.genome.getInputfile() << "\"" << std::endl;
  out << "Redundancy threshold: >" << p.complexity.getThreshold() << std::endl;

  p.complexity.print(out);
  p.genome.dflen.printFlen(out);
  if(p.gc.isGcNormOn()) out << "GC summit: " << p.getmaxGC() << std::endl;

  // Global stats
  out << "\n\tlength\tmappable base\tmappability\t";
  out << "total reads\t\t\t\t";
  out << "nonredundant reads\t\t\t";
  out << "redundant reads\t\t\t";
  if(p.gc.isGcNormOn()) out << "reads (GCnormed)\t\t\t";
  out << "read depth\t";
  out << "scaling weight\t";
  out << "normalized read number\t";
  p.gcov.printhead(out);
  if(p.isBedOn()) out << "reads in peaks\tFRiP";
  //  out << "\tbin mean\tbin variance";
  // out << "\tnb_p\tnb_n\tnb_p0";
  out << std::endl;
  out << "\t\t\t\t";
  out << "both\tforward\treverse\t% genome\t";
  out << "both\tforward\treverse\t";
  out << "both\tforward\treverse\t";
  if(p.gc.isGcNormOn()) out << "both\tforward\treverse\t";
  out << std::endl;

  // SeqStats
  print_SeqStats(out, p.genome, p.gcov, p);
  //  p.wsGenome.genome.printPoispar(out);
  // p.wsGenome.genome.printZINBpar(out);
  out << std::endl;

  for(size_t i=0; i<p.genome.chr.size(); ++i) {
    print_SeqStats(out, p.genome.chr[i], p.gcov.chr[i], p);
    //   p.wsGenome.chr[i].printPoispar(out);
    // p.wsGenome.chr[i].printZINBpar(out);
    out << std::endl;
  }

  std::cout << "stats is output in " << filename << "." << std::endl;

  return;
}

void output_wigstats(const Mapfile &p)
{
  std::string filename = p.getbinprefix() + ".readcountdist.tsv";
  std::ofstream out(filename);

  std::cout << "generate " << filename << ".." << std::flush;

  out << "\tGenome\t\t";
  for (auto &x: p.genome.chr) out << x.getname() << "\t\t";
  out << std::endl;
  out << "read number\tnum of bins genome\tprop\t";  // ZINB estimated\t
  for (size_t i=0; i<p.genome.chr.size(); ++i) out << "num of bins\tprop\t"; // \tPoisson estimated\tZINB estimated
  out << std::endl;

  for(int32_t i=0; i<p.wsGenome.getWigDistsize(); ++i) {
    out << i << "\t";
    //    p.wsGenome.genome.printWigDist(out, i);
    //  out << p.wsGenome.genome.getZINB(i) << "\t";
    //    out << p.genome.getZIP(i) << "\t";
    for (auto &x: p.wsGenome.chr) {
      x.printWigDist(out, i);
      //  out << x.getPoisson(i) << "\t";
      // out << x.getZINB(i) << "\t";
    }
    out << std::endl;
  }

  std::cout << "done." << std::endl;
  return;
}

void Mapfile::setValues(const MyOpt::Variables &values)
{
  DEBUGprint("Mapfile setValues...");

  on_bed = values.count("bed");
  if(on_bed) {
    bedfilename = MyOpt::getVal<std::string>(values, "bed");
    isFile(bedfilename);
    vbed = parseBed<bed>(bedfilename);
  }

  if (values.count("mp")) mpdir = MyOpt::getVal<std::string>(values, "mp");
  mpthre = MyOpt::getVal<double>(values, "mpthre");

  genome.setValues(values);
  wsGenome.setValues(values, genome.chr);

  //  for (auto &x: genome.chr) wsGenome.chr.emplace_back(x.getlen(), wsGenome.getbinsize());

  rpm.setValues(values);
  complexity.setValues(values);
  sspst.setValues(values);
  gc.setValues(values);

  verbose = values.count("verbose");

  samplename = MyOpt::getVal<std::string>(values, "output");
  id_longestChr = setIdLongestChr(genome);
  oprefix = MyOpt::getVal<std::string>(values, "odir") + "/" + MyOpt::getVal<std::string>(values, "output");
  obinprefix = oprefix + "." + std::to_string(MyOpt::getVal<int32_t>(values, "binsize"));

  DEBUGprint("Mapfile setValues done.");
}
