#pragma once

#include <chrono>
#include <thread>

namespace autotune::timeutil
{

inline void sleepSeconds(int sec)
{
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}

} // namespace autotune::timeutil
