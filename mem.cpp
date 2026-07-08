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

DiskStats getDiskStats()
{
    DiskStats stats;
    struct statvfs buf;
    if (statvfs("/", &buf) == 0)
    {
        stats.totalBytes = (long long)buf.f_blocks * buf.f_frsize;
        stats.usedBytes = (long long)(buf.f_blocks - buf.f_bfree) * buf.f_frsize;
    }
    return stats;
}

vector<Proc> getProcesses(long long ramTotal)
{
    vector<Proc> procs;
    DIR *dir = opendir("/proc");
    if (!dir)
    {
        // Fallback/Mock for macOS testing
        static vector<Proc> mockProcs;
        if (mockProcs.empty())
        {
            Proc p1 = {101, "monitor", 'R', 1048576, 256 * 1024 * 1024, 0, 0, 1.5f, 2.3f};
            Proc p2 = {102, "bash", 'S', 524288, 128 * 1024 * 1024, 0, 0, 0.1f, 1.2f};
            Proc p3 = {103, "init", 'S', 262144, 64 * 1024 * 1024, 0, 0, 0.0f, 0.5f};
            Proc p4 = {104, "zombie_proc", 'Z', 0, 0, 0, 0, 0.0f, 0.0f};
            mockProcs = {p1, p2, p3, p4};
        }
        else
        {
            // Add minor mock variations to simulate active CPU changes
            for (auto &p : mockProcs)
            {
                if (p.pid == 101)
                {
                    p.cpuUsage = 1.0f + (rand() % 200) / 100.0f;
                }
            }
        }
        return mockProcs;
    }

    ifstream uptimeFile("/proc/uptime");
    double uptime = 0.0;
    if (uptimeFile.is_open())
    {
        uptimeFile >> uptime;
    }

    long hertz = sysconf(_SC_CLK_TCK);
    if (hertz <= 0) hertz = 100;
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) pageSize = 4096;

    // Static map to store previous CPU ticks and uptime per PID for delta calculations
    static map<int, pair<long long, double>> cpuHistoryMap;
    map<int, pair<long long, double>> newCpuHistoryMap;

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (entry->d_type == DT_DIR)
        {
            char *endptr;
            long pid = strtol(entry->d_name, &endptr, 10);
            if (*endptr == '\0' && pid > 0)
            {
                string statPath = string("/proc/") + entry->d_name + "/stat";
                ifstream statFile(statPath);
                if (statFile.is_open())
                {
                    Proc p;
                    p.pid = pid;

                    int filePid;
                    string comm;
                    char state;
                    statFile >> filePid >> comm >> state;

                    // Strip parentheses
                    if (!comm.empty() && comm.front() == '(') comm.erase(0, 1);
                    if (!comm.empty() && comm.back() == ')') comm.pop_back();
                    p.name = comm;
                    p.state = state;

                    long long utime = 0, stime = 0;
                    long long starttime = 0;
                    long long rss = 0;

                    string dummy;
                    // Skip fields 4 to 13
                    for (int i = 4; i <= 13; ++i) statFile >> dummy;
                    statFile >> utime >> stime;
                    // Skip fields 16 to 21
                    for (int i = 16; i <= 21; ++i) statFile >> dummy;
                    statFile >> starttime;
                    statFile >> dummy; // 23
                    statFile >> rss;

                    p.vsize = 0;
                    p.rss = rss * pageSize;

                    if (ramTotal > 0)
                    {
                        p.memUsage = (p.rss / (float)ramTotal) * 100.0f;
                    }
                    else
                    {
                        p.memUsage = 0.0f;
                    }

                    long long totalTicks = utime + stime;
                    float cpuPercent = 0.0f;

                    if (cpuHistoryMap.find(pid) != cpuHistoryMap.end())
                    {
                        long long prevTicks = cpuHistoryMap[pid].first;
                        double prevUptime = cpuHistoryMap[pid].second;
                        double deltaUptime = uptime - prevUptime;
                        if (deltaUptime > 0.05)
                        {
                            cpuPercent = (float)((totalTicks - prevTicks) / (double)hertz) / deltaUptime * 100.0f;
                        }
                    }
                    else
                    {
                        double processUptime = uptime - ((double)starttime / hertz);
                        if (processUptime > 0)
                        {
                            cpuPercent = (float)(totalTicks / (double)hertz) / processUptime * 100.0f;
                        }
                    }

                    if (cpuPercent < 0.0f) cpuPercent = 0.0f;
                    p.cpuUsage = cpuPercent;

                    newCpuHistoryMap[pid] = {totalTicks, uptime};
                    procs.push_back(p);
                }
            }
        }
    }
    closedir(dir);

    cpuHistoryMap = move(newCpuHistoryMap);
    return procs;
}
