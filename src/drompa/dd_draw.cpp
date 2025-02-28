/* Copyright(c) Ryuichiro Nakato <rnakato@iqb.u-tokyo.ac.jp>
 * All rights reserved.
 */

#include <sstream>
#include <iomanip>
#include "dd_draw.hpp"
#include "dd_draw_pdfpage.hpp"
#include "dd_draw_dataframe.hpp"
#include "color.hpp"
#include "../submodules/SSP/common/inline.hpp"
#include "../submodules/SSP/common/util.hpp"

/*    if (p->gapfile && d->gaparray[i] >= GAP_THRE){
      cr->set_source_rgba(CLR_BLACK, 0.3);
      rel_yline(cr, xcen, yaxis - height_df, height_df);
      cr->stroke();
      } else if(p->mpfile && d->mparray[i] < p->mpthre){
      cr->set_source_rgba(CLR_BLACK, 0.3);
      rel_yline(cr, xcen, yaxis - height_df, height_df);
      cr->stroke();
      }*/

namespace {
  void strokeGraph4EachWindow(const Cairo::RefPtr<Cairo::Context> cr,
                              double x_pre, double y_pre,
                              double x_cen, double y_cen,
                              int32_t bottom)
  {
    if(y_pre > bottom && y_cen > bottom) return;

    double x1(x_pre);
    double x2(x_cen);
    double y1(y_pre);
    double y2(y_cen);
    if (y_pre > bottom || y_cen > bottom) {
      double xlen = abs(x2 - x1);
      double ylen = abs(y2 - y1);
      if (y_pre > bottom) {
        double ydiff(abs(y_cen - bottom));
        x1 = x2 - xlen*(ydiff/ylen);
        y1 = bottom;
      } else {
        double ydiff(abs(y_pre - bottom));
        x2 = x1 - xlen*(ydiff/ylen);
        y2 = bottom;
      }
    }
    cr->move_to(x1, y1);
    cr->line_to(x2, y2);
    cr->stroke();
    return;
  }

  RGB getInterRGB(double val)
  {
    val = val*0.4 + 0.6;
    HSV color(val, 1.0, 1.0);

    return HSVtoRGB(color);
  }

  void showColorBar(const Cairo::RefPtr<Cairo::Context> cr, int32_t x, const int32_t y, const double maxval)
  {
    int32_t barwidth(1);
    cr->set_line_width(barwidth+0.1);

    cr->set_source_rgba(CLR_BLACK, 1);
    showtext_cr(cr, x-36, y+20, "-log(p)", 10);

    for (int32_t i=0; i<=50; ++i) {
      RGB color(getInterRGB(i*0.02));
      cr->set_source_rgba(color.r, color.g, color.b, 0.8);
      rel_yline(cr, x, y, 12);
      cr->stroke();
      cr->set_source_rgba(CLR_BLACK, 1);
      if (!i)    showtext_cr(cr, x-3, y+20, "0", 10);
      if (i==40) showtext_cr(cr, x-3, y+20, float2string(maxval/3, 1), 10);
      x += barwidth;
      cr->stroke();
    }
    return;
  }
}

void GraphData::setValue(const DROMPA::GraphDataFileName &g,
	      const std::string &chr,
	      const int32_t chrlen, const std::string &l,
	      const double ymin, const double ymax)
{
  binsize = g.getbinsize();
  label = l;
  memnum = MEMNUM_GC;
  boxheight = BOXHEIGHT_GRAPH;
  std::string filename(g.getfilename() + "/" + chr + "-bs" + std::to_string(binsize));
  std::ifstream in(filename);
  if (!in) PRINTERR_AND_EXIT("cannot open " << filename);

  array = std::vector<double>(chrlen/binsize +1);

  double maxtemp(0);
  std::string lineStr;
  while (!in.eof()) {
    getline(in, lineStr);
    if (lineStr.empty()) continue;

    std::vector<std::string> v;
    boost::split(v, lineStr, boost::algorithm::is_any_of(" \t"), boost::algorithm::token_compress_on);

    int32_t start(stoi(v[0]));
    if(start % binsize){
      printf("%d %d\n", start, binsize);
      PRINTERR_AND_EXIT("[E]graph: invalid start position or binsize:  " << filename);
    }
    double val(stod(v[1]));
    array[start/binsize] = val;
    if(maxtemp < val) maxtemp = val;
  }
  if (ymax) {
    mmin = ymin;
    mmax = ymax;
  } else {
    mmin = 0;
    if(MEM_MAX_DEFAULT > maxtemp) mmax = MEM_MAX_DEFAULT;
    else mmax = maxtemp;
  }
  mwid = mmax - mmin;
}

void PDFPage::StrokeWidthOfInteractionSite(const bed &site, const double y)
{
  cr->set_line_width(2);
  cr->set_source_rgba(CLR_DARKORANGE, 0.8);
  double s = BP2PIXEL(site.start - par.xstart);
  double e = BP2PIXEL(site.end - par.xstart);
  rel_xline(cr, s, y, e-s);
  cr->stroke();
}

// cr->arc(中心x, 中心y, 半径, start角度, end角度) 角度はラジアン
void PDFPage::drawArc_from_to(const Interaction &inter, const int32_t start, const int32_t end, const int32_t ref_height, const double ref_ytop)
{
  double ytop = ref_ytop + 10;
  int32_t height = ref_height - 20;
  double radius((end - start)/2.0 * par.dot_per_bp); // 半径
  double r = std::min(0.4, height/radius);
  //    printf("r %f %f %d %d %d\n", r, radius, height, start, end);

  cr->set_line_width(3);
  cr->scale(1, r);
  cr->arc(BP2PIXEL((start + end) /2), ytop/r, radius, 0, M_PI);
  cr->stroke();
  cr->scale(1, 1/r);

  // Highlight each site of interaction
  StrokeWidthOfInteractionSite(inter.first, ytop);
  StrokeWidthOfInteractionSite(inter.second, ytop);
}

void PDFPage::drawArc_from_none(const Interaction &inter, const int32_t start, const int32_t end, const int32_t ref_height, const double ref_ytop)
{
  double ytop = ref_ytop + 10;
  int32_t height = ref_height;
  double radius(height*3);
  double r(1/3.0);

  double bp_s(BP2PIXEL(start));
  double bp_e(BP2PIXEL(end));
  double bp_x(bp_s + radius);
  double bp_y(ytop/r);

  cr->set_line_width(4);
  cr->scale(1, r);
  cr->arc(bp_x, bp_y, radius, 0.5*M_PI, M_PI);
  if (bp_e - bp_x > 0) rel_xline(cr, bp_x, bp_y + radius, bp_e - bp_x);
  cr->stroke();
  cr->scale(1, 1/r);

  // bin of interaction
  StrokeWidthOfInteractionSite(inter.first, ytop);
}

void PDFPage::drawArc_none_to(const Interaction &inter, const int32_t start, const int32_t end, const int32_t ref_height, const double ref_ytop)
{
  double ytop = ref_ytop + 10;
  int32_t height = ref_height;
  double radius(height*3);
  double r(1/3.0);

  double bp_s(BP2PIXEL(start));
  double bp_e(BP2PIXEL(end));
  double bp_x(bp_e - radius);
  double bp_y(ytop/r);

  cr->set_line_width(4);
  cr->scale(1, r);
  cr->arc(bp_x, bp_y, radius, 0, 0.5*M_PI);
  if (bp_x - bp_s > 0) rel_xline(cr, bp_x, bp_y + radius, -(bp_x - bp_s));
  cr->stroke();
  cr->scale(1, 1/r);

  // bin of interaction
  StrokeWidthOfInteractionSite(inter.second, ytop);
}

void PDFPage::drawInteraction(const InteractionSet &vinter)
{
  DEBUGprint_FUNCStart();

  int32_t boxheight(BOXHEIGHT_INTERACTION);
  std::string chr(rmchr(chrname));
  double ytop(par.yaxis_now);
  double ycenter(par.yaxis_now + boxheight/2);

  // colorbar
  showColorBar(cr, 70, ycenter-2, vinter.getmaxval());

  // label
  cr->set_source_rgba(CLR_BLACK, 1);
  showtext_cr(cr, 70, ycenter-6, vinter.getlabel(), 12);

  for (auto &x: vinter.getvinter()) {
    //    printList(x.first.chr, x.second.chr, chr);
    if (x.first.chr != chr && x.second.chr != chr) continue;

    // interchromosomalは描画しない
    if (x.first.chr != chr || x.second.chr != chr) continue;

    RGB color(getInterRGB(x.getval()/vinter.getmaxval() *3)); // maxval の 1/3 を色のmax値に設定
    cr->set_source_rgba(color.r, color.g, color.b, 0.8);
    /*    else {   // inter-chromosomal
	  RGB color(getInterRGB(x.getval()/vinter.getmaxval() *3)); // maxval の 1/3 を色のmax値に設定
	  cr->set_source_rgba(color.r, color.g, color.b, 0.8);
	  }*/

    int32_t xcen_head(-1);
    int32_t xcen_tail(-1);
    if (par.xstart <= x.first.summit  && x.first.summit  <= par.xend) xcen_head = x.first.summit  - par.xstart;
    if (par.xstart <= x.second.summit && x.second.summit <= par.xend) xcen_tail = x.second.summit - par.xstart;

    if (xcen_head < 0 && xcen_tail < 0) continue;

    //    printf("%d, %d, %d, %d, %d, %d\n", x.first.start, x.first.summit, x.first.end, x.second.start, x.second.summit, x.second.end);
    if (xcen_head >= 0 && xcen_tail >= 0) drawArc_from_to(x, xcen_head, xcen_tail, boxheight, ytop);
    if (xcen_head > 0 && xcen_tail < 0)   drawArc_from_none(x, xcen_head, par.xend - par.xstart, boxheight, ytop);
    if (xcen_head < 0 && xcen_tail > 0)   drawArc_none_to(x, xcen_head, xcen_tail, boxheight, ytop);
  }
  cr->stroke();
  par.yaxis_now += boxheight;

  DEBUGprint_FUNCend();
  return;
}

void PDFPage::stroke_xaxis(const double y)
{
  int32_t interval_large(setInterval());
  int32_t interval(interval_large/10);

  cr->set_source_rgba(CLR_BLACK, 1);
  for(int32_t i=setline(par.xstart, interval); i<=par.xend; i+=interval) {
//    std::cout << par.xstart << "\t" << interval << "\t" << par.xend << "\t" << i << std::endl;

    double x(BP2PIXEL(i - par.xstart));
    if (!(i%interval_large)) {
      cr->set_line_width(1);
      rel_yline(cr, x, y-4, 8);
    } else {
      cr->set_line_width(0.5);
      rel_yline(cr, x, y-1.5, 3);
    }
    cr->stroke();
  }
  return;
}

void PDFPage::StrokeGraph(const GraphData &graph)
{
  DEBUGprint_FUNCStart();

  int32_t s(par.xstart/graph.binsize);
  int32_t e(par.xend/graph.binsize +1);
  double diff = graph.binsize * par.dot_per_bp;

  double ytop(par.yaxis_now);
  double ycenter(par.yaxis_now + graph.boxheight/2);
  double ybottom(par.yaxis_now + graph.boxheight);

  // graph line
  cr->set_line_width(0.6);
  cr->set_source_rgba(CLR_GREEN, 1);
  double xpre(OFFSET_X);
  double xcen(BP2PIXEL(0.5*graph.binsize));
  double ypre(ybottom - graph.getylen(s));
  for (int32_t i=s; i<e; ++i, xcen += diff) {
    double ycen(ybottom - graph.getylen(i));
    strokeGraph4EachWindow(cr, xpre, ypre, xcen, ycen, ybottom + 10);
    xpre = xcen;
    ypre = ycen;
  }

  cr->set_source_rgba(CLR_BLACK, 1);

  // label
  showtext_cr(cr, OFFSET_X - 5*graph.label.length() -55, ycenter, graph.label, 13);

  // x-axis
  stroke_xaxis(ybottom);

  // y-axis
  cr->set_line_width(0.4);
  rel_yline(cr, OFFSET_X, ytop, graph.boxheight);
  cr->stroke();
  cr->set_line_width(1.5);
  rel_xline(cr, OFFSET_X, ybottom, par.getXaxisLen());
  cr->stroke();

  // y memory
  cr->set_line_width(0.5);
  for (int32_t i=0; i<=graph.memnum; ++i) {
    std::string str(graph.getmemory(i));
    double x(OFFSET_X - 5*str.length() - 7);
    double y(ybottom - i*graph.getBoxHeight4mem());
    showtext_cr(cr, x, y+2, str, 9);
    rel_xline(cr, OFFSET_X-2, y, 2);
    cr->stroke();
  }
  par.yaxis_now += graph.boxheight + MERGIN_BETWEEN_GRAPH_DATA;

  DEBUGprint_FUNCend();
  return;
}

void setcolor(const Cairo::RefPtr<Cairo::Context> &cr, bed &x)
{
  static int32_t on(0);
  if(!on) {
    cr->set_source_rgba(CLR_GRAY4, 1);
    on=1;
  } else {
    cr->set_source_rgba(CLR_GREEN, 1);
    on=0;
  }
}

void setcolor(const Cairo::RefPtr<Cairo::Context> &cr, bed12 &x)
{
//  printf("%d, %d, %d\n", x.rgb_r, x.rgb_g, x.rgb_b);
  cr->set_source_rgba(x.rgb_r/(double)255, x.rgb_g/(double)255, x.rgb_b/(double)255, 0.6);
}

template <class T>
void PDFPage::drawBedAnnotation(const DROMPA::Global &p, const vbed<T> &vbed)
{
  DEBUGprint_FUNCStart();
  int32_t boxheight(BOXHEIGHT_BEDANNOTATION);
  std::string chr(rmchr(chrname));
  double ycenter(par.yaxis_now + boxheight/2);

  cr->set_line_width(0.5);
  rel_xline(cr, OFFSET_X, ycenter, par.getXaxisLen());
  cr->stroke();

  // bed
  cr->set_line_width(boxheight/2);
  for (auto &x: vbed.getvBed()) {
    if (x.chr != chr) continue;

    if (par.xstart <= x.end && x.start <= par.xend) {
      setcolor(cr, x);
      double x1 = BP2PIXEL(x.start - par.xstart);
      double len = (x.end - x.start) * par.dot_per_bp;
      rel_xline(cr, x1, ycenter, len);
      cr->stroke();
      if (p.drawparam.isshowbedname() && x.name != "") { // bed12
        cr->set_source_rgba(CLR_BLACK, 1);
        showtext_cr(cr, x1+len/2, ycenter-1, x.name, 8);
      }
    }
  }
  cr->stroke();

  // label
  cr->set_source_rgba(CLR_BLACK, 1);
  showtext_cr(cr, 90, ycenter+4, vbed.getlabel(), 12);


  par.yaxis_now += boxheight;

  DEBUGprint_FUNCend();
  return;
}

void PDFPage::stroke_xaxis_num(const double y, const int32_t fontsize)
{
  int32_t mega, kilo;
  int32_t interval(setInterval());

  cr->set_source_rgba(CLR_BLACK, 1);
  for(int32_t i=setline(par.xstart, interval); i<=par.xend; i+=interval) {
    std::string str;
    double x(BP2PIXEL(i - par.xstart));
    if (par.width_per_line > 100*NUM_1M)     str = float2string(i/static_cast<double>(NUM_1M), 1) + "M";
//    else if (par.width_per_line > 10*NUM_1M) str = float2string(i/static_cast<double>(NUM_1K), 1) + "k";
    else {
      mega = i/NUM_1M;
      kilo = (i%NUM_1M)/NUM_1K;
      if (par.width_per_line > 10*NUM_1K) str = float2string(i/static_cast<double>(NUM_1M), 3) + "M";
      else if (par.width_per_line > 10) {
        if (mega) str = std::to_string(mega) + "," + float2string((i%NUM_1M)/static_cast<double>(NUM_1K), 3) + "K";
        else str = float2string((i%NUM_1M)/static_cast<double>(NUM_1K), 3) + "K";
      } else {
        if (mega) str = std::to_string(mega) + "," + std::to_string(kilo) + "," + std::to_string(i%NUM_1K);
        else if (kilo) str = std::to_string(kilo) + "," + std::to_string(i%NUM_1K);
        else str = std::to_string(i%NUM_1K);
      }
    }
    showtext_cr(cr, x - 3*str.length(), y+10, str, fontsize);
  }
  return;
}

void PDFPage::StrokeReadLines(const DROMPA::Global &p)
{
  auto &d = p.drawparam;

  for (auto &pair: vsamplepairoverlayed) {
    if (d.showpinter) StrokeDataFrame<PinterDataFrame>(p, pair);
    if (d.showpenrich && pair.first.InputExists()) StrokeDataFrame<PenrichDataFrame>(p, pair);
    if (d.showratio && pair.first.InputExists()) {
      if (d.showratio == 1)      StrokeDataFrame<RatioDataFrame>(p, pair);
      else if (d.showratio == 2) StrokeDataFrame<LogRatioDataFrame>(p, pair);
    }
    if (d.showctag) StrokeDataFrame<ChIPDataFrame>(p, pair);
    if (d.showitag==1 && pair.first.InputExists()) StrokeDataFrame<InputDataFrame>(p, pair);
  }
  if (d.showitag==2 && vsamplepairoverlayed[0].first.InputExists()) StrokeDataFrame<InputDataFrame>(p, vsamplepairoverlayed[0]);

  stroke_xaxis_num(par.yaxis_now, 9);
  return;
}

void PDFPage::DrawIdeogram(const DROMPA::Global &p)
{
  DEBUGprint_FUNCStart();

  int32_t boxheight(BOXHEIGHT_IDEOGRAM);
  int32_t on(0);
  int32_t acen_once(0);

  cr->set_line_width(1);
  // frame
  cr->set_source_rgba(CLR_BLACK, 1);
  cr->rectangle(OFFSET_X, par.yaxis_now, par.getXaxisLen(), boxheight);
  cr->stroke();

  for (auto &x: p.anno.vcytoband) {
    if (rmchr(chrname) != x.chr) continue;
    //    x.print();
    if (x.stain == "acen") cr->set_source_rgba(CLR_SALMON, 1);
    else if (x.stain == "gneg") cr->set_source_rgba(CLR_GRAY0, 1);
    else if (x.stain == "gpos25" || x.stain == "stalk") cr->set_source_rgba(CLR_GRAY, 1);
    else if (x.stain == "gpos50") cr->set_source_rgba(CLR_GRAY2, 1);
    else if (x.stain == "gpos75") cr->set_source_rgba(CLR_GRAY3, 1);
    else if (x.stain == "gpos100" || x.stain == "gvar") cr->set_source_rgba(CLR_GRAY4, 1);
    else { std::cout << "Warning: stain " << x.stain << " is not annotated." << std::endl; }

    double s(BP2PIXEL(x.start));
    double len(x.getlen() * par.dot_per_bp);

    if (x.stain == "acen") {
      if(!acen_once) {
        mytriangle(s,     par.yaxis_now,
                   s,     par.yaxis_now + boxheight,
                   s+len, par.yaxis_now + boxheight/2);
        ++acen_once;
      } else {
        mytriangle(s,     par.yaxis_now + boxheight/2,
                   s+len+1, par.yaxis_now,
                   s+len+1, par.yaxis_now + boxheight);
      }
    } else {
      cr->rectangle(s, par.yaxis_now, len, boxheight);
    }
    cr->fill();
    cr->stroke();

    cr->set_source_rgba(CLR_BLACK, 1);
    double y;
    if(on) y = par.yaxis_now + boxheight + 5;
    else y = par.yaxis_now - 3;
    showtext_cr(cr, s+1, y, x.name, 5);
    if(on) on=0; else { ++on; }
  }

  // label
  showtext_cr(cr, 90, par.yaxis_now + MERGIN_BETWEEN_GRAPH_DATA/2, "Ideogram", 13);

  par.yaxis_now += boxheight + MERGIN_BETWEEN_GRAPH_DATA;

  DEBUGprint_FUNCend();
  return;
}

template <class T>
void PDFPage::Draw_vbedlist(const DROMPA::Global &p, const std::vector<vbed<T>> &vlist)
{
  if(!vlist.size()) return;

  par.yaxis_now += MERGIN_BETWEEN_READ_BED;
  for (auto &x: vlist) {
    drawBedAnnotation(p, x);
    par.yaxis_now += 2;
  }
}

void PDFPage::StrokeEachLayer(const DROMPA::Global &p)
{
  if (p.anno.showIdeogram()) DrawIdeogram(p);

  if (p.anno.GC.isOn()) StrokeGraph(GC);
  if (p.anno.GD.isOn()) StrokeGraph(GD);

  // Gene
  if (p.anno.genefile != "" || p.anno.arsfile != "" || p.anno.terfile != "") DrawGeneAnnotation(p);

  // Read
  StrokeReadLines(p);

  // BED12 files
  Draw_vbedlist(p, p.anno.vbed12list);
  // BED files
  Draw_vbedlist(p, p.anno.vbedlist);

  // ChIA-Drop
  if (p.anno.existChIADrop()) {
    par.yaxis_now += MERGIN_BETWEEN_READ_BED;
    StrokeChIADrop(p);
  }

  // Interaction
  if (p.anno.vinterlist.size()) {
    par.yaxis_now += MERGIN_BETWEEN_READ_BED;
    for (auto &x: p.anno.vinterlist) {
      drawInteraction(x);
      par.yaxis_now += +5;
    }
  }

  //if(d->repeat.argv) draw_repeat(d, cr, xstart, xend);

  return;
}

void PDFPage::MakePage(const DROMPA::Global &p,
                       const int32_t page_no,
                       const std::string &pagelabel)
{
  DEBUGprint_FUNCStart();

  int32_t line_start, line_end;
  std::tie(line_start, line_end) = get_start_end_linenum(page_no, p.drawparam.getlpp());

  par.yaxis_now = OFFSET_Y;
  cr->set_source_rgba(CLR_WHITE, 1);
  cr->paint();

  // Stroke each layer
  for (int32_t i=line_start; i<line_end; ++i) {
    set_xstart_xend(i);
    if (par.xstart >= par.xend) continue;
    StrokeEachLayer(p);
    if (i != line_end-1) par.yaxis_now += MERGIN_BETWEEN_EACHLAYER;
  }

  // Page title
  std::string title;
  if (pagelabel == "None") {
    if (par.num_page>1) title = chrname  + "_" + std::to_string(page_no+1);
    else                title = chrname;
  } else {
    if (par.num_page>1) title = chrname + "_" + pagelabel + "_" + std::to_string(page_no+1);
    else                title = chrname + "_" + pagelabel;
  }
  cr->set_source_rgba(CLR_BLACK, 1);
  showtext_cr(cr, 50, 30, title, 16);
  cr->stroke();

  cr->show_page();

  DEBUGprint_FUNCend();
  return;
}

void Figure::Draw_SpecificRegion(DROMPA::Global &p,
                                 std::string &pdffilename,
                                 int32_t width,
                                 int32_t height)
{
  const auto surface = Cairo::PdfSurface::create(pdffilename, width, height);
  int32_t region_no(1);
  for (auto &x: regionBed) {
    int32_t num_page = p.drawparam.getNumPage(x.start, x.end);
    for(int32_t i=0; i<num_page; ++i) {
      std::cout << boost::format("   page %5d/%5d/%5d\r")
        % (i+1) % num_page % region_no << std::flush;
      PDFPage page(p, vReadArray, vsamplepairoverlayed, surface, x.start, x.end);
      page.MakePage(p, i, std::to_string(region_no));
    }
    ++region_no;
    printf("\n");
  }
}

void Figure::Draw_SpecificGene(DROMPA::Global &p,
                               std::string &pdffilename,
                               int32_t width,
                               int32_t height)
{
  const auto surface = Cairo::PdfSurface::create(pdffilename, width, height);
  int32_t len(p.drawregion.getLenGeneLoci());
  auto &gmp_chr = p.anno.gmp.at(vReadArray.getchr().getname());
  for (auto &m: gmp_chr) {
    if(!p.drawregion.ExistInGeneLociFile(m.second.gname)) continue;

    int32_t start = std::max(0, m.second.txStart - len);
    int32_t end   = std::min(m.second.txEnd + len, vReadArray.getchrlen() -1);
    int32_t num_page(p.drawparam.getNumPage(start, end));
    for(int32_t i=0; i<num_page; ++i) {
      std::cout << boost::format("   page %5d/%5d/%s\r") % (i+1) % num_page % m.second.gname << std::flush;
      PDFPage page(p, vReadArray, vsamplepairoverlayed, surface, start, end);
      page.MakePage(p, i, m.second.gname);
    }
    printf("\n");
  }
}

void Figure::Draw_WholeGenome(DROMPA::Global &p,
                              std::string &pdffilename,
                              int32_t width,
                              int32_t height)
{
#ifdef CAIRO_HAS_PDF_SURFACE
  const auto surface = Cairo::PdfSurface::create(pdffilename, width, height);
  int32_t num_page = p.drawparam.getNumPage(0, vReadArray.getchrlen());

  for (int32_t i=0; i<num_page; ++i) {
    std::cout << boost::format("   page %5d/%5d\r") % (i+1) % num_page << std::flush;
    PDFPage page(p, vReadArray, vsamplepairoverlayed, surface, 0, vReadArray.getchrlen());
    page.MakePage(p, i, "None");
  }
  printf("\n");
#else
  std::cout << "You must compile cairo with PDF support for DROMPA+." << std::endl;
  return;
#endif
}
