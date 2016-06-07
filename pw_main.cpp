/* Copyright(c)  Ryuichiro Nakato <rnakato@iam.u-tokyo.ac.jp>
 * This file is a part of DROMPA sources.
 */
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include "pw_readmapfile.h"
#include "util.h"
#include "common.h"
#include "warn.h"
#include "macro.h"
#include "readdata.h"
#include "pw_makefile.h"
#include "pw_gc.h"

using namespace std;
using namespace boost::program_options;

variables_map getOpts(int argc, char* argv[]);
void setOpts(options_description &);
void init_dump(const variables_map &);
void output_stats(const variables_map &values, Mapfile &p);

Mapfile::Mapfile(const variables_map &values):
  genome("Genome"), thre4filtering(0), nt_all(0), nt_nonred(0), nt_red(0), tv(0), r4cmp(0)
{
  vector<string> v;
  string lineStr;

  // genome_table
  ifstream in(values["gt"].as<string>());
  if(!in) PRINTERR("Could nome open " << values["gt"].as<string>() << ".");
  while (!in.eof()) {
    getline(in, lineStr);
    if(lineStr.empty() || lineStr[0] == '#') continue;
    boost::split(v, lineStr, boost::algorithm::is_any_of("\t"));
    SeqStats s(v[0], stoi(v[1]));
    chr.push_back(s);
    lastchr = v[0];
  }

  // mappability
  if (values.count("mp")) {
    string mpfile = values["mp"].as<string>() + "/map_fragL150_genome.txt";
    ifstream in_mpbl(mpfile);
    if(!in_mpbl) PRINTERR("Could nome open " << mpfile << ".");
    while (!in_mpbl.eof()) {
      getline(in_mpbl, lineStr);
      if(lineStr.empty() || lineStr[0] == '#') continue;
      boost::split(v, lineStr, boost::algorithm::is_any_of("\t"));
      for(auto &x:chr) {
	if(x.name == v[0]) x.len_mpbl = stoi(v[1]);
      }
    }
  }

  for(auto &x:chr) {
    genome.len += x.len;
    genome.len_mpbl += x.len_mpbl;
    genome.nbin += x.nbin = x.len/values["binsize"].as<int>() +1;
    x.p_mpbl = x.len_mpbl/(double)x.len;
    genome.p_mpbl = genome.len_mpbl/(double)genome.len;
  }

  dist.eflen = values["flen"].as<int>();
}

void printVersion()
{
  cerr << "parse2wig version " << VERSION << endl;
  exit(0);
}  

void help_global()
{
  auto helpmsg = R"(
===============

Usage: parse2wig+ [option] -i <inputfile> -o <output> -gt <genome_table>)";
  
  cerr << "\nparse2wig v" << VERSION << helpmsg << endl;
  return;
}

void calcGenomeCoverage(const variables_map &values, Mapfile &p)
{
  cout << "calculate genome coverage.." << flush;
  for (auto &chr:p.chr) {
    vector<char> array;
    if(values.count("mp")) array = readMpbl_binary(values["mp"].as<string>(), chr.name, chr.len);
    else array = readMpbl_binary(chr.len);
    if(values.count("bed")) arraySetBed(array, chr.name, p.vbed);

    for(int strand=0; strand<STRANDNUM; ++strand) {
      for (auto &x: chr.seq[strand].vRead) {
	if(x.duplicate) continue;
	int s(min(x.F3, x.F5));
	int e(max(x.F3, x.F5));
	for(int i=s; i<=e; ++i) if(array[i]==MAPPABLE) array[i]=COVREAD;
      }
    }
    int n(0), n1(0);
    for(int i=0; i<chr.len; ++i) {
      if(array[i] == MAPPABLE || array[i] == COVREAD) ++n1;
      if(array[i] == COVREAD) ++n;
    }
    chr.gcov = n/(double)n1;
  }
  cout << "done." << endl;
  return;
}

int main(int argc, char* argv[])
{
  variables_map values = getOpts(argc, argv);
  
  boost::filesystem::path dir(values["odir"].as<string>());
  boost::filesystem::create_directory(dir);

  /* read mapfile */
  Mapfile p(values);
  read_mapfile(values, p);

  /*  if(p->ccp) pw_ccp(p, mapfile, g->chrmax, (int)g->chr[g->chrmax].len);*/

  // BED file
  if (values.count("bed")) {
    string bedfile = values["bed"].as<string>();
    isFile(bedfile);
    p.vbed = parseBed<bed>(bedfile);
    //    printBed(p.vbed);
    p.calcFRiP();
  }

  // Genome coverage
  calcGenomeCoverage(values, p);
  
  // GC contents
  if (values.count("genome")) make_GCdist(values, p);

  /* make and output wigdata */
  makewig(values, p);

  /* output stats */
  output_stats(values, p);

  return 0;
}

void checkParam(const variables_map &values)
{
  vector<string> intopts = {"binsize", "flen", "maxins" , "nrpm", "flen4gc"};
  for (auto x: intopts) chkminus<int>(values, x, 0);
  vector<string> intopts2 = {"rcenter", "thre_pb"};
  for (auto x: intopts2) chkminus<int>(values, x, -1);
  vector<string> dbopts = {"ndepth", "mpthre"};
  for (auto x: dbopts) chkminus<double>(values, x, 0);
  
  if(!RANGE(values["of"].as<int>(), 0, PWFILETYPENUM-1)) printerr("invalid wigfile type.\n");

  string ftype = values["ftype"].as<string>();
  if(ftype != "SAM" && ftype != "BAM" && ftype != "BOWTIE" && ftype != "TAGALIGN") printerr("invalid --ftype.\n");
  string ntype = values["ntype"].as<string>();
  if(ntype != "NONE" && ntype != "GR" && ntype != "GD" && ntype != "CR" && ntype != "CD") printerr("invalid --ntype.\n");

  if (values.count("genome")) {
    if(!values.count("mpbin")) printerr("-GC option requires -mpbin option.\n");
    isFile(values["genome"].as<string>());
  }

  return;
}

variables_map getOpts(int argc, char* argv[])
{
  options_description allopts("Options");
  setOpts(allopts);
  
  variables_map values;
  
  try {
    parsed_options parsed = parse_command_line(argc, argv, allopts);
    store(parsed, values);
    
    if (values.count("version")) printVersion();
    
    if (argc ==1) {
      help_global();
      cerr << "Use --help option for more information on the other options\n\n";
      exit(0);
    }
    if (values.count("help")) {
      help_global();
      cout << "\n" << allopts << endl;
      exit(0);
    }
    vector<string> opts = {"input", "output", "gt"};
    for (auto x: opts) {
      if (!values.count(x)) PRINTERR("specify --" << x << " option.");
    }

    notify(values);

    checkParam(values);
    init_dump(values);
  } catch (exception &e) {
    cout << e.what() << endl;
  }
  return values;
}

void setOpts(options_description &allopts){
  options_description optreq("Required",100);
  optreq.add_options()
    ("input,i",   value<string>(), "Mapping file. Multiple files are allowed (separated by ',')")
    ("output,o",  value<string>(), "Prefix of output files")
    ("gt",        value<string>(), "Genome table (tab-delimited file describing the name and length of each chromosome)")
    ;
  options_description optIO("Input/Output",100);
  optIO.add_options()
    ("binsize,b",   value<int>()->default_value(100),	  "bin size")
    ("ftype,f",     value<string>()->default_value("SAM"), "{SAM|BAM|BOWTIE|TAGALIGN}: format of input file (default:SAM)\nTAGALIGN could be gzip'ed (extension: tagAlign.gz)")
    ("of",        value<int>()->default_value(0),	  "output format\n   0: binary (.bin)\n   1: compressed wig (.wig.gz)\n   2: uncompressed wig (.wig)\n   3: bedGraph (.bedGraph)\n   4: bigWig (.bw)")
    ("odir",        value<string>()->default_value("parse2wigdir"),	  "output directory name")
    ("rcenter", value<int>()->default_value(0), "consider length around the center of fragment ")
    ;
  options_description optsingle("For single-end read",100);
  optsingle.add_options()
    ("flen",        value<int>()->default_value(150), "expected fragment length\n(Automatically calculated in paired-end mode)")
    ;
  options_description optpair("For paired-end read",100);
  optpair.add_options()
    ("pair", 	  "add when the input file is paired-end")
    ("maxins",        value<int>()->default_value(500), "maximum fragment length")
    ;
  options_description optpcr("PCR bias filtering",100);
  optpcr.add_options()
    ("nofilter", 	  "do not filter PCR bias")
    ("thre_pb",        value<int>()->default_value(0),	  "PCRbias threshold (default: more than max(1 read, 10 times greater than genome average)) ")
    ("ncmp",        value<int>()->default_value(10000000),	  "read number for calculating library complexity")
    ;
  options_description optnorm("Total read normalization",100);
  optnorm.add_options()
    ("ntype,n",        value<string>()->default_value("NONE"),  "Total read normalization\n{NONE|GR|GD|CR|CD}\n   NONE: not normalize\n   GR: for whole genome, read number\n   GD: for whole genome, read depth\n   CR: for each chromosome, read number\n   CD: for each chromosome, read depth")
    ("nrpm",        value<int>()->default_value(20000000),	  "Total read number after normalization")
    ("ndepth",      value<double>()->default_value(1.0),	  "Averaged read depth after normalization")
    ("bed",        value<string>(),	  "specify the BED file of enriched regions (e.g., peak regions)")
    ;  
  options_description optmp("Mappability normalization",100);
  optmp.add_options()
    ("mp",        value<string>(),	  "Mappability file")  // mpbin ni touitu?
    ("mpthre",    value<double>()->default_value(0.3),	  "Threshold of low mappability regions")
    ("mpbin",        value<string>(),	  "mpbinaryfile")
    ;
  options_description optgc("GC bias normalization\n   (require large time and memory)",100);
  optgc.add_options()
    ("genome",     value<string>(),	  "reference genome sequence for GC content estimation")
    ("flen4gc",    value<int>()->default_value(120),  "fragment length for calculation of GC distribution")
    ("gcdepthoff", "do not consider depth of GC contents")
    ;
  options_description optother("Others",100);
  optother.add_options()
    ("ccp", 	  "make cross-correlation profile")
    ("version,v", "print version")
    ("help,h", "show help message")
    ;
  allopts.add(optreq).add(optIO).add(optsingle).add(optpair).add(optpcr).add(optnorm).add(optmp).add(optgc).add(optother);
  return;
}

void init_dump(const variables_map &values){
  vector<string> str_wigfiletype = {"BINARY", "COMPRESSED WIG", "WIG", "BEDGRAPH", "BIGWIG"};

  BPRINT("\n======================================\n");
  BPRINT("parse2wig version %1%\n\n") % VERSION;
  BPRINT("Input file %1%\n")         % values["input"].as<string>();
  BPRINT("\tFormat: %1%\n")          % values["ftype"].as<string>();
  BPRINT("Output file: %1%/%2%\n")   % values["odir"].as<string>() % values["output"].as<string>();
  BPRINT("\tFormat: %1%\n")          % str_wigfiletype[values["of"].as<int>()];
  BPRINT("Genome-table file: %1%\n") % values["gt"].as<string>();
  BPRINT("Binsize: %1% bp\n")        % values["binsize"].as<int>();
  if (!values.count("pair")) {
    cout << "Single-end mode: ";
    BPRINT("Predefined fragment length: %1%\n") % values["flen"].as<int>();
  } else {
    cout << "Paired-end mode: ";
    BPRINT("Maximum fragment length: %1%\n") % values["maxins"].as<int>();
  }
  if (!values.count("nofilter")) {
    BPRINT("PCR bias filtering: ON\n");
    if (values["thre_pb"].as<int>()) BPRINT("PCR bias threshold: > %1%\n") % values["thre_pb"].as<int>();
  }
  BPRINT("\t%1% reads used for library complexity\n") % values["ncmp"].as<int>();
  if (values.count("bed")) BPRINT("Bed file: %1%\n") % values["bed"].as<string>();
  if (values.count("ccp")) BPRINT("Make cross-correlation profile.\n");

  string ntype = values["ntype"].as<string>();
  BPRINT("\nTotal read normalization: %1%\n") % ntype;
  if(ntype == "GR" || ntype == "CR"){
    BPRINT("\tnormed read: %1% M for genome\n") % (values["nrpm"].as<int>() /(double)NUM_1M);
  }
  else if(ntype == "GD" || ntype == "CD"){
    BPRINT("\tnormed depth: %1%\n") % values["ndepth"].as<double>();
  }
  printf("\n");
  if (values.count("mp")) {
    printf("Mappability normalization:\n");
    BPRINT("\tfile prefix: %1%\n") % values["mp"].as<string>();
    BPRINT("\tLow mappablitiy threshold: %1%\n") % values["mpthre"].as<double>();
  }
  if (values.count("genome")) {
    printf("Correcting GC bias:\n");
    BPRINT("\tChromosome directory: %1%\n") % values["genome"].as<string>();
    BPRINT("\tmappability binary prefix: %1%\n") % values["mpbin"].as<string>();
    BPRINT("\tLength for GC distribution: %1%\n") % values["flen4gc"].as<int>();
  }
  printf("======================================\n");
  return;
}

void print_SeqStats(const variables_map &values, ofstream &out, SeqStats &p, Mapfile &mapfile)
{
  /* genome data */
  out << p.name << "\t" << p.len  << "\t" << p.len_mpbl << "\t" << p.p_mpbl << "\t";
  /* total reads*/
  out << boost::format("%1%\t%2%\t%3%\t%4$.1f%%\t")
    % p.bothnread() % p.seq[STRAND_PLUS].nread % p.seq[STRAND_MINUS].nread
    % (p.bothnread()*100/(double)mapfile.genome.bothnread());

  /* nonredundant reads */
  printr(out, p.bothnread_nonred(), p.bothnread());
  p.seq[STRAND_PLUS].printnonred(out);
  p.seq[STRAND_MINUS].printnonred(out);
  printr(out, p.bothnread_red(), p.bothnread());
  p.seq[STRAND_PLUS].printred(out);
  p.seq[STRAND_MINUS].printred(out);
  /* reads after GCnorm */
  if(values.count("genome")) {
    printr(out, p.bothnread_afterGC(), p.bothnread());
    p.seq[STRAND_PLUS].printafterGC(out);
    p.seq[STRAND_MINUS].printafterGC(out);
  }
  /* read depth */
  //out << p.depth <<"\t";
  out << boost::format("%1$.3f\t") % p.depth;
  /* weight */
  if(p.w) out << boost::format("%1$.3f\t") % p.w; else out << " - \t";
  /* read number after rpkm normalization */
  if(values["ntype"].as<string>() == "NONE") out << p.bothnread_nonred() << "\t"; else out << p.bothnread_rpm() << "\t";
  /* genome coverage */
  out << p.gcov << "\t";
  /* FRiP */
  if(values.count("bed")) out << boost::format("%1$.3f\t") % p.FRiP;

  out << endl;
  return;
}

void output_stats(const variables_map &values, Mapfile &p)
{
  string filename = values["odir"].as<string>() + "/" + values["output"].as<string>() + "." + IntToString(values["binsize"].as<int>()) + ".csv";
  ofstream out(filename);

  out << "parse2wig version " << VERSION << endl;
  out << "Input file: \"" << values["input"].as<string>() << "\"" << endl;
  out << "Redundancy threshold: >" << p.thre4filtering << endl;

  if(p.tv) out << boost::format("Library complexity: (%1$.3f) (%2%/%3%)\n") % p.complexity() % p.nt_nonred % p.nt_all;
  else     out << boost::format("Library complexity: %1$.3f (%2%/%3%)\n")   % p.complexity() % p.nt_nonred % p.nt_all;
 
  //  if(values.count("genome")) out << "GC summit: %d\n", mapfile->maxGC);
  // out << "Poisson: lambda = %f\n", mapfile->wstats.genome->ave);
  // out << "Negative binomial: p=%f, n=%f, p0=%f\n", mapfile->wstats.genome->nb_p, mapfile->wstats.genome->nb_n, mapfile->wstats.genome->nb_p0);

  /* Global stats */
  out << "\n\tlength\tmappable base\tmappability\t";
  out << "total reads\t\t\t\t";
  out << "nonredundant reads\t\t\t";
  out << "redundant reads\t\t\t";
  if(values.count("genome")) out << "reads (GCnormed)\t\t\t";
  out << "read depth\t";
  out << "scaling weight\t";
  out << "normalized read number\t";
  out << "genome coverage\t";
  if(values.count("bed")) out << "FRiP\t";
  out << endl;
  out << "\t\t\t\t";
  out << "both\tforward\treverse\t% genome\t";
  out << "both\tforward\treverse\t";
  out << "both\tforward\treverse\t";
  if(values.count("genome")) out << "both\tforward\treverse\t";
  out << endl;

  /*** SeqStats ***/
  print_SeqStats(values, out, p.genome, p);
  for(auto x:p.chr) print_SeqStats(values, out, x, p);

  cout << "stats is output in " << filename << "." << endl;

  return;
}
