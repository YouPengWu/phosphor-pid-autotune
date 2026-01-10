#pragma once

#include "../buildjson/buildjson.hpp"
#include "../core/steady_state.hpp"

namespace autotune::exp
{

// Run noise profile experiment:
// 1. Collect 'samples' at 'intervalSec'.
// 2. Compute stats (Slope, RMSE).
// 3. Log results if cfg has 'noiselog'.
autotune::steady::WindowStats runNoiseProfile(const autotune::Config& cfg,
                                              int samples, int intervalSec);

} // namespace autotune::exp
