// SPDX-License-Identifier: GPL-3.0-only
// reference: Dragonfly Room DSP (freeverb3 earlyref + progenitor2) as DSP.cpp drives them, an impulse in, the IR out (float32 stereo raw)
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <vector>
#include "freeverb/earlyref.hpp"
#include "freeverb/progenitor2.hpp"
#include "freeverb/biquad.hpp"
/* dfroom_ref FS OUT.raw : the impulse response of the Dragonfly Room (Medium Clear Room) as the plugin's DSP.cpp drives Freeverb3's
 * earlyref and progenitor2, float32 x 4 per sample: early L, early R, late L, late R (the early reflections' send into the late reverb
 * included). Built by tests/dfroom_ref.sh against a clone of dragonfly-reverb (GPL-3.0, Freeverb3 GPL-2+): nothing of it is copied here. */
int main(int argc, char **argv)
{
  double fs = argc > 1 ? atof(argv[1]) : 44100;
  int n = (int)(fs * 4);
  // Medium Clear Room: dry early esend late size width delay decay diffuse spin wander highcut earlydamp latedamp boost boostlpf lowcut
  double size = 12, width = 100, predelay = 8, decay = 0.4, diffuse = 70, spin = 0.8, wander = 40, earlydamp = 10000, latedamp = 9400, boost = 50, boostlpf = 600;
  fv3::earlyref_f early; fv3::progenitor2_f late;
  early.loadPresetReflection(FV3_EARLYREF_PRESET_1);
  early.setMuteOnChange(false); early.setdryr(0); early.setwet(0); early.setwidth(0.8); early.setLRDelay(0.3);
  early.setLRCrossApFreq(750, 4); early.setDiffusionApFreq(150, 4); early.setSampleRate(fs);
  late.setMuteOnChange(false); late.setwet(0); late.setdryr(0); late.setwidth(1.0); late.setSampleRate(fs);
  early.setRSFactor(size / 10.0); late.setRSFactor(size / 10.0);
  late.setbassboost(boost / 20.0 / pow(decay, 1.5) * (size / 10.0));
  early.setwidth(width / 120.0); late.setwidth(width / 100.0);
  late.setPreDelay(predelay); late.setrt60(decay);
  late.setidiffusion1(diffuse / 120.0); late.setodiffusion1(diffuse / 120.0);
  late.setspin(spin); late.setspin2(std::sqrt(100.0 - (10.0 - spin) * (10.0 - spin)) / 2.0);
  late.setwander(wander / 200.0 + 0.1); late.setwander2(wander / 200.0 + 0.1);
  early.setoutputlpf(earlydamp); late.setdamp(latedamp); late.setoutputdamp(latedamp); late.setdamp2(boostlpf);
  double early_send = 0.2;
  std::vector<float> inl(n, 0), inr(n, 0), el(n), er(n), ll(n), lr(n), lin(n), lin2(n);
  inl[0] = inr[0] = 1.0f;
  early.processreplace(inl.data(), inr.data(), el.data(), er.data(), n);
  for (int i = 0; i < n; i++) { lin[i] = early_send * el[i] + inl[i]; lin2[i] = early_send * er[i] + inr[i]; }
  late.processreplace(lin.data(), lin2.data(), ll.data(), lr.data(), n);
  // outputs: early and late separately (the plugin sums them with levels 10 % and 20 % x LATE_GAIN is not applied in DSP.cpp run: lateLevel only)
  FILE *f = fopen(argc > 2 ? argv[2] : "ir.raw", "wb");
  for (int i = 0; i < n; i++) { float v[4] = {el[i], er[i], ll[i], lr[i]}; fwrite(v, 4, 4, f); }
  fclose(f);
  return 0;
}
