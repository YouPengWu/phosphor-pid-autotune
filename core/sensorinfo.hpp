#pragma once
#include <optional>
#include <string>

namespace sensorinfo
{

// Physical temperature sensor spec resolved by "sensortype".
struct TempInfo
{
    double qStepC{0.0625}; // °C per LSB (quantization step)
    double accuracyC{0.5}; // °C typical absolute accuracy
    int bits{0};           // optional, informational
    int tconvMs{0};        // optional, informational
};

// Load a JSON database once and cache; safe to call multiple times.
// Return true if loaded (or already loaded). Non-fatal if missing file.
bool loadFromFile(const std::string& path);

// Lookup by sensor type, e.g., "tmp75". Returns nullopt if not found.
std::optional<TempInfo> lookupTempInfo(const std::string& type);

// Clear cache for tests or hot reloads.
void clearCache();

} // namespace sensorinfo
