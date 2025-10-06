#pragma once

#include "../buildjson/buildjson.hpp"

#include <optional>

namespace autotune::dbuscfg
{

// Query EntityManager and return a fully-populated Config if available.
// On any error or if not found, returns std::nullopt.
std::optional<autotune::Config> loadConfigFromEntityManager();

} // namespace autotune::dbuscfg
