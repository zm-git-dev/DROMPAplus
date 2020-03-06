/* Copyright(c)  Ryuichiro Nakato <rnakato@iam.u-tokyo.ac.jp>
 * All rights reserved.
 */
#ifndef _DD_DRAW_DATAFRAME_H_
#define _DD_DRAW_DATAFRAME_H_

#include "dd_draw_pdfpage.hpp"
#include "color.hpp"

class DataFrame {
  virtual void StrokeYmem(const int32_t nlayer);

  void StrokeFrame() {
    cr->set_line_width(0.4);
    cr->set_source_rgba(CLR_BLACK, 1);
    rel_xline(cr, OFFSET_X, par.yaxis_now, par.getXaxisLen());
    rel_yline(cr, OFFSET_X, par.yaxis_now - height_df, height_df);
    cr->stroke();
  }

  void StrokeBins(const SamplePairEach &pair, const vChrArray &vReadArray, const int32_t nlayer);

  virtual void StrokeEachBin(const SamplePairEach &pair, const vChrArray &vReadArray,
			     const int32_t i, const double xcen, const int32_t yaxis,
			     const int32_t nlayer);

protected:
  enum {POSI_XLABEL=50};
  const Cairo::RefPtr<Cairo::Context> cr;
  const DParam &par;
  double scale, scale2nd;
  std::string label, label2nd;
  double width_df;
  double height_df;
  double len_binedge;

  bool sigtest;
  double threshold;
  std::string chrname;

  double getEthre(const DROMPA::Global &p) {
    double thre(0);
    if (p.isGV) thre = p.drawparam.scale_ratio;
    else thre = p.thre.ethre;
    return thre;
  }

 public:
  DataFrame(const Cairo::RefPtr<Cairo::Context> cr_,
	    const std::string &l, const std::string &l2,
	    const double sglobal, const double slocal1, const double slocal2,
	    const DParam &refparam, const bool sig, const double thre,
	    const std::string &_chrname, const int32_t width_draw):
    cr(cr_), par(refparam), label(l), label2nd(l2),
    width_df(width_draw), height_df(refparam.get_height_df()), len_binedge(2),
    sigtest(sig), threshold(thre), chrname(_chrname)
  {
    if (slocal1) scale    = slocal1; else scale    = sglobal;
    if (slocal2) scale2nd = slocal2; else scale2nd = sglobal;
  }

  void Draw(const DROMPA::Global &p, const SamplePairOverlayed &pair, const vChrArray &vReadArray);

  void HighlightPeaks(const SamplePairOverlayed &pair){ (void)(pair); return; }
  int32_t getbinlen(const double value) const { return -std::min(par.ystep*value, height_df); }

  virtual void StrokeYlab(const SamplePairOverlayed &pair)=0;
  virtual void setColor(const double value, const int32_t nlayer, const double alpha)=0;
  virtual double getVal(const SamplePairEach &pair, const vChrArray &vReadArray, const int32_t i)=0;
};


class ChIPDataFrame : public DataFrame {
  void setColor(const double value, const int32_t nlayer, const double alpha) {
    (void)(value);
    if (!nlayer) cr->set_source_rgba(CLR_GREEN3, alpha);
    else cr->set_source_rgba(CLR_ORANGE, alpha);
  }
  double getVal(const SamplePairEach &pair, const vChrArray &vReadArray, const int32_t i) {
    return vReadArray.getArray(pair.argvChIP).array[i];
  }
  void StrokeYlab(const SamplePairOverlayed &pair);

 public:
  ChIPDataFrame(const Cairo::RefPtr<Cairo::Context> cr_,
		const DROMPA::Global &p,
		const SamplePairOverlayed &pair,
		const DParam &refparam,
		const std::string &chrname):
    DataFrame(cr_, pair.first.label, pair.second.label,
	      p.drawparam.scale_tag, pair.first.scale.tag, pair.second.scale.tag,
	      refparam,
	      p.thre.sigtest, p.thre.ipm, chrname,
	      p.drawparam.width_draw_pixel)
  {}

  void HighlightPeaks(const SamplePairOverlayed &pair) {
    cr->set_source_rgba(CLR_RED, 0.4);

    for (auto &bed: pair.first.getpeaksChr(chrname)) {
      if (!my_overlap(bed.start, bed.end, par.xstart, par.xend)) continue;
      cr->set_line_width(bed.length() * par.dot_per_bp);
      double x(BP2PIXEL(bed.summit - par.xstart));
      rel_yline(cr, x, par.yaxis_now - height_df -5, height_df +10);
    }
    cr->stroke();
  }
};

class InputDataFrame : public DataFrame {
  void setColor(const double value, const int32_t nlayer, const double alpha) {
    (void)(value);
    if (!nlayer) cr->set_source_rgba(CLR_BLUE, alpha);
    else cr->set_source_rgba(CLR_OLIVE, alpha);
  }
  double getVal(const SamplePairEach &pair, const vChrArray &vReadArray, const int32_t i) {
    return vReadArray.getArray(pair.argvInput).array[i];
  }
  void StrokeYlab(const SamplePairOverlayed &pair);

 public:
  InputDataFrame(const Cairo::RefPtr<Cairo::Context> cr_, const DROMPA::Global &p,
		 const SamplePairOverlayed &pair, const DParam &refparam,
		 const std::string &chrname):
    DataFrame(cr_, "Input", "",
	      p.drawparam.scale_tag, pair.first.scale.tag, pair.second.scale.tag,
	      refparam, false, 0, chrname,
	      p.drawparam.width_draw_pixel)
  {
    (void)(pair);
  }
};


class RatioDataFrame : public DataFrame {
  bool isGV;

  void setColor(const double value, const int32_t nlayer, const double alpha);

  double getVal(const SamplePairEach &pair, const vChrArray &vReadArray, const int32_t i) {
    return CalcRatio(vReadArray.getArray(pair.argvChIP).array[i],
		     vReadArray.getArray(pair.argvInput).array[i],
		     pair.ratio);
  }

  const std::string getlabel(const DROMPA::Global &p, const SamplePairOverlayed &pair) const {
    if(p.drawparam.showctag) return "IP/Input";
    else return pair.first.label;
  }
  const std::string get2ndlabel(const DROMPA::Global &p, const SamplePairOverlayed &pair) const {
    if(p.drawparam.showctag) return "";
    else return pair.second.label;
  }
  void StrokeYlab(const SamplePairOverlayed &pair);

public:
  RatioDataFrame(const Cairo::RefPtr<Cairo::Context> cr_, const DROMPA::Global &p,
		 const SamplePairOverlayed &pair, const DParam &refparam,
		 const std::string &chrname):
    DataFrame(cr_, getlabel(p, pair), get2ndlabel(p, pair),
	      p.drawparam.scale_ratio, pair.first.scale.ratio, pair.second.scale.ratio,
	      refparam, p.thre.sigtest, getEthre(p), chrname,
	      p.drawparam.width_draw_pixel),
    isGV(p.isGV)
  {}
};

class LogRatioDataFrame : public DataFrame {
  bool isGV;
  int32_t barnum_minus;
  int32_t barnum_plus;
  double len_minus, len_plus;

  void setColor(const double value, const int32_t nlayer, const double alpha);

  double getVal(const SamplePairEach &pair, const vChrArray &vReadArray, const int32_t i)  // not used
  {
    return vReadArray.getArray(pair.argvChIP).array[i];
  }

  const std::string getlabel(const DROMPA::Global &p, const SamplePairOverlayed &pair) const {
    if(p.drawparam.showctag) return "IP/Input";
    else return pair.first.label;
  }

  const std::string get2ndlabel(const DROMPA::Global &p, const SamplePairOverlayed &pair) const {
    if(p.drawparam.showctag) return "";
    else return pair.second.label;
  }

  void StrokeYmem(const int32_t nlayer) {
    cr->set_source_rgba(CLR_BLACK, 0.5);
    for (int32_t i=0; i<par.barnum; ++i) rel_xline(cr, OFFSET_X, par.yaxis_now - i*par.ystep, par.getXaxisLen());
    cr->stroke();

    cr->set_source_rgba(CLR_BLACK, 1);
    double x(0);
    if (!nlayer) x = OFFSET_X + par.getXaxisLen() + 7;
    else x = OFFSET_X - 20;

    for(int32_t i=0; i<=par.barnum; ++i) {
      double y_value;
      if (!nlayer) y_value = std::pow(scale, i - barnum_minus);
      else         y_value = std::pow(scale2nd, i - barnum_minus);
      showtext_cr(cr, x, par.yaxis_now - i*(par.ystep - 1.5), float2string(y_value, 2), 9);
    }
  }

  void StrokeYlab(const SamplePairOverlayed &pair);

  void StrokeEachBin(const SamplePairEach &pair, const vChrArray &vReadArray,
		     const int32_t i, const double xcen, const int32_t yaxis, const int32_t nlayer);

 public:
  LogRatioDataFrame(const Cairo::RefPtr<Cairo::Context> cr_, const DROMPA::Global &p,
		    const SamplePairOverlayed &pair, const DParam &refparam,
		    const std::string &chrname):
    DataFrame(cr_, getlabel(p, pair), get2ndlabel(p, pair),
	      p.drawparam.scale_ratio, pair.first.scale.ratio, pair.second.scale.ratio,
	      refparam, p.thre.sigtest, getEthre(p), chrname,
	      p.drawparam.width_draw_pixel),
    isGV(p.isGV),
    barnum_minus(refparam.barnum/2),
    barnum_plus(refparam.barnum - barnum_minus),
    len_minus(barnum_minus*par.ystep),
    len_plus(barnum_plus*par.ystep)
  {}
};

class PinterDataFrame : public DataFrame {
  void setColor(const double value, const int32_t nlayer, const double alpha)
  {
    if (!nlayer) { // first layer
      if (value > threshold) cr->set_source_rgba(CLR_RED, alpha);
      else cr->set_source_rgba(CLR_GRAY, alpha);
    } else {    // second layer
      if (value > threshold) cr->set_source_rgba(CLR_YELLOW2, alpha);
      else cr->set_source_rgba(CLR_GRAY2, alpha);
    }
  }
  double getVal(const SamplePairEach &pair, const vChrArray &vReadArray, const int32_t i) {
    const ChrArray &a = vReadArray.getArray(pair.argvChIP);
    return a.stats.getlogp(a.array[i]);
  }
  const std::string getlabel(const DROMPA::Global &p, const SamplePairOverlayed &pair) const {
    if (p.drawparam.showctag || p.drawparam.showratio) return "log10(p) (ChIP)";
    else return pair.first.label;
  }
  const std::string get2ndlabel(const DROMPA::Global &p, const SamplePairOverlayed &pair) const {
    if (p.drawparam.showctag || p.drawparam.showratio) return "";
    else return pair.second.label;
  }
  void StrokeYlab(const SamplePairOverlayed &pair);

public:
  PinterDataFrame(const Cairo::RefPtr<Cairo::Context> cr_, const DROMPA::Global &p,
		  const SamplePairOverlayed &pair, const DParam &refparam,
		  const std::string &chrname):
    DataFrame(cr_, getlabel(p, pair), get2ndlabel(p, pair),
	      p.drawparam.scale_pvalue, pair.first.scale.pvalue, pair.second.scale.pvalue,
	      refparam, p.thre.sigtest, -log10(p.thre.pthre_inter), chrname,
	      p.drawparam.width_draw_pixel)
  {}

};

class PenrichDataFrame : public DataFrame {
  void setColor(const double value, const int32_t nlayer, const double alpha)
  {
    if (!nlayer) {
      if (value > threshold) cr->set_source_rgba(CLR_RED, alpha);
      else cr->set_source_rgba(CLR_GRAY, alpha);
    } else {
      if (value > threshold) cr->set_source_rgba(CLR_YELLOW2, alpha);
      else cr->set_source_rgba(CLR_GRAY2, alpha);
    }
  }
  double getVal(const SamplePairEach &pair, const vChrArray &vReadArray, const int32_t i) {
    return binomial_test(vReadArray.getArray(pair.argvChIP).array[i],
			 vReadArray.getArray(pair.argvInput).array[i],
			 pair.ratio);
  }
  const std::string getlabel(const DROMPA::Global &p, const SamplePairOverlayed &pair) const {
    if(p.drawparam.showctag || p.drawparam.showratio) return "log10(p) (ChIP/Input)";
    else return pair.first.label;
  }
  const std::string get2ndlabel(const DROMPA::Global &p, const SamplePairOverlayed &pair) const {
    if(p.drawparam.showctag || p.drawparam.showratio) return "";
    else return pair.second.label;
  }
  void StrokeYlab(const SamplePairOverlayed &pair);

 public:
  PenrichDataFrame(const Cairo::RefPtr<Cairo::Context> cr_,
		   const DROMPA::Global &p, const SamplePairOverlayed &pair,
		   const DParam &refparam, const std::string &chrname):
    DataFrame(cr_, getlabel(p, pair), get2ndlabel(p, pair),
	      p.drawparam.scale_pvalue, pair.first.scale.pvalue, pair.second.scale.pvalue,
	      refparam, p.thre.sigtest, -log10(p.thre.pthre_enrich), chrname,
	      p.drawparam.width_draw_pixel)
  {}

};

#endif /* _DD_DRAW_DATAFRAME_H_ */
