#pragma once
#include <algorithm>
#include <cmath>

namespace CubelessContact {
// An interval can be stationary only when BOTH endpoints support contact.
// Keep ankle and toe channels independent: toe-off is not full-foot stance.
inline float Probability(float Logit) {
    if (!std::isfinite(Logit)) return 0.f;
    return Logit >= 0.f ? 1.f / (1.f + std::exp(-Logit))
                       : std::exp(Logit) / (1.f + std::exp(Logit));
}
inline float Interval(float PreviousLogit, float CurrentLogit) {
    return std::min(Probability(PreviousLogit), Probability(CurrentLogit));
}
}
