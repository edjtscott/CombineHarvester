#include "CombineHarvester/CombinePdfs/interface/MorphFunctions.h"
#include "CombineHarvester/CombineTools/interface/Algorithm.h"
#include "CombineHarvester/CombineTools/interface/AutoRebin.h"
#include "CombineHarvester/CombineTools/interface/BinByBin.h"
#include "CombineHarvester/CombineTools/interface/CardWriter.h"
#include "CombineHarvester/CombineTools/interface/CombineHarvester.h"
#include "CombineHarvester/CombineTools/interface/Observation.h"
#include "CombineHarvester/CombineTools/interface/Process.h"
#include "CombineHarvester/CombineTools/interface/Systematics.h"
#include "CombineHarvester/CombineTools/interface/Utilities.h"
#include "CombineHarvester/HTTSM2017/interface/HttSystematics_SMRun2.h"
#include "RooRealVar.h"
#include "RooWorkspace.h"
#include "TF1.h"
#include "TH2.h"
#include "boost/algorithm/string/predicate.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/program_options.hpp"
#include "boost/regex.hpp"
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;
using boost::starts_with;
namespace po = boost::program_options;

int main(int argc, char **argv) {
  typedef vector<string> VString;
  typedef vector<pair<int, string>> Categories;
  using ch::syst::bin_id;

  // Define program options
  string output_folder = "sm_run2";
  string input_folder_em = "Vienna/";
  string input_folder_et = "Vienna/";
  string input_folder_mt = "Vienna/";
  string input_folder_tt = "Vienna/";
  string input_folder_mm = "Vienna/";
  string chan = "all";
  string postfix = "-ML";
  bool auto_rebin = false;
  bool manual_rebin = false;
  bool real_data = false;
  bool jetfakes = true;
  bool embedding = false;
  bool verbose = false;
  int stxs_signals = 0;
  int stxs_categories = 0;
  po::variables_map vm;
  po::options_description config("configuration");
  config.add_options()
      ("input_folder_em", po::value<string>(&input_folder_em)->default_value(input_folder_em))
      ("input_folder_et", po::value<string>(&input_folder_et)->default_value(input_folder_et))
      ("input_folder_mt", po::value<string>(&input_folder_mt)->default_value(input_folder_mt))
      ("input_folder_tt", po::value<string>(&input_folder_tt)->default_value(input_folder_tt))
      ("postfix", po::value<string>(&postfix)->default_value(postfix))
      ("channel", po::value<string>(&chan)->default_value(chan))
      ("auto_rebin", po::value<bool>(&auto_rebin)->default_value(auto_rebin))
      ("real_data", po::value<bool>(&real_data)->default_value(real_data))
      ("manual_rebin", po::value<bool>(&manual_rebin)->default_value(manual_rebin))
      ("verbose", po::value<bool>(&verbose)->default_value(verbose))
      ("output_folder", po::value<string>(&output_folder)->default_value(output_folder))
      ("stxs_signals", po::value<int>(&stxs_signals)->default_value(stxs_signals))
      ("stxs_categories", po::value<int>(&stxs_categories)->default_value(stxs_categories))
      ("jetfakes", po::value<bool>(&jetfakes)->default_value(jetfakes))
      ("embedding", po::value<bool>(&embedding)->default_value(embedding));
  po::store(po::command_line_parser(argc, argv).options(config).run(), vm);
  po::notify(vm);

  // Define the location of the "auxiliaries" directory where we can
  // source the input files containing the datacard shapes
  std::map<string, string> input_dir;
  input_dir["mt"] = string(getenv("CMSSW_BASE")) +
                    "/src/CombineHarvester/HTTSM2017/shapes/" +
                    input_folder_mt + "/";
  input_dir["et"] = string(getenv("CMSSW_BASE")) +
                    "/src/CombineHarvester/HTTSM2017/shapes/" +
                    input_folder_et + "/";
  input_dir["tt"] = string(getenv("CMSSW_BASE")) +
                    "/src/CombineHarvester/HTTSM2017/shapes/" +
                    input_folder_tt + "/";

  // Define channels
  VString chns;
  if (chan.find("mt") != std::string::npos)
    chns.push_back("mt");
  if (chan.find("et") != std::string::npos)
    chns.push_back("et");
  if (chan.find("tt") != std::string::npos)
    chns.push_back("tt");
  if (chan == "all")
    chns = {"mt", "et", "tt"};

  // Define background processes
  map<string, VString> bkg_procs;
  if (jetfakes) {
    bkg_procs["et"] = {"ZTT", "ZL", "TTT", "VVT", "EWKZ", "jetFakes"};
    bkg_procs["mt"] = {"ZTT", "ZL", "TTT", "VVT", "EWKZ", "jetFakes"};
    bkg_procs["tt"] = {"ZTT", "ZL", "TTT", "VVT", "EWKZ", "jetFakes"};
  } else {
    bkg_procs["et"] = {"W", "ZTT", "QCD", "ZL", "ZJ", "TTT", "TTJ", "VVJ", "VVT", "EWKZ"};
    bkg_procs["mt"] = {"W", "ZTT", "QCD", "ZL", "ZJ", "TTT", "TTJ", "VVJ", "VVT", "EWKZ"};
    bkg_procs["tt"] = {"W", "ZTT", "QCD", "ZL", "ZJ", "TTT", "TTJ", "VVJ", "VVT", "EWKZ"};
  }

  // Define categories
  map<string, Categories> cats;
  // STXS stage 0 categories (optimized on ggH and VBF)
  if(stxs_categories == 0){
    cats["et"] = {
        { 1, "et_ggh"},
        { 2, "et_qqh"},
        {11, "et_w"},
        {12, "et_ztt"},
        {13, "et_tt"},
        {14, "et_ss"},
        {15, "et_zll"},
        {16, "et_misc"},
    };
     cats["mt"] = {
        { 1, "mt_ggh"},
        { 2, "mt_qqh"},
        {11, "mt_w"},
        {12, "mt_ztt"},
        {13, "mt_tt"},
        {14, "mt_ss"},
        {15, "mt_zll"},
        {16, "mt_misc"},
    };
     cats["tt"] = {
        { 1, "tt_ggh"},
        { 2, "tt_qqh"},
        {12, "tt_ztt"},
        {16, "tt_misc"},
        {17, "tt_noniso"},
    };
  }
  // STXS stage 1 categories (optimized on STXS stage 1 splits of ggH and VBF)
  else if(stxs_categories == 1){
    cats["et"] = {
        { 3, "et_ggh_0jet"},
        { 4, "et_ggh_1jet"},
        { 5, "et_ggh_ge2jets"},
        { 6, "et_qqh_l2jets"},
        { 7, "et_qqh_2jets"},
        { 8, "et_qqh_g2jets"},
        {11, "et_w"},
        {12, "et_ztt"},
        {13, "et_tt"},
        {14, "et_ss"},
        {15, "et_zll"},
        {16, "et_misc"},
    };
     cats["mt"] = {
        { 3, "mt_ggh_0jet"},
        { 4, "mt_ggh_1jet"},
        { 5, "mt_ggh_ge2jets"},
        { 6, "mt_qqh_l2jets"},
        { 7, "mt_qqh_2jets"},
        { 8, "mt_qqh_g2jets"},
        {11, "mt_w"},
        {12, "mt_ztt"},
        {13, "mt_tt"},
        {14, "mt_ss"},
        {15, "mt_zll"},
        {16, "mt_misc"},
    };
     cats["tt"] = {
        { 3, "tt_ggh_0jet"},
        { 4, "tt_ggh_1jet"},
        { 5, "tt_ggh_ge2jets"},
        { 6, "tt_qqh_l2jets"},
        { 7, "tt_qqh_2jets"},
        { 8, "tt_qqh_g2jets"},
        {12, "tt_ztt"},
        {16, "tt_misc"},
        {17, "tt_noniso"},
    };
  }
  else throw std::runtime_error("Given STXS categories are not known.");

  // Specify signal processes and masses
  vector<string> sig_procs;
  // STXS stage 0: ggH and VBF processes
  if(stxs_signals == 0) sig_procs = {"ggH", "qqH"};
  // STXS stage 1: Splits of ggH and VBF processes
  // References:
  // - https://twiki.cern.ch/twiki/bin/view/LHCPhysics/LHCHXSWGFiducialAndSTXS
  // - https://twiki.cern.ch/twiki/bin/view/LHCPhysics/LHCHXSWG2
  else if(stxs_signals == 1) sig_procs = {
      "ggH_0J", "ggH_1J", "ggH_GE2J", "ggH_VBFTOPO",
      "qqH_VBFTOPO_JET3", "qqH_VBFTOPO_JET3VETO", "qqH_REST", "qqH_PTJET1_GT200"};
  else throw std::runtime_error("Given STXS signals are not known.");
  vector<string> masses = {"125"};

  // Create combine harverster object
  ch::CombineHarvester cb;

  // Add observations and processes
  for (auto chn : chns) {
    cb.AddObservations({"*"}, {"htt"}, {"13TeV"}, {chn}, cats[chn]);
    cb.AddProcesses({"*"}, {"htt"}, {"13TeV"}, {chn}, bkg_procs[chn], cats[chn],
                    false);
    cb.AddProcesses(masses, {"htt"}, {"13TeV"}, {chn}, sig_procs, cats[chn],
                    true);
  }

  // Add systematics
  ch::AddSMRun2Systematics(cb, jetfakes, embedding);

  // Extract shapes from input ROOT files
  for (string chn : chns) {
    cb.cp().channel({chn}).backgrounds().ExtractShapes(
        input_dir[chn] + "htt_" + chn + ".inputs-sm-13TeV" + postfix + ".root",
        "$BIN/$PROCESS", "$BIN/$PROCESS_$SYSTEMATIC");
    cb.cp().channel({chn}).process(sig_procs).ExtractShapes(
        input_dir[chn] + "htt_" + chn + ".inputs-sm-13TeV" + postfix + ".root",
        "$BIN/$PROCESS$MASS", "$BIN/$PROCESS$MASS_$SYSTEMATIC");
  }

  // Delete processes with 0 yield
  cb.FilterProcs([&](ch::Process *p) {
    bool null_yield = !(p->rate() > 0.0);
    if (null_yield) {
      std::cout << "[INFO] Removing process with null yield: \n ";
      std::cout << ch::Process::PrintHeader << *p << "\n";
      cb.FilterSysts([&](ch::Systematic *s) {
        bool remove_syst = (MatchingProcess(*p, *s));
        return remove_syst;
      });
    }
    return null_yield;
  });

  // Replacing observation with the sum of the backgrounds (Asimov data)
  // useful to be able to check this, so don't do the replacement
  // for these
  if (!real_data) {
    for (auto b : cb.cp().bin_set()) {
      std::cout << "[INFO] Replacing data with asimov in bin " << b << "\n";
      cb.cp().bin({b}).ForEachObs([&](ch::Observation *obs) {
        obs->set_shape(cb.cp().bin({b}).backgrounds().GetShape() +
                           cb.cp().bin({b}).signals().GetShape(),
                       true);
      });
    }
  }

  // Rebin manually
  if (manual_rebin) {
    vector<double> binning = {0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
    for (auto b : cb.cp().bin_set()) {
      std::cout << "[INFO] Rebinning by hand for bin: " << b << std::endl;
      cb.cp().bin({b}).VariableRebin(binning);
    }
  }

  // At this point we can fix the negative bins
  std::cout << "[INFO] Fixing negative bins.\n";
  cb.ForEachProc([](ch::Process *p) {
    if (ch::HasNegativeBins(p->shape())) {
      auto newhist = p->ClonedShape();
      ch::ZeroNegativeBins(newhist.get());
      p->set_shape(std::move(newhist), false);
    }
  });

  cb.ForEachSyst([](ch::Systematic *s) {
    if (s->type().find("shape") == std::string::npos)
      return;
    if (ch::HasNegativeBins(s->shape_u()) ||
        ch::HasNegativeBins(s->shape_d())) {
      auto newhist_u = s->ClonedShapeU();
      auto newhist_d = s->ClonedShapeD();
      ch::ZeroNegativeBins(newhist_u.get());
      ch::ZeroNegativeBins(newhist_d.get());
      s->set_shapes(std::move(newhist_u), std::move(newhist_d), nullptr);
    }
  });

  // Merge bins and set bin-by-bin uncertainties
  auto bbb = ch::BinByBinFactory()
                 .SetAddThreshold(0.05)
                 .SetMergeThreshold(0.8)
                 .SetFixNorm(false);
  bbb.MergeBinErrors(cb.cp().backgrounds());
  bbb.AddBinByBin(cb.cp().backgrounds(), cb);

  // This function modifies every entry to have a standardised bin name of
  // the form: {analysis}_{channel}_{bin_id}_{era}
  ch::SetStandardBinNames(cb);

  // Write out datacards. Naming convention important for rest of workflow. We
  // make one directory per chn-cat, one per chn and cmb. In this code we only
  // store the individual datacards for each directory to be combined later.
  string output_prefix = "output/";
  ch::CardWriter writer(output_prefix + output_folder + "/$TAG/$MASS/$BIN.txt",
                        output_prefix + output_folder +
                            "/$TAG/common/htt_input.root");

  // We're not using mass as an identifier - which we need to tell the
  // CardWriter
  // otherwise it will see "*" as the mass value for every object and skip it
  //    writer.SetWildcardMasses({});
  if (verbose)
    writer.SetVerbosity(1);

  // Write datacards combined and per channel
  writer.WriteCards("cmb", cb);

  for (auto chn : chns) {
    writer.WriteCards(chn, cb.cp().channel({chn}));
  }

  if (verbose)
    cb.PrintAll();

  cout << "[INFO] Done producing datacards.\n";
}