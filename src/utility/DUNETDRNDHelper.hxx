#ifndef DUNETDRNDHELPER_SEEN
#define DUNETDRNDHELPER_SEEN

#include "CAFReader.hxx"

inline bool ND_FHC_Select(CAFReader const &ev) {
  return (ev.reco_q == -1) && (ev.muon_exit == 0) && (ev.Ehad_veto < 30);
}
inline bool ND_RHC_Select(CAFReader const &ev) {
  return (ev.reco_q == 1) && (ev.muon_exit == 0) && (ev.Ehad_veto < 30);
}
inline bool NotCathode_Select(double const &vtx_x) {
  constexpr double gap = 1.5;
  for (int i = -3; i < 4; ++i) {
    double cathode_center = i * 102.1; // cm
    if ((vtx_x > (cathode_center - gap / 2.0)) &&
        (vtx_x < (cathode_center + gap / 2.0))) {
      return false;
    }
  }
  return true;
}
inline bool LArActive_Select(CAFReader const &ev) {
  return (std::abs(ev.vtx_x) < 350) && (std::abs(ev.vtx_y) < 150) &&
         (ev.vtx_z > 0) && (ev.vtx_z < 400);
}
inline bool FV_Select(CAFReader const &ev) {
  return (std::abs(ev.vtx_x) < 300) && (std::abs(ev.vtx_y) < 100) &&
         (ev.vtx_z > 50) && (ev.vtx_z < 350) && NotCathode_Select(ev.vtx_x);
}

#endif
