/* Copyright(c) Ryuichiro Nakato <rnakato@iqb.u-tokyo.ac.jp>
 * This file is a part of DROMPA sources.
 */
#ifndef _DD_COMMAND_H_
#define _DD_COMMAND_H_

#include <iostream>
#include "dd_gv.hpp"
#include "version.hpp"
#include "../submodules/SSP/common/BoostOptions.hpp"

void exec_PCSHARP(DROMPA::Global &p);
void exec_GV(DROMPA::Global &p);
void exec_PROFILE(DROMPA::Global &p);
void exec_MULTICI(DROMPA::Global &p);
void exec_GENWIG(DROMPA::Global &p);

class Command {
  std::string name;
  std::string desc;
  std::string requiredstr;
  std::vector<DrompaCommand> vopts;
  MyOpt::Variables values;
  std::function<void(DROMPA::Global &p)> func;

  DROMPA::Global p;

public:

  Command(const std::string &n,
	  const std::string &d,
	  const std::string &r,
	  const std::function<void(DROMPA::Global &p)> &_func,
	  const std::vector<DrompaCommand> &v,
	  const CommandParamSet &cps):
    name(n), desc(d), requiredstr(r), vopts(v),
    func(_func)
  {
    p.setOpts(v, cps);
  };

  void printCommandName() const {
    std::cout << std::setw(8) << " " << std::left << std::setw(12) << name
	      << std::left << std::setw(40) << desc << std::endl;
  }
  void printhelp() const {
    std::cout << boost::format("%1%:  %2%\n") % name % desc;
    std::cout << boost::format("Usage:\n\tdrompa+ %1% [options] -o <output> --gt <genometable> %2%\n\n") % name % requiredstr;
    std::cout << p.opts << std::endl;
  }
  void InitDump();
  void SetValue(int argc, char* argv[]) {
    if (argc ==1) {
      printhelp();
      exit(0);
    }
    try {
      store(parse_command_line(argc, argv, p.opts), values);

      if (values.count("help")) {
	printhelp();
	exit(0);
      }

      notify(values);
      p.setValues(vopts, values);
      InitDump();

    } catch (std::exception &e) {
      std::cerr << e.what() << std::endl;
      exit(0);
    }
  }

  const std::string & getname() const { return name; }
  void execute(){ func(p); }
};

std::vector<Command> generateCommands();

#endif /* _DD_COMMAND_H_ */
