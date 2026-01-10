#include "time_utils.hpp"
#include <chrono>
#include <thread>

namespace autotune::timeutil
{

void sleepSeconds(int sec)
{
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}

} // namespace autotune::timeutil
