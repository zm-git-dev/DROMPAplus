/* Copyright(c)  Ryuichiro Nakato <rnakato@iqb.u-tokyo.ac.jp>
 * All rights reserved.
 */
#include <algorithm>
#include "GenomeCoverage.hpp"
#include "pw_gv.hpp"
#include "ReadMpbldata.hpp"
#include "../submodules/SSP/src/SeqStats.hpp"

namespace GenomeCov {
  std::vector<BpStatus> makeGcovArray(const Mapfile &p, const SeqStats &chr, const double r4cmp)
  {
    int32_t chrlen(chr.getlen());

    auto array = readMpblBpArray(p.getMpblBinaryDir(), ("chr" + chr.getname()), chrlen, p.wsGenome.getbinsize());
    if(p.isBedOn()) setPeak_to_MpblBpArray(array, chr.getname(), p.getvbedref());

    for (auto strand: {Strand::FWD, Strand::REV}) {
      for (auto &x: chr.getvReadref(strand)) {
	if (x.duplicate) continue;

	BpStatus val;
	if(rand() >= r4cmp) val = BpStatus::COVREAD_ALL;
	else                val = BpStatus::COVREAD_NORM;

	int32_t s(std::max(0, std::min(x.F3, x.F5)));
	int32_t e(std::min(std::max(x.F3, x.F5), chrlen-1));
	//	std::cout << static_cast<int>(val) << "\t"<< x.F3<< "\t"<< x.F5<< "\t"<< s<< "\t"<< e<<std::endl;
	if (s >= chrlen || e < 0) {
	  std::cerr << "Warning: " << chr.getname() << " read " << s <<"-"<< e << " > array size " << chr.getlen() << std::endl;
	}
	for (int32_t i=s; i<=e; ++i) {
	  if (array[i]==BpStatus::MAPPABLE) array[i] = val;
	}
      }
    }
    return array;
  }
}
