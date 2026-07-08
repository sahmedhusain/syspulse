#include "header.h"

MemoryStats getMemoryStats()
{
    MemoryStats stats;
    ifstream file("/proc/meminfo");
    if (file.is_open())
    {
        string key;
        long long value;
        string unit;
        long long memFree = 0;
        long long memAvailable = 0;
        bool hasAvailable = false;
        long long swapFree = 0;

        while (file >> key >> value >> unit)
        {
            if (key == "MemTotal:")
            {
                stats.ramTotal = value * 1024;
            }
            else if (key == "MemFree:")
            {
                memFree = value * 1024;
            }
            else if (key == "MemAvailable:")
            {
                memAvailable = value * 1024;
                hasAvailable = true;
            }
            else if (key == "SwapTotal:")
            {
                stats.swapTotal = value * 1024;
            }
            else if (key == "SwapFree:")
            {
                swapFree = value * 1024;
            }
        }
        if (hasAvailable)
        {
            stats.ramUsed = stats.ramTotal - memAvailable;
        }
        else
        {
            stats.ramUsed = stats.ramTotal - memFree;
        }
        stats.swapUsed = stats.swapTotal - swapFree;
        return stats;
    }

    // Fallback/Mock for macOS testing (RAM: 16GB total, 8GB used; SWAP: 4GB total, 1GB used)
    stats.ramTotal = 16ULL * 1024 * 1024 * 1024;
    stats.ramUsed = 8ULL * 1024 * 1024 * 1024;
    stats.swapTotal = 4ULL * 1024 * 1024 * 1024;
    stats.swapUsed = 1ULL * 1024 * 1024 * 1024;
    return stats;
}
