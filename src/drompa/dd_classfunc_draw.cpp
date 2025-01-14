/* Copyright(c) Ryuichiro Nakato <rnakato@iqb.u-tokyo.ac.jp>
 * All rights reserved.
 */
#include "dd_gv.hpp"
#include "dd_draw_environment_variable.hpp"

using namespace DROMPA;

int32_t DrawParam::getHeightEachSample(const SamplePairEach &pair) const
{
  int32_t height(0);
  int32_t n(0);
  if (showctag)                          { height += getHeightDf(); ++n; }
  if (showitag==1 && pair.InputExists()) { height += getHeightDf(); ++n; }
  if (showratio   && pair.InputExists()) { height += getHeightDf(); ++n; }
  if (showpinter)                        { height += getHeightDf(); ++n; }
  if (showpenrich && pair.InputExists()) { height += getHeightDf(); ++n; }
  height += MERGIN_BETWEEN_DATA * (n-1);

#ifdef DEBUG
  std::cout << "LineHeight: " << getHeightDf() << ", n: " << n << std::endl;
  std::cout << "HeightEachSample: " << height << std::endl;
#endif
  return height;
}

int32_t DrawParam::getHeightAllSample(const Global &p, const std::vector<SamplePairOverlayed> &pairs) const
{
  int32_t height(0);
  for (auto &x: pairs) height += getHeightEachSample(x.first);
  height += MERGIN_BETWEEN_DATA * (samplenum-1);
  if (showitag==2) height += getHeightDf() + MERGIN_BETWEEN_DATA;

  if (p.anno.showIdeogram()) height += BOXHEIGHT_IDEOGRAM + MERGIN_BETWEEN_GRAPH_DATA;
  if (p.anno.GC.isOn()) height += BOXHEIGHT_GRAPH + MERGIN_BETWEEN_GRAPH_DATA;
  if (p.anno.GD.isOn()) height += BOXHEIGHT_GRAPH + MERGIN_BETWEEN_GRAPH_DATA;

  if (p.anno.genefile != "" || p.anno.arsfile != "" || p.anno.terfile != "") {
    height += BOXHEIGHT_GENEBOX_EXON + MERGIN_BETWEEN_DATA;
  }
  height += (BOXHEIGHT_BEDANNOTATION + MERGIN_BETWEEN_READ_BED + 2) * p.anno.vbedlist.size() + 15;
  height += (BOXHEIGHT_BEDANNOTATION + MERGIN_BETWEEN_READ_BED + 2) * p.anno.vbed12list.size() + 15;
  height += (BOXHEIGHT_INTERACTION   + MERGIN_BETWEEN_READ_BED + 5) * p.anno.vinterlist.size() + 15;

  if (p.anno.existChIADrop()) height += BOXHEIGHT_ChIADROP + MERGIN_BETWEEN_READ_BED;

#ifdef DEBUG
  std::cout << "HeightAllSample; " << height << std::endl;
#endif
  return height;
}

int32_t DrawParam::getPageHeight(const Global &p, const std::vector<SamplePairOverlayed> &pairs) const
{
  int32_t height(OFFSET_Y*2);
  height += getHeightAllSample(p, pairs) * linenum_per_page;
  height += MERGIN_BETWEEN_EACHLAYER * (linenum_per_page-1);
  height += 50; // 保険

#ifdef DEBUG
  std::cout << "PageHeight: " << height << std::endl;
#endif
  return height;
}
