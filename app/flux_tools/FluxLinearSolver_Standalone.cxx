#include "FluxLinearSolver_Standalone.hxx"

#include "TGraph.h"

std::string OutputFile = "FluxLinearSolver.root";

std::string NDFile, NDHist, FDFile, FDHist, BCTree, BCBranches;

std::string OARange;

int NEnuBinMerge = 0;

int method = 1;

double OutOfRangeChi2Factor = 0.1;
double BCRegFactor = 1E-2;
double FitRangeLow = 0.5, FitRangeHigh = 10.0;

void SayUsage(char const *argv[]) {
  std::cout << "Runlike: " << argv[0]
            << " -N <NDFluxFile,NDFluxHistName> -F <FDFluxFile,FDFluxHistName> "
               "[-o Output.root -M <1:SVD, 2:QR, 3:Normal, 4:Inverse> -MX "
               "<NEnuBinMerge> -OR OutOfRangeChi2Factor -RF BeamConfigsRegFactor "
	       "-FR <FitRangeLow, FitRangeHigh> "
	       "-OA <OffAxisLow_OffAxisHigh:BinWidth,....,OffAxisLow_OffAxisHigh:BinWidth> ]"
            << std::endl;
}

double deriv(double *evals, double step) {
  return (-evals[2] + 8 * evals[1] - 8 * evals[-1] + evals[-2]) / (12 * step);
}

double second_deriv(double *evals, double step) {
  double first_deriv_evals[5];

  first_deriv_evals[0] = deriv(&evals[-2], step);
  first_deriv_evals[1] = deriv(&evals[-1], step);
  first_deriv_evals[3] = deriv(&evals[1], step);
  first_deriv_evals[4] = deriv(&evals[2], step);

  return deriv(&first_deriv_evals[2], step);
}

void handleOpts(int argc, char const *argv[]) {
  int opt = 1;
  while (opt < argc) {
    if (std::string(argv[opt]) == "-o") {
      OutputFile = argv[++opt];
    } else if (std::string(argv[opt]) == "-N") {
      std::vector<std::string> params =
          ParseToVect<std::string>(argv[++opt], ",");
      if (params.size() != 2) {
        std::cout << "[ERROR]: Recieved " << params.size()
                  << " entrys for -i, expected 2." << std::endl;
        exit(1);
      }
      NDFile = params[0];
      NDHist = params[1];
    } else if (std::string(argv[opt]) == "-F") {
      std::vector<std::string> params =
          ParseToVect<std::string>(argv[++opt], ",");
      if (params.size() != 2) {
        std::cout << "[ERROR]: Recieved " << params.size()
                  << " entrys for -i, expected 2." << std::endl;
        exit(1);
      }
      FDFile = params[0];
      FDHist = params[1];
    } else if (std::string(argv[opt]) == "-B") {
      std::vector<std::string> params =
          ParseToVect<std::string>(argv[++opt], ",");
      if (params.size() != 2) {
        std::cout << "[ERROR]: Recieved " << params.size()
                  << " entrys for -i, expected 2." << std::endl;
        exit(1);
      }
      BCTree = params[0];
      BCBranches = params[1];
//      BCTree = "ConfigTree";
    } else if (std::string(argv[opt]) == "-M") {
      method = str2T<int>(argv[++opt]);
      if ((method < 1) || (method > 4)) {
        std::cout
            << "[WARN]: Invalid option for solving method, defaulting to QR."
            << std::endl;
        method = 2;
      }
    } else if (std::string(argv[opt]) == "-FR") {
      std::vector<std::string> params =
          ParseToVect<std::string>(argv[++opt], ",");
      if (params.size() != 2) {
        std::cout << "[ERROR]: Recieved " << params.size()
                  << " entrys for -i, expected 2." << std::endl;
        exit(1);
      }
      FitRangeLow = str2T<double>(params[0]);
      FitRangeHigh = str2T<double>(params[1]);
    } else if (std::string(argv[opt]) == "-MX") {
      NEnuBinMerge = str2T<int>(argv[++opt]);
    } else if (std::string(argv[opt]) == "-OA") {
      OARange = std::string(argv[++opt]);
    } else if (std::string(argv[opt]) == "-OR") {
      OutOfRangeChi2Factor = str2T<double>(argv[++opt]);
    } else if (std::string(argv[opt]) == "-RF") {
      BCRegFactor = str2T<double>(argv[++opt]);
    } else if ((std::string(argv[opt]) == "-?") ||
               (std::string(argv[opt]) == "--help")) {
      SayUsage(argv);
      exit(0);
    } else {
      std::cout << "[ERROR]: Unknown option: \"" << argv[opt] << "\""
                << std::endl;
      SayUsage(argv);
      exit(1);
    }
    opt++;
  }
}

int main(int argc, char const *argv[]) {
  TH1::SetDefaultSumw2();
  handleOpts(argc, argv);

  FluxLinearSolver fls;
  FluxLinearSolver::Params p = FluxLinearSolver::GetDefaultParams();

  p.algo_id = FluxLinearSolver::Params::Solver(method);

  // Eigen LS solver
  // p.algo_id = FluxLinearSolver::Params::kSVD; // Slowest but most accurate
  // p.algo_id =
  //     FluxLinearSolver::Params::kQR; // Middle, includes pre-conditioning
  // p.algo_id = FluxLinearSolver::Params::kNormal; // Fastest but least
  // accurate
  // p.algo_id =
  //     FluxLinearSolver::Params::kInverse; // Middle, includes
  //     pre-conditioning

  // Out of fit range behavior
  p.OORMode = FluxLinearSolver::Params::kGaussianDecay; // To left/right of last
  //     'fit'
  // bin decay gaussianly
  // p.OORMode =
  //     FluxLinearSolver::Params::kIgnore; // Throw away bins outside of the
  //     range
  // p.OORMode = FluxLinearSolver::Params::kZero; // Set the target outside of
  // the range to zero

  // Out of fit range sides to care about
  // p.OORSide = FluxLinearSolver::Params::kLeft; // Try and fit low energy side
  p.OORSide = FluxLinearSolver::Params::kBoth; // Try and fit both low and high energy side
  // p.OORSide = Params::kRight; // Try and fit high energy side

  // Rate of gaussian decay for p.OORMode == Params::kGaussianDecay
  p.ExpDecayRate = 3;

  // Chi2 factor out of fit range
  p.OORFactor = OutOfRangeChi2Factor;
  p.FitBetweenFoundPeaks = false;
  // p.FitBetween = {0.5,10.0};
   p.FitBetween = {FitRangeLow,FitRangeHigh};
  p.MergeENuBins = NEnuBinMerge;
  p.MergeOAPBins = 0;
  // Use 0.5 m flux windows between -0.25 m and 32.5 m (65)
  //p.OffAxisRangesDescriptor = "-1.45_37.55:0.1";
  p.OffAxisRangesDescriptor = OARange;
  //p.OffAxisRangesDescriptor = "-1.45_32.45:0.1,37.45_37.55:0.1";

  fls.Initialize(p, {NDFile, NDHist}, {FDFile, FDHist}, {BCTree, BCBranches}, true);

  // std::array<double, 6> OscParameters{0.297,   0.0214,   0.534,
  //                                     7.37E-5, 2.539E-3, 0};
  // // numu disp
  // std::pair<int, int> OscChannel{14, 14};
  //
  // // Defaults to DUNE baseline;
  // fls.OscillateFDFlux(OscParameters, OscChannel);

  size_t nsteps = 1000;
  double start = -10;
  double end = -6;
  TGraph lcurve(nsteps);
  TGraph kcurve(nsteps - 8);
  double step = double(end - start) / double(nsteps);

  std::vector<double> eta_hat, rho_hat;
  for (size_t l_it = 0; l_it < nsteps; ++l_it) {
    double reg_exp = start + double(l_it) * step;
    // Passed parameter is regularization factor, should scan for the best one,
    double soln_norm, res_norm;
    fls.Solve(pow(10, reg_exp), BCRegFactor, res_norm, soln_norm);

    eta_hat.push_back(log(soln_norm));
    rho_hat.push_back(log(res_norm));

    lcurve.SetPoint(l_it, rho_hat.back() / 2.0, eta_hat.back() / 2.0);
  }

  double max_curv = -std::numeric_limits<double>::max();
  double best_reg;
  for (size_t l_it = 4; l_it < (nsteps - 4); ++l_it) {

    double curv =
        2.0 *
        (deriv(&rho_hat[l_it], step) * second_deriv(&eta_hat[l_it], step) -
         deriv(&eta_hat[l_it], step) * second_deriv(&rho_hat[l_it], step)) /
        pow(pow(deriv(&rho_hat[l_it], step), 2) +
                pow(deriv(&eta_hat[l_it], step), 2),
            3 / 2);

    kcurve.SetPoint(l_it - 4, start + double(l_it) * step, curv);

    if (curv > max_curv) {
      max_curv = curv;
      best_reg = pow(10, start + double(l_it) * step);
    }
  }

  fls.Solve(best_reg, BCRegFactor);

  TFile *f = CheckOpenFile(OutputFile, "RECREATE");
  fls.Write(f);
  lcurve.Write("lcurve");
  kcurve.Write("kcurve");
  f->Write();
}
