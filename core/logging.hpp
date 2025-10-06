#pragma once
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace autotune::log
{

inline std::string nowIso()
{
    using namespace std::chrono;
    auto t = system_clock::now();
    std::time_t tt = system_clock::to_time_t(t);
    std::tm tm = *std::gmtime(&tt);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// Append a timestamped line to the given path, creating directories as needed.
inline void appendLine(const std::string& path, const std::string& line)
{
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path());
    std::ofstream f(path, std::ios::app);
    f << nowIso() << " " << line << "\n";
}

} // namespace autotune::log
