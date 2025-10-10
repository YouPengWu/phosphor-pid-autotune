#include "sensorinfo.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <mutex>
#include <unordered_map>

namespace j = nlohmann;

namespace sensorinfo
{

static std::mutex gMutex;
static bool gLoaded = false;
static std::unordered_map<std::string, TempInfo> gMap;

bool loadFromFile(const std::string& path)
{
    std::lock_guard<std::mutex> lk(gMutex);
    if (gLoaded)
        return true;

    std::ifstream ifs(path);
    if (!ifs.good())
    {
        // Not fatal: fall back to defaults later.
        return false;
    }
    j::json root = j::json::parse(ifs, nullptr, /*allow_exceptions*/ false);
    if (!root.is_object())
        return false;

    if (root.contains("tempsensorinfo") && root["tempsensorinfo"].is_array())
    {
        for (const auto& it : root["tempsensorinfo"])
        {
            const std::string type = it.value("type", "");
            if (type.empty())
                continue;
            TempInfo ti;
            ti.qStepC = it.value("q", 0.0625);
            ti.accuracyC = it.value("accuracy_c", 0.5);
            ti.bits = it.value("bits", 0);
            ti.tconvMs = it.value("tconv_ms", it.value("tconvMs", 0));
            gMap[type] = ti;
        }
    }

    gLoaded = true;
    return true;
}

std::optional<TempInfo> lookupTempInfo(const std::string& type)
{
    std::lock_guard<std::mutex> lk(gMutex);
    auto it = gMap.find(type);
    if (it == gMap.end())
        return std::nullopt;
    return it->second;
}

void clearCache()
{
    std::lock_guard<std::mutex> lk(gMutex);
    gLoaded = false;
    gMap.clear();
}

} // namespace sensorinfo
