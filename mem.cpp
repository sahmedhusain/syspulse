#include "header.h"

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <unistd.h>
#include <libproc.h>
#endif

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
#ifdef __APPLE__
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    int64_t totalRam = 0;
    size_t len = sizeof(totalRam);
    if (sysctl(mib, 2, &totalRam, &len, NULL, 0) == 0)
    {
        stats.ramTotal = totalRam;
    }

    mach_port_t host_port = mach_host_self();
    mach_msg_type_number_t host_size = sizeof(vm_statistics64_data_t) / sizeof(integer_t);
    vm_statistics64_data_t vm_stat;
    if (host_statistics64(host_port, HOST_VM_INFO64, (host_info64_t)&vm_stat, &host_size) == KERN_SUCCESS)
    {
        long long pageSize = sysconf(_SC_PAGESIZE);
        long long freeRam = (vm_stat.free_count + vm_stat.inactive_count) * pageSize;
        stats.ramUsed = stats.ramTotal - freeRam;
    }

    struct xsw_usage swapusage;
    size_t swapsize = sizeof(swapusage);
    if (sysctlbyname("vm.swapusage", &swapusage, &swapsize, NULL, 0) == 0)
    {
        stats.swapTotal = swapusage.xsu_total;
        stats.swapUsed = swapusage.xsu_used;
    }
    return stats;
#endif

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
#ifdef __APPLE__
        int num_procs = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
        if (num_procs > 0)
        {
            vector<pid_t> pids(num_procs / sizeof(pid_t));
            num_procs = proc_listpids(PROC_ALL_PIDS, 0, pids.data(), num_procs * sizeof(pid_t));

            static map<int, pair<long long, double>> cpuHistoryMap;
            map<int, pair<long long, double>> newCpuHistoryMap;
            
            // Inline system uptime calculation for macOS
            double uptime = 0.0;
            struct timeval boottime;
            size_t len = sizeof(boottime);
            int mib[2] = {CTL_KERN, KERN_BOOTTIME};
            if (sysctl(mib, 2, &boottime, &len, NULL, 0) >= 0)
            {
                time_t bsec = boottime.tv_sec;
                time_t csec = time(NULL);
                uptime = difftime(csec, bsec);
            }

            for (int i = 0; i < num_procs / sizeof(pid_t); ++i)
            {
                pid_t pid = pids[i];
                if (pid <= 0) continue;

                struct proc_bsdinfo bsdinfo;
                if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsdinfo, sizeof(bsdinfo)) == sizeof(bsdinfo))
                {
                    Proc p;
                    p.pid = pid;
                    p.name = "(" + string(bsdinfo.pbi_name) + ")";

                    string state = "sleeping";
                    switch (bsdinfo.pbi_status)
                    {
                        case 1: state = "sleeping"; break; // Idle
                        case 2: state = "running"; break;  // Run
                        case 3: state = "sleeping"; break; // Sleep
                        case 4: state = "stopped"; break;  // Stop
                        case 5: state = "zombie"; break;   // Zombie
                    }
                    p.state = state;

                    struct proc_taskinfo taskinfo;
                    if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &taskinfo, sizeof(taskinfo)) == sizeof(taskinfo))
                    {
                        p.rss = taskinfo.pti_resident_size;
                        p.vsize = taskinfo.pti_virtual_size;

                        long long totalTicks = taskinfo.pti_total_user + taskinfo.pti_total_system;
                        float cpuPercent = 0.0f;
                        if (cpuHistoryMap.find(pid) != cpuHistoryMap.end())
                        {
                            long long prevTicks = cpuHistoryMap[pid].first;
                            double prevUptime = cpuHistoryMap[pid].second;
                            double deltaUptime = uptime - prevUptime;
                            if (deltaUptime > 0.05)
                            {
                                cpuPercent = (float)(((totalTicks - prevTicks) / 1000000000.0) / deltaUptime) * 100.0f;
                            }
                        }
                        p.cpuUsage = cpuPercent;
                        newCpuHistoryMap[pid] = {totalTicks, uptime};
                    }
                    else
                    {
                        p.rss = 0;
                        p.vsize = 0;
                        p.cpuUsage = 0.0f;
                    }

                    if (ramTotal > 0)
                    {
                        p.memUsage = (p.rss / (float)ramTotal) * 100.0f;
                    }
                    else
                    {
                        p.memUsage = 0.0f;
                    }

                    procs.push_back(p);
                }
            }
            cpuHistoryMap = move(newCpuHistoryMap);
            return procs;
        }
#endif
        // Fallback/Mock for non-macOS/non-Linux testing
        static vector<Proc> mockProcs;
        if (mockProcs.empty())
        {
            Proc p1 = {101, "(monitor)", "running", 1048576, 256 * 1024 * 1024, 0, 0, 1.5f, 2.3f};
            Proc p2 = {102, "(bash)", "sleeping", 524288, 128 * 1024 * 1024, 0, 0, 0.1f, 1.2f};
            Proc p3 = {103, "(init)", "sleeping", 262144, 64 * 1024 * 1024, 0, 0, 0.0f, 0.5f};
            Proc p4 = {104, "(zombie_proc)", "zombie", 0, 0, 0, 0, 0.0f, 0.0f};
            mockProcs = {p1, p2, p3, p4};
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

                    p.name = comm;
                    
                    string stateStr = "sleeping";
                    if (state == 'R') stateStr = "running";
                    else if (state == 'Z') stateStr = "zombie";
                    else if (state == 'T' || state == 't') stateStr = "stopped";
                    p.state = stateStr;

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
