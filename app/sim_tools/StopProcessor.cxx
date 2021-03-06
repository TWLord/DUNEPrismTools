#include "StopDimensions.hxx"

#include "DetectorStop.hxx"

#include "ROOTUtility.hxx"
#include "PhysicsUtility.hxx"
#include "GetUsage.hxx"

#include "DepositsSummaryTreeReader.hxx"
#include "CondensedDepositsTreeReader.hxx"
#include "SimConfigTreeReader.hxx"
#include "StopConfigTreeReader.hxx"

#include "TH3D.h"
#include "TRandom3.h"
#include "TTree.h"

#include <limits>
#include <utility>
#include <algorithm>

// #define DEBUG

struct DetBox {
  double XOffset;
  double XWidth_nonveto;
  double YWidth_nonveto;
  double ZWidth_nonveto;

  double XWidth_det;
  double YWidth_det;
  double ZWidth_det;

  double X_Range_nonveto[2];
  double Y_Range_nonveto[2];
  double Z_Range_nonveto[2];

  size_t X_nonveto[2];
  size_t X_veto_left[2];
  size_t X_veto_right[2];

  double POTExposure;
};

std::vector<DetectorStop> DetectorStops;
std::vector<DetBox> Detectors;

std::string inpDir, outputFile;
std::string runPlanCfg, runPlanName = "";
double VetoThreshold = 10E-3;
double LepExitThreshold = 50E-3;
bool WriteOutNonStop = false;
Long64_t NMax = std::numeric_limits<Long64_t>::max();
Long64_t NSkip = 0;
double POTPerFile = 0xdeadbeef;
double TotalPOTPerFile = 0xdeadbeef;

void SayUsage(char const *argv[]) {
  std::cout << "[USAGE]: " << argv[0] << "\n"
            << GetUsageText(argv[0], "sim_tools") << std::endl;
}

void handleOpts(int argc, char const *argv[]) {
  int opt = 1;
  while (opt < argc) {
    if (std::string(argv[opt]) == "-?"||
               std::string(argv[opt]) == "--help") {
      SayUsage(argv);
      exit(0);
    } else if (std::string(argv[opt]) == "-i") {
      inpDir = argv[++opt];
    } else if (std::string(argv[opt]) == "-o") {
      outputFile = argv[++opt];
    } else if (std::string(argv[opt]) == "-r") {
      std::vector<std::string> params =
          ParseToVect<std::string>(argv[++opt], ",");

      runPlanCfg = params[0];

      if (params.size() > 1) {
        runPlanName = params[1];
      }
    } else if (std::string(argv[opt]) == "-v") {
      VetoThreshold = str2T<double>(argv[++opt])*1E-3;
    } else if (std::string(argv[opt]) == "-A") {
      WriteOutNonStop = true;
    } else if (std::string(argv[opt]) == "-n") {
      NMax = str2T<Long64_t>(argv[++opt]);
    } else if (std::string(argv[opt]) == "-ns") {
      NSkip = str2T<Long64_t>(argv[++opt]);
    } else if (std::string(argv[opt]) == "-P") {
      POTPerFile = str2T<double>(argv[++opt]);
    } else {
      std::cout << "[ERROR]: Unknown option: " << argv[opt] << std::endl;
      SayUsage(argv);
      exit(1);
    }
    opt++;
  }
}

enum OOFVInclusion {
  kIncludeFVYZ = 1,
  kIncludeOOFVYZ = 2,
  kIncludeWholeDet = 3
};

double Jaccumulate(double ***arr, size_t ind1, size_t ind2, double init,
                   OOFVInclusion fvincl) {
  double sum = init;
  for (size_t i = ind1; i < ind2; ++i) {
    if ((arr[i][1][1] != 0) && (!std::isnormal(arr[i][1][1]))) {
      std::cout << "[" << i << "][1][1]: bin non-normal = " << arr[i][1][1]
                << std::endl;
      throw;
    }

    if (fvincl & 1) {
      sum += arr[i][1][1];
    }

    if (fvincl & 2) {
      sum += arr[i][0][0];
      sum += arr[i][0][1];
      sum += arr[i][0][2];

      sum += arr[i][1][0];
      sum += arr[i][1][2];

      sum += arr[i][2][0];
      sum += arr[i][2][1];
      sum += arr[i][2][2];

      if ((arr[i][0][0] != 0) && (!std::isnormal(arr[i][0][0]))) {
        std::cout << "[" << i << "][0][0]: bin non-normal = " << arr[i][0][0]
                  << std::endl;
        throw;
      }
      if ((arr[i][0][1] != 0) && (!std::isnormal(arr[i][0][1]))) {
        std::cout << "[" << i << "][0][1]: bin non-normal = " << arr[i][0][1]
                  << std::endl;
        throw;
      }
      if ((arr[i][0][2] != 0) && (!std::isnormal(arr[i][0][2]))) {
        std::cout << "[" << i << "][0][2]: bin non-normal = " << arr[i][0][2]
                  << std::endl;
        throw;
      }

      if ((arr[i][1][0] != 0) && (!std::isnormal(arr[i][1][0]))) {
        std::cout << "[" << i << "][1][0]: bin non-normal = " << arr[i][1][0]
                  << std::endl;
        throw;
      }
      if ((arr[i][1][2] != 0) && (!std::isnormal(arr[i][1][2]))) {
        std::cout << "[" << i << "][1][2]: bin non-normal = " << arr[i][1][2]
                  << std::endl;
        throw;
      }

      if ((arr[i][2][0] != 0) && (!std::isnormal(arr[i][2][0]))) {
        std::cout << "[" << i << "][2][0]: bin non-normal = " << arr[i][2][0]
                  << std::endl;
        throw;
      }
      if ((arr[i][2][1] != 0) && (!std::isnormal(arr[i][2][1]))) {
        std::cout << "[" << i << "][2][1]: bin non-normal = " << arr[i][2][1]
                  << std::endl;
        throw;
      }
      if ((arr[i][2][2] != 0) && (!std::isnormal(arr[i][2][2]))) {
        std::cout << "[" << i << "][2][2]: bin non-normal = " << arr[i][2][2]
                  << std::endl;
        throw;
      }
    }
  }
  return sum;
}

int main(int argc, char const *argv[]) {
  TH1::SetDefaultSumw2();
  handleOpts(argc, argv);

  SimConfig sc(inpDir);

  // If input file does have POTPerFile and no override specified here, use it
  // as the POTPerFile.
  if ((sc.POTPerFile != 0xdeadbeef) && (POTPerFile == 0xdeadbeef)) {
    POTPerFile = sc.POTPerFile;
  }

  StopDimensions detdims(inpDir);
  TH3D *DetMap = detdims.BuildDetectorMap();

  CondensedDeposits *rdr = new CondensedDeposits(
      inpDir, sc.NXSteps, sc.NMaxTrackSteps,
      sc.timesep_us);

  if (!rdr->GetEntries()) {
    std::cout << "No valid input files found." << std::endl;
    return 1;
  }
  if (!runPlanCfg.length()) {
    std::cout << "[ERROR]: Found no run plan configuration file." << std::endl;
  }

  if (POTPerFile != 0xdeadbeef) {
    TotalPOTPerFile = POTPerFile * rdr->NFiles;
    std::cout << "[INFO]: Processing " << TotalPOTPerFile << " POT from "
              << rdr->NFiles << " input files." << std::endl;
  }

  TFile *outfile = CheckOpenFile(outputFile, "RECREATE");

  //Copy Sim Tree
  TTree *SimConfigTree_clone = sc.tree->CloneTree();
  SimConfigTree_clone->SetDirectory(outfile);

  // Read in det stops
  DetectorStops = ReadDetectorStopConfig(runPlanCfg, runPlanName);

  size_t NDets = DetectorStops.size();
  Detectors.reserve(NDets);

  StopConfig *stc = StopConfig::MakeTreeWriter();

  for (size_t d_it = 0; d_it < NDets; ++d_it) {
    DetBox db;
    // ActiveExent
    // CenterPosition
    db.XOffset = DetectorStops[d_it].CenterPosition[0] * 100.0;
    db.XWidth_det = DetectorStops[d_it].ActiveExent[0] * 100.0;
    db.YWidth_det = DetectorStops[d_it].ActiveExent[1] * 100.0;
    db.ZWidth_det = DetectorStops[d_it].ActiveExent[2] * 100.0;
    db.XWidth_nonveto = db.XWidth_det - 2 * detdims.VetoGap[0];
    db.YWidth_nonveto = db.YWidth_det - 2 * detdims.VetoGap[1];
    db.ZWidth_nonveto = db.ZWidth_det - 2 * detdims.VetoGap[2];
    db.POTExposure = DetectorStops[d_it].POTExposure;

    double Detlow = db.XOffset - db.XWidth_det / 2.0;
    double DetHigh = db.XOffset + db.XWidth_det / 2.0;

    db.X_Range_nonveto[0] = db.XOffset - db.XWidth_nonveto / 2.0;
    db.X_Range_nonveto[1] = db.XOffset + db.XWidth_nonveto / 2.0;

    db.Y_Range_nonveto[0] = -db.YWidth_nonveto / 2.0;
    db.Y_Range_nonveto[1] = db.YWidth_nonveto / 2.0;

    db.Z_Range_nonveto[0] = -db.ZWidth_nonveto / 2.0;
    db.Z_Range_nonveto[1] = db.ZWidth_nonveto / 2.0;

    std::cout << "[INFO]: Det: [" << Detlow << ", " << DetHigh << "], FV: ["
              << db.X_Range_nonveto[0] << ", " << db.X_Range_nonveto[1] << "]."
              << std::endl;

    db.X_nonveto[0] = DetMap->GetXaxis()->FindFixBin(db.X_Range_nonveto[0]) - 1;
    db.X_nonveto[1] = DetMap->GetXaxis()->FindFixBin(db.X_Range_nonveto[1]) - 1;

    db.X_veto_left[0] = DetMap->GetXaxis()->FindFixBin(Detlow) - 1;
    db.X_veto_left[1] =
        DetMap->GetXaxis()->FindFixBin(Detlow + detdims.VetoGap[0]) - 1;

    db.X_veto_right[0] =
        DetMap->GetXaxis()->FindFixBin(DetHigh - detdims.VetoGap[0]) - 1;
    db.X_veto_right[1] = DetMap->GetXaxis()->FindFixBin(DetHigh) - 1;

    stc->ActiveMin[0] = db.XOffset - db.XWidth_det / 2.0;
    stc->ActiveMax[0] = db.XOffset + db.XWidth_det / 2.0;
    stc->ActiveMin[1] = -db.YWidth_det / 2.0;
    stc->ActiveMax[1] = db.YWidth_det / 2.0;
    stc->ActiveMin[2] = -db.ZWidth_det / 2.0;
    stc->ActiveMax[2] = db.ZWidth_det / 2.0;
    std::copy_n(detdims.VetoGap,3,stc->VetoGap);
    for(size_t dim_it = 0; dim_it < 3; ++dim_it){
      stc->CenterPosition[dim_it] = DetectorStops[d_it].CenterPosition[dim_it]*100.0;
    }
    stc->POTExposure = DetectorStops[d_it].POTExposure;

    stc->Fill();

    Detectors.push_back(db);
  }

  std::cout << "[INFO]: Reading " << rdr->GetEntries() << " input entries."
            << std::endl;

  size_t loud_every = std::min(Long64_t(rdr->GetEntries()), NMax) / 10;
  TRandom3 *rnjesus = new TRandom3();

  DepositsSummary *OutputEDep =
    DepositsSummary::MakeTreeWriter(sc.timesep_us);

  std::vector<int> overlapping_stops;
  size_t NFills = 0;
  Long64_t NEntries = std::min(Long64_t(rdr->GetEntries()), NSkip + NMax);
  for (Long64_t e_it = NSkip; e_it < NEntries; ++e_it) {
    rdr->GetEntry(e_it);

    if (loud_every && !(e_it % loud_every)) {
      std::cout << "\r[INFO]: Read " << e_it << "/" << NEntries
                << " entries... ( vtx: {" << rdr->VertexPosition[0] << ", "
                << rdr->VertexPosition[1] << ", " << rdr->VertexPosition[2]
                << "}, Enu: " << rdr->nu_4mom[3] << " )" << std::endl;
    }

    // DetBox + Reader -> DepositsSummary -> Fill out tree
    //
    //
    // Jake: Adding in a vector of overlapping stops

    overlapping_stops.clear();

    int stop = -1;
    double stop_weight = 1;
    double POT_weight = 1;
    for (size_t d_it = 0; d_it < NDets; ++d_it) {
      DetBox &db = Detectors[d_it];

      if ((rdr->VertexPosition[0] < db.X_Range_nonveto[0]) ||
          (rdr->VertexPosition[0] > db.X_Range_nonveto[1]) ||
          (rdr->VertexPosition[1] < db.Y_Range_nonveto[0]) ||
          (rdr->VertexPosition[1] > db.Y_Range_nonveto[1]) ||
          (rdr->VertexPosition[2] < db.Z_Range_nonveto[0]) ||
          (rdr->VertexPosition[2] > db.Z_Range_nonveto[1])) {
        continue;
      }
      stop = d_it;
      POT_weight = db.POTExposure / TotalPOTPerFile;
      overlapping_stops.push_back(stop);
    }

    if (overlapping_stops.size() > 1) {  // Choose stop according to exposure

      double total_POT = 0.;

      for (size_t s_it = 0; s_it < overlapping_stops.size(); ++s_it) {
        total_POT += Detectors.at(s_it).POTExposure;
      }

      double rand_val = rnjesus->Uniform() * total_POT;
      double running_POT = 0;

      stop = overlapping_stops.back();
      for (size_t s_it = 0; s_it < overlapping_stops.size(); ++s_it) {
        running_POT += Detectors.at(s_it).POTExposure;
        if (rand_val < running_POT) {
          stop = overlapping_stops[s_it];
          stop_weight = total_POT / Detectors.at(s_it).POTExposure;
          if (TotalPOTPerFile != 0xdeadbeef) {
            POT_weight = Detectors.at(s_it).POTExposure / TotalPOTPerFile;
          }
          break;
        }
      }
    }

    OutputEDep->stop = stop;
    OutputEDep->stop_weight = stop_weight;
    OutputEDep->POT_weight = POT_weight;
    (*OutputEDep->EventCode) = (*rdr->EventCode);

    GENIECodeStringParser gcp(rdr->EventCode->GetString().Data());
    OutputEDep->GENIEInteractionTopology = static_cast<int>(gcp.channel);

    std::copy_n(rdr->VertexPosition, 3, OutputEDep->vtx);
    std::copy_n(rdr->nu_4mom, 4, OutputEDep->nu_4mom);
    OutputEDep->nu_PDG = rdr->nu_PDG;

    OutputEDep->y_True = rdr->y_True;
    OutputEDep->Q2_True = rdr->Q2_True;
    std::copy_n(rdr->FourMomTransfer_True, 4, OutputEDep->FourMomTransfer_True);
    OutputEDep->W_Rest = rdr->W_Rest;

    OutputEDep->PrimaryLepPDG = rdr->PrimaryLepPDG;
    std::copy_n(rdr->PrimaryLep_4mom, 4, OutputEDep->PrimaryLep_4mom);

    OutputEDep->NFSParts = rdr->NFSParts;
    std::copy_n(rdr->FSPart_PDG, rdr->NFSParts, OutputEDep->FSPart_PDG);
    OutputEDep->NFSPart4MomEntries = rdr->NFSPart4MomEntries;
    std::copy_n(rdr->FSPart_4Mom, rdr->NFSPart4MomEntries,
                OutputEDep->FSPart_4Mom);

    OutputEDep->NLep = rdr->NLep;
    OutputEDep->NPi0 = rdr->NPi0;
    OutputEDep->NPiC = rdr->NPiC;
    OutputEDep->NProton = rdr->NProton;
    OutputEDep->NNeutron = rdr->NNeutron;
    OutputEDep->NGamma = rdr->NGamma;
    OutputEDep->NOther = rdr->NOther;
    OutputEDep->NBaryonicRes = rdr->NBaryonicRes;
    OutputEDep->NAntiNucleons = rdr->NAntiNucleons;

    OutputEDep->EKinPi0_True = rdr->EKinPi0_True;
    OutputEDep->EMassPi0_True = rdr->EMassPi0_True;
    OutputEDep->EKinPiC_True = rdr->EKinPiC_True;
    OutputEDep->EMassPiC_True = rdr->EMassPiC_True;
    OutputEDep->EKinProton_True = rdr->EKinProton_True;
    OutputEDep->EMassProton_True = rdr->EMassProton_True;
    OutputEDep->EKinNeutron_True = rdr->EKinNeutron_True;
    OutputEDep->EMassNeutron_True = rdr->EMassNeutron_True;
    OutputEDep->EGamma_True = rdr->EGamma_True;
    OutputEDep->EOther_True = rdr->EOther_True;
    OutputEDep->Total_ENonPrimaryLep_True = rdr->ENonPrimaryLep_True;
    std::copy_n(rdr->TotalFS_3mom, 3, OutputEDep->TotalFS_3mom);
    OutputEDep->ENonPrimaryLep_KinNucleonTotalOther_True =
        rdr->EKinPi0_True + rdr->EMassPi0_True + rdr->EKinPiC_True +
        rdr->EMassPiC_True + rdr->EKinProton_True + rdr->EKinNeutron_True +
        rdr->EGamma_True + rdr->EOther_True;

    OutputEDep->ERecProxy_True =
        rdr->EKinPi0_True + rdr->EMassPi0_True + rdr->EKinPiC_True +
        rdr->EMassPiC_True + rdr->EKinProton_True + rdr->EKinNeutron_True +
        rdr->EGamma_True + rdr->EOther_True - rdr->NBaryonicRes * 0.938 +
        2 * rdr->NAntiNucleons * 0.938 + rdr->PrimaryLep_4mom[3];

    OutputEDep->IsNumu = (abs(rdr->nu_PDG) == 14);
    OutputEDep->IsAntinu = (rdr->nu_PDG < 0);
    OutputEDep->IsCC = !(abs(rdr->nu_PDG) - abs(rdr->PrimaryLepPDG) - 1);
    OutputEDep->Is0Pi =
        ((rdr->NPiC + rdr->NPi0 + rdr->NGamma + rdr->NOther) == 0);
    OutputEDep->Is1PiC =
        ((rdr->NGamma + rdr->NOther + rdr->NPi0) == 0) && (rdr->NPiC == 1);
    OutputEDep->Is1Pi0 =
        ((rdr->NGamma + rdr->NOther + rdr->NPiC) == 0) && (rdr->NPi0 == 1);
    OutputEDep->Is1Pi = OutputEDep->Is1PiC || OutputEDep->Is1Pi0;
    OutputEDep->IsNPi =
        ((rdr->NGamma + rdr->NOther) == 0) && ((rdr->NPiC + rdr->NPi0) > 1);
    OutputEDep->IsOther = (rdr->NGamma + rdr->NOther);
    OutputEDep->Topology = (OutputEDep->IsCC ? 1 : -1) *
                           (1 * OutputEDep->Is0Pi + 2 * OutputEDep->Is1PiC +
                            3 * OutputEDep->Is1Pi0 + 4 * OutputEDep->IsNPi +
                            5 * OutputEDep->IsOther);

    if (stop == -1) {
      if (WriteOutNonStop) {
        OutputEDep->Fill();
        NFills++;
      }
      OutputEDep->Reset();

      continue;
    }

    DetBox &stopBox = Detectors[stop];
    OutputEDep->vtxInDetX = OutputEDep->vtx[0] - stopBox.XOffset;
    OutputEDep->XOffset = stopBox.XOffset;

    double DetXLow = stopBox.XOffset - stopBox.XWidth_det / 2.0;
    double DetXHigh = stopBox.XOffset + stopBox.XWidth_det / 2.0;
    double DetYLow = 0. - stopBox.YWidth_det / 2.0;
    double DetYHigh = stopBox.YWidth_det / 2.0;
    double DetZLow = 0. - stopBox.ZWidth_det / 2.0;
    double DetZHigh = stopBox.ZWidth_det / 2.0;

    // Checking if lepton exits -- muon only
    OutputEDep->LepExitBack = false;
    OutputEDep->LepExitFront = false;
    OutputEDep->LepExitYLow = false;
    OutputEDep->LepExitYHigh = false;
    OutputEDep->LepExitXLow = false;
    OutputEDep->LepExitXHigh = false;
    OutputEDep->LepExit = false;

    if (abs(rdr->PrimaryLepPDG) == 13) {
      for (int i = 0; i < rdr->NMuonTrackSteps; ++i) {
        if (OutputEDep->LepExit) {
          break;
        }

        double posX = rdr->MuonTrackPos[i][0];
        double posY = rdr->MuonTrackPos[i][1];
        double posZ = rdr->MuonTrackPos[i][2];
        if ((posX == 0xdeadbeef) && (posY == 0xdeadbeef) &&
            (posZ == 0xdeadbeef)) {
          std::cout << "[INFO]: Step " << i << ", lep pos deadbeef."
                    << std::endl;
          break;
        }

        double dX = 0., dY = 0., dZ = 0.;
        bool exitX = false, exitY = false, exitZ = false;

        if (posX <= DetXLow || posX >= DetXHigh) {
          dX = fabs(DetXLow - posX);
          exitX = true;
        } else {
          exitX = false;
        }

        if (posY <= DetYLow || posY >= DetYHigh) {
          dY = fabs(DetYLow - posY);
          exitY = true;
        } else {
          exitY = false;
        }

        if (posZ <= DetZLow || posZ >= DetZHigh) {
          dZ = fabs(DetZLow - posZ);
          exitZ = true;
        } else {
          exitZ = false;
        }

        if (exitX || exitY || exitZ) {
          if (dX > dY && dX > dZ) {
            OutputEDep->LepExitBack = false;
            OutputEDep->LepExitFront = false;
            OutputEDep->LepExitYLow = false;
            OutputEDep->LepExitYHigh = false;

            if (posX <= DetXLow) {
              OutputEDep->LepExitXLow = true;
              OutputEDep->LepExitXHigh = false;
            } else if (posX >= DetXHigh) {
              OutputEDep->LepExitXLow = false;
              OutputEDep->LepExitXHigh = true;
            } else {
              std::cout << "ERROR X" << std::endl;
              throw;
            }
          } else if (dY > dX && dY > dZ) {
            OutputEDep->LepExitBack = false;
            OutputEDep->LepExitFront = false;
            OutputEDep->LepExitXLow = false;
            OutputEDep->LepExitXHigh = false;

            if (posY <= DetYLow) {
              OutputEDep->LepExitYLow = true;
              OutputEDep->LepExitYHigh = false;
            } else if (posY >= DetYHigh) {
              OutputEDep->LepExitYLow = false;
              OutputEDep->LepExitYHigh = true;
            } else {
              std::cout << "ERROR Y" << std::endl;
              throw;
            }
          } else if (dZ > dX && dZ > dY) {
            OutputEDep->LepExitXLow = false;
            OutputEDep->LepExitXHigh = false;
            OutputEDep->LepExitYLow = false;
            OutputEDep->LepExitYHigh = false;

            if (posZ <= DetZLow) {
              OutputEDep->LepExitBack = false;
              OutputEDep->LepExitFront = true;
            } else if (posZ >= DetZHigh) {
              OutputEDep->LepExitFront = false;
              OutputEDep->LepExitBack = true;
            } else {
              std::cout << "ERROR Z" << std::endl;
              throw;
            }
          }
          OutputEDep->LepExit = true;
          std::copy_n(rdr->MuonTrackPos[i], 3, OutputEDep->LepExitingPos);
          std::copy_n(rdr->MuonTrackMom[i], 3, OutputEDep->LepExitingMom);

          OutputEDep->LepExitKE =
              sqrt(
                  pow(0.1056, 2) +
                  (OutputEDep->LepExitingMom[0] * OutputEDep->LepExitingMom[0] +
                   OutputEDep->LepExitingMom[1] * OutputEDep->LepExitingMom[1] +
                   OutputEDep->LepExitingMom[2] *
                       OutputEDep->LepExitingMom[2])) -
              0.1056;
          OutputEDep->LepExit_AboveThresh =
              (OutputEDep->LepExitKE > LepExitThreshold);

#ifdef DEBUG
          std::cout << "[INFO]: Muon stopped tracking at step [" << i
                    << "] at {" << rdr->MuonTrackPos[i][0] << ", "
                    << rdr->MuonTrackPos[i][1] << ", "
                    << rdr->MuonTrackPos[i][2] << " } with 3-mom {"
                    << rdr->MuonTrackMom[i][0] << ", "
                    << rdr->MuonTrackMom[i][1] << ", "
                    << rdr->MuonTrackMom[i][2] << " }." << std::endl;
#endif
          break;
        }
      }

      OutputEDep->LepExitTopology =
          (OutputEDep->LepExit ? 1 : 0) *
          (1 * OutputEDep->LepExitBack + 2 * OutputEDep->LepExitFront +
           3 * OutputEDep->LepExitYLow + 4 * OutputEDep->LepExitYHigh +
           5 * OutputEDep->LepExitXLow + 6 * OutputEDep->LepExitXHigh);
    }
    ////////End exiting lepton section
    OutputEDep->LepDep_FV = Jaccumulate(rdr->LepDep, stopBox.X_nonveto[0],
                                        stopBox.X_nonveto[1], 0, kIncludeFVYZ);

#ifdef DEBUG
    for (Int_t x_it = 0; x_it < detdims.NXSteps; ++x_it) {
      for (size_t y_it = 0; y_it < 3; ++y_it) {
        for (size_t z_it = 0; z_it < 3; ++z_it) {
          if (rdr->ProtonDep[x_it][y_it][z_it]) {
            std::cout << "[INFO]: Bin " << x_it << ", " << y_it << ", " << z_it
                      << ", ProtonDep content = "
                      << rdr->ProtonDep[x_it][y_it][z_it] << std::endl;
          }

          if (rdr->timesep_us != 0xdeadbeef) {
            if (rdr->ProtonDep_timesep[x_it][y_it][z_it]) {
              std::cout << "[INFO]: Bin " << x_it << ", " << y_it << ", "
                        << z_it << ", ProtonDep_timesep content = "
                        << rdr->LepDep_timesep[x_it][y_it][z_it] << std::endl;
            }
          }
        }
      }
    }

#endif

    OutputEDep->LepDep_veto =
        Jaccumulate(rdr->LepDep, stopBox.X_veto_left[0], stopBox.X_veto_left[1],
                    0, kIncludeWholeDet) +
        Jaccumulate(rdr->LepDep, stopBox.X_veto_right[0],
                    stopBox.X_veto_right[1], 0, kIncludeWholeDet) +

        Jaccumulate(rdr->LepDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeOOFVYZ);

    OutputEDep->LepDepDescendent_FV = Jaccumulate(
        rdr->LepDaughterDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0, kIncludeFVYZ);
    OutputEDep->LepDepDescendent_veto =
        Jaccumulate(rdr->LepDaughterDep, stopBox.X_veto_left[0],
                    stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->LepDaughterDep, stopBox.X_veto_right[0],
                    stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->LepDaughterDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeOOFVYZ);

    OutputEDep->ProtonDep_FV =
        Jaccumulate(rdr->ProtonDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeFVYZ) +
        Jaccumulate(rdr->ProtonDaughterDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeFVYZ);
    OutputEDep->ProtonDep_veto =
        Jaccumulate(rdr->ProtonDep, stopBox.X_veto_left[0],
                    stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->ProtonDaughterDep, stopBox.X_veto_left[0],
                    stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->ProtonDep, stopBox.X_veto_right[0],
                    stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->ProtonDaughterDep, stopBox.X_veto_right[0],
                    stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->ProtonDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeOOFVYZ) +
        Jaccumulate(rdr->ProtonDaughterDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeOOFVYZ);

    OutputEDep->NeutronDep_FV =
        Jaccumulate(rdr->NeutronDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeFVYZ) +
        Jaccumulate(rdr->NeutronDaughterDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1],
                    0, kIncludeFVYZ);
    OutputEDep->NeutronDep_veto =
        Jaccumulate(rdr->NeutronDep, stopBox.X_veto_left[0],
                    stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->NeutronDaughterDep, stopBox.X_veto_left[0],
                    stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->NeutronDep, stopBox.X_veto_right[0],
                    stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->NeutronDaughterDep, stopBox.X_veto_right[0],
                    stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->NeutronDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeOOFVYZ) +
        Jaccumulate(rdr->NeutronDaughterDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1],
                    0, kIncludeOOFVYZ);

    OutputEDep->NeutronDep_ChrgWAvgTime_FV =
        Jaccumulate(rdr->NeutronDep_ChrgWSumTime, stopBox.X_nonveto[0],
                    stopBox.X_nonveto[1], 0, kIncludeFVYZ) +
        Jaccumulate(rdr->NeutronDaughterDep_ChrgWSumTime, stopBox.X_nonveto[0],
                    stopBox.X_nonveto[1], 0, kIncludeFVYZ);
    OutputEDep->NeutronDep_ChrgWAvgTime_veto =
        Jaccumulate(rdr->NeutronDep_ChrgWSumTime, stopBox.X_veto_left[0],
                    stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->NeutronDaughterDep_ChrgWSumTime,
                    stopBox.X_veto_left[0], stopBox.X_veto_left[1], 0,
                    kIncludeWholeDet) +
        Jaccumulate(rdr->NeutronDep_ChrgWSumTime, stopBox.X_veto_right[0],
                    stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->NeutronDaughterDep_ChrgWSumTime,
                    stopBox.X_veto_right[0], stopBox.X_veto_right[1], 0,
                    kIncludeWholeDet) +
        Jaccumulate(rdr->NeutronDep_ChrgWSumTime, stopBox.X_nonveto[0],
                    stopBox.X_nonveto[1], 0, kIncludeOOFVYZ) +
        Jaccumulate(rdr->NeutronDaughterDep_ChrgWSumTime, stopBox.X_nonveto[0],
                    stopBox.X_nonveto[1], 0, kIncludeOOFVYZ);

    OutputEDep->NeutronDep_ChrgWAvgTime_FV /= OutputEDep->NeutronDep_FV;
    OutputEDep->NeutronDep_ChrgWAvgTime_veto /= OutputEDep->NeutronDep_veto;

    OutputEDep->PiCDep_FV = Jaccumulate(rdr->PiCDep, stopBox.X_nonveto[0],
                                        stopBox.X_nonveto[1], 0, kIncludeFVYZ) +
                            Jaccumulate(rdr->PiCDaughterDep, stopBox.X_nonveto[0],
                                        stopBox.X_nonveto[1], 0, kIncludeFVYZ);
    OutputEDep->PiCDep_veto =
        Jaccumulate(rdr->PiCDep, stopBox.X_veto_left[0], stopBox.X_veto_left[1],
                    0, kIncludeWholeDet) +
        Jaccumulate(rdr->PiCDaughterDep, stopBox.X_veto_left[0],
                    stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->PiCDep, stopBox.X_veto_right[0],
                    stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->PiCDaughterDep, stopBox.X_veto_right[0],
                    stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->PiCDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeOOFVYZ) +
        Jaccumulate(rdr->PiCDaughterDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeOOFVYZ);

    OutputEDep->Pi0Dep_FV = Jaccumulate(rdr->Pi0Dep, stopBox.X_nonveto[0],
                                        stopBox.X_nonveto[1], 0, kIncludeFVYZ) +
                            Jaccumulate(rdr->Pi0DaughterDep, stopBox.X_nonveto[0],
                                        stopBox.X_nonveto[1], 0, kIncludeFVYZ);
    OutputEDep->Pi0Dep_veto =
        Jaccumulate(rdr->Pi0Dep, stopBox.X_veto_left[0], stopBox.X_veto_left[1],
                    0, kIncludeWholeDet) +
        Jaccumulate(rdr->Pi0DaughterDep, stopBox.X_veto_left[0],
                    stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->Pi0Dep, stopBox.X_veto_right[0],
                    stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->Pi0DaughterDep, stopBox.X_veto_right[0],
                    stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->Pi0Dep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeOOFVYZ) +
        Jaccumulate(rdr->Pi0DaughterDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeOOFVYZ);

    OutputEDep->OtherDep_FV =
        Jaccumulate(rdr->OtherDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeFVYZ) +
        Jaccumulate(rdr->OtherDaughterDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeFVYZ);
    OutputEDep->OtherDep_veto =
        Jaccumulate(rdr->OtherDep, stopBox.X_veto_left[0],
                    stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->OtherDaughterDep, stopBox.X_veto_left[0],
                    stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->OtherDep, stopBox.X_veto_right[0],
                    stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->OtherDaughterDep, stopBox.X_veto_right[0],
                    stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
        Jaccumulate(rdr->OtherDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeOOFVYZ) +
        Jaccumulate(rdr->OtherDaughterDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                    kIncludeOOFVYZ);

    OutputEDep->TotalNonlep_Dep_FV =
        OutputEDep->ProtonDep_FV + OutputEDep->NeutronDep_FV +
        OutputEDep->PiCDep_FV + OutputEDep->Pi0Dep_FV + OutputEDep->OtherDep_FV;

    OutputEDep->TotalNonlep_Dep_veto =
        OutputEDep->ProtonDep_veto + OutputEDep->NeutronDep_veto +
        OutputEDep->PiCDep_veto + OutputEDep->Pi0Dep_veto +
        OutputEDep->OtherDep_veto;

    if (((OutputEDep->TotalNonlep_Dep_veto != 0) &&
         (!std::isnormal(OutputEDep->TotalNonlep_Dep_veto))) ||
        ((OutputEDep->TotalNonlep_Dep_FV != 0) &&
         (!std::isnormal(OutputEDep->TotalNonlep_Dep_FV)))) {
      std::cout << "\n[INFO] XBins: " << stopBox.X_nonveto[0] << " -- "
                << stopBox.X_nonveto[1] << ", Veto left: " << stopBox.X_veto_left[0]
                << " -- " << stopBox.X_veto_left[1]
                << ", Veto right: " << stopBox.X_veto_right[0] << " -- "
                << stopBox.X_veto_right[1] << std::endl;

      std::cout << "FV -- Total: " << OutputEDep->TotalNonlep_Dep_FV
                << ", Proton: " << OutputEDep->ProtonDep_FV
                << ", Neutron: " << OutputEDep->NeutronDep_FV
                << ", PiC: " << OutputEDep->PiCDep_FV
                << ", Pi0: " << OutputEDep->Pi0Dep_FV
                << ", Other: " << OutputEDep->OtherDep_FV << std::endl;

      std::cout << "Veto -- Total: " << OutputEDep->TotalNonlep_Dep_veto
                << ", Proton: " << OutputEDep->ProtonDep_veto
                << ", Neutron: " << OutputEDep->NeutronDep_veto
                << ", PiC: " << OutputEDep->PiCDep_veto
                << ", Pi0: " << OutputEDep->Pi0Dep_veto
                << ", Other: " << OutputEDep->OtherDep_veto << std::endl
                << std::endl;
    }

    if (rdr->timesep_us != 0xdeadbeef) {
      OutputEDep->LepDep_timesep_FV =
          Jaccumulate(rdr->LepDep_timesep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                      kIncludeFVYZ);

      OutputEDep->LepDep_timesep_veto =
          Jaccumulate(rdr->LepDep_timesep, stopBox.X_veto_left[0],
                      stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->LepDep_timesep, stopBox.X_veto_right[0],
                      stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->LepDep_timesep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                      kIncludeOOFVYZ);

      OutputEDep->LepDepDescendent_timesep_FV =
          Jaccumulate(rdr->LepDaughterDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                      kIncludeFVYZ);
      OutputEDep->LepDepDescendent_timesep_veto =
          Jaccumulate(rdr->LepDaughterDep_timesep, stopBox.X_veto_left[0],
                      stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->LepDaughterDep_timesep, stopBox.X_veto_right[0],
                      stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->LepDaughterDep_timesep, stopBox.X_nonveto[0],
                      stopBox.X_nonveto[1], 0, kIncludeOOFVYZ);

      OutputEDep->ProtonDep_timesep_FV =
          Jaccumulate(rdr->ProtonDep_timesep, stopBox.X_nonveto[0], stopBox.X_nonveto[1],
                      0, kIncludeFVYZ) +
          Jaccumulate(rdr->ProtonDaughterDep_timesep, stopBox.X_nonveto[0],
                      stopBox.X_nonveto[1], 0, kIncludeFVYZ);

      OutputEDep->ProtonDep_timesep_veto =
          Jaccumulate(rdr->ProtonDep_timesep, stopBox.X_veto_left[0],
                      stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->ProtonDaughterDep_timesep, stopBox.X_veto_left[0],
                      stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->ProtonDep_timesep, stopBox.X_veto_right[0],
                      stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->ProtonDaughterDep_timesep, stopBox.X_veto_right[0],
                      stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->ProtonDep_timesep, stopBox.X_nonveto[0], stopBox.X_nonveto[1],
                      0, kIncludeOOFVYZ) +
          Jaccumulate(rdr->ProtonDaughterDep_timesep, stopBox.X_nonveto[0],
                      stopBox.X_nonveto[1], 0, kIncludeOOFVYZ);

      OutputEDep->NeutronDep_timesep_FV =
          Jaccumulate(rdr->NeutronDep_timesep, stopBox.X_nonveto[0], stopBox.X_nonveto[1],
                      0, kIncludeFVYZ) +
          Jaccumulate(rdr->NeutronDaughterDep_timesep, stopBox.X_nonveto[0],
                      stopBox.X_nonveto[1], 0, kIncludeFVYZ);
      OutputEDep->NeutronDep_timesep_veto =
          Jaccumulate(rdr->NeutronDep_timesep, stopBox.X_veto_left[0],
                      stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->NeutronDaughterDep_timesep, stopBox.X_veto_left[0],
                      stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->NeutronDep_timesep, stopBox.X_veto_right[0],
                      stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->NeutronDaughterDep_timesep, stopBox.X_veto_right[0],
                      stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->NeutronDep_timesep, stopBox.X_nonveto[0], stopBox.X_nonveto[1],
                      0, kIncludeOOFVYZ) +
          Jaccumulate(rdr->NeutronDaughterDep_timesep, stopBox.X_nonveto[0],
                      stopBox.X_nonveto[1], 0, kIncludeOOFVYZ);

      OutputEDep->PiCDep_timesep_FV =
          Jaccumulate(rdr->PiCDep_timesep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                      kIncludeFVYZ) +
          Jaccumulate(rdr->PiCDaughterDep_timesep, stopBox.X_nonveto[0],
                      stopBox.X_nonveto[1], 0, kIncludeFVYZ);
      OutputEDep->PiCDep_timesep_veto =
          Jaccumulate(rdr->PiCDep_timesep, stopBox.X_veto_left[0],
                      stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->PiCDaughterDep_timesep, stopBox.X_veto_left[0],
                      stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->PiCDep_timesep, stopBox.X_veto_right[0],
                      stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->PiCDaughterDep_timesep, stopBox.X_veto_right[0],
                      stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->PiCDep_timesep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                      kIncludeOOFVYZ) +
          Jaccumulate(rdr->PiCDaughterDep_timesep, stopBox.X_nonveto[0],
                      stopBox.X_nonveto[1], 0, kIncludeOOFVYZ);

      OutputEDep->Pi0Dep_timesep_FV =
          Jaccumulate(rdr->Pi0Dep_timesep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                      kIncludeFVYZ) +
          Jaccumulate(rdr->Pi0DaughterDep_timesep, stopBox.X_nonveto[0],
                      stopBox.X_nonveto[1], 0, kIncludeFVYZ);
      OutputEDep->Pi0Dep_timesep_veto =
          Jaccumulate(rdr->Pi0Dep_timesep, stopBox.X_veto_left[0],
                      stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->Pi0DaughterDep_timesep, stopBox.X_veto_left[0],
                      stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->Pi0Dep_timesep, stopBox.X_veto_right[0],
                      stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->Pi0DaughterDep_timesep, stopBox.X_veto_right[0],
                      stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->Pi0Dep_timesep, stopBox.X_nonveto[0], stopBox.X_nonveto[1], 0,
                      kIncludeOOFVYZ) +
          Jaccumulate(rdr->Pi0DaughterDep_timesep, stopBox.X_nonveto[0],
                      stopBox.X_nonveto[1], 0, kIncludeOOFVYZ);

      OutputEDep->OtherDep_timesep_FV =
          Jaccumulate(rdr->OtherDep_timesep, stopBox.X_nonveto[0], stopBox.X_nonveto[1],
                      0, kIncludeFVYZ) +
          Jaccumulate(rdr->OtherDaughterDep_timesep, stopBox.X_nonveto[0],
                      stopBox.X_nonveto[1], 0, kIncludeFVYZ);
      OutputEDep->OtherDep_timesep_veto =
          Jaccumulate(rdr->OtherDep_timesep, stopBox.X_veto_left[0],
                      stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->OtherDaughterDep_timesep, stopBox.X_veto_left[0],
                      stopBox.X_veto_left[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->OtherDep_timesep, stopBox.X_veto_right[0],
                      stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->OtherDaughterDep_timesep, stopBox.X_veto_right[0],
                      stopBox.X_veto_right[1], 0, kIncludeWholeDet) +
          Jaccumulate(rdr->OtherDep_timesep, stopBox.X_nonveto[0], stopBox.X_nonveto[1],
                      0, kIncludeOOFVYZ) +
          Jaccumulate(rdr->OtherDaughterDep, stopBox.X_nonveto[0], stopBox.X_nonveto[1],
                      0, kIncludeOOFVYZ);

      if (OutputEDep->ProtonDep_FV &&
          (OutputEDep->ProtonDep_FV == OutputEDep->ProtonDep_timesep_FV)) {
        std::cout
            << "\n[ERROR]: Found identical deposits before and after timesep: "
            << OutputEDep->ProtonDep_FV << std::endl;
        throw;
      }

      OutputEDep->TotalNonlep_Dep_timesep_FV =
          OutputEDep->ProtonDep_timesep_FV + OutputEDep->NeutronDep_timesep_FV +
          OutputEDep->PiCDep_timesep_FV + OutputEDep->Pi0Dep_timesep_FV +
          OutputEDep->OtherDep_timesep_FV;

      OutputEDep->TotalNonlep_Dep_timesep_veto =
          OutputEDep->ProtonDep_timesep_veto +
          OutputEDep->NeutronDep_timesep_veto +
          OutputEDep->PiCDep_timesep_veto + OutputEDep->Pi0Dep_timesep_veto +
          OutputEDep->OtherDep_timesep_veto;
    }

    OutputEDep->HadrShowerContainedInFV =
        (OutputEDep->TotalNonlep_Dep_veto < VetoThreshold);
    OutputEDep->PrimaryLeptonContainedInFV =
        (OutputEDep->LepDep_veto < VetoThreshold);

    NFills++;
    OutputEDep->Fill();
    OutputEDep->Reset();
  }

  std::cout << "\r[INFO]: Read " << rdr->GetEntries() << " entries."
            << std::endl;
  std::cout << "[INFO]: Filled output tree " << NFills << " times."
            << std::endl;

  outfile->Write();
  outfile->Close();
}
