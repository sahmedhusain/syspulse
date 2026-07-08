#include "header.h"

// get cpu id and information, you can use `proc/cpuinfo`
string CPUinfo()
{
#ifdef CAN_USE_CPUID
    char CPUBrandString[0x40];
    unsigned int CPUInfo[4] = {0, 0, 0, 0};

    // unix system
    // for windoes maybe we must add the following
    // __cpuid(regs, 0);
    // regs is the array of 4 positions
    __cpuid(0x80000000, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
    unsigned int nExIds = CPUInfo[0];

    memset(CPUBrandString, 0, sizeof(CPUBrandString));

    for (unsigned int i = 0x80000000; i <= nExIds; ++i)
    {
        __cpuid(i, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);

        if (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }
    string str(CPUBrandString);
    return str;
#else
    return "ARM64 CPU";
#endif
}

// getOsName, this will get the OS of the current computer
const char *getOsName()
{
#ifdef _WIN32
    return "Windows 32-bit";
#elif _WIN64
    return "Windows 64-bit";
#elif __APPLE__ || __MACH__
    return "Mac OSX";
#elif __linux__
    return "Linux";
#elif __FreeBSD__
    return "FreeBSD";
#elif __unix || __unix__
    return "Unix";
#else
    return "Other";
#endif
}

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

#ifndef LOGIN_NAME_MAX
#define LOGIN_NAME_MAX 256
#endif

string getLoggedInUser()
{
    char username[LOGIN_NAME_MAX];
    if (getlogin_r(username, sizeof(username)) == 0)
    {
        return string(username);
    }
    const char *env_user = getenv("USER");
    if (env_user)
    {
        return string(env_user);
    }
    return "Unknown";
}

string getHostName()
{
    char hostname[HOST_NAME_MAX];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        return string(hostname);
    }
    return "Unknown";
}

string getCPUModel()
{
    ifstream file("/proc/cpuinfo");
    string line;
    if (file.is_open())
    {
        while (getline(file, line))
        {
            if (line.rfind("model name", 0) == 0 || line.rfind("Model", 0) == 0)
            {
                size_t colon = line.find(':');
                if (colon != string::npos)
                {
                    string model = line.substr(colon + 1);
                    // Trim leading spaces
                    size_t first = model.find_first_not_of(" \t");
                    if (first != string::npos)
                    {
                        model = model.substr(first);
                    }
                    // Trim trailing spaces/newlines
                    size_t last = model.find_last_not_of(" \t\r\n");
                    if (last != string::npos)
                    {
                        model = model.substr(0, last + 1);
                    }
                    return model;
                }
            }
        }
    }
    // Fallback if /proc/cpuinfo is not readable/present (e.g. on macOS)
    return CPUinfo();
}

TaskCounts getTaskCounts()
{
    TaskCounts counts;
    DIR *dir = opendir("/proc");
    if (!dir)
    {
        return counts;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (entry->d_type == DT_DIR)
        {
            char *endptr;
            long pid = strtol(entry->d_name, &endptr, 10);
            if (*endptr == '\0' && pid > 0)
            {
                counts.total++;
                // Read state
                string statPath = string("/proc/") + entry->d_name + "/stat";
                ifstream statFile(statPath);
                if (statFile.is_open())
                {
                    int filePid;
                    string temp;
                    char state = '\0';

                    statFile >> filePid;
                    // Read process name in parentheses (e.g., "(bash)")
                    statFile >> temp;
                    if (!temp.empty() && temp.front() == '(')
                    {
                        while (!temp.empty() && temp.back() != ')' && statFile >> temp)
                        {
                            // Loop to skip name with spaces
                        }
                    }
                    statFile >> state;

                    switch (state)
                    {
                    case 'R':
                        counts.running++;
                        break;
                    case 'S':
                    case 'I': // Idle kernel threads
                        counts.sleeping++;
                        break;
                    case 'D':
                        counts.uninterruptible++;
                        break;
                    case 'Z':
                        counts.zombie++;
                        break;
                    case 'T':
                    case 't':
                        counts.stopped++;
                        break;
                    default:
                        counts.sleeping++;
                        break;
                    }
                }
            }
        }
    }
    closedir(dir);
    return counts;
}

float getCPUUsage()
{
    static long long prevUser = 0, prevNice = 0, prevSystem = 0, prevIdle = 0;
    static long long prevIowait = 0, prevIrq = 0, prevSoftirq = 0, prevSteal = 0;

    ifstream file("/proc/stat");
    string label;
    if (file.is_open())
    {
        file >> label;
        if (label == "cpu")
        {
            long long user, nice, system, idle, iowait, irq, softirq, steal;
            file >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

            long long prevActive = prevUser + prevNice + prevSystem + prevIrq + prevSoftirq + prevSteal;
            long long active = user + nice + system + irq + softirq + steal;

            long long prevIdleTotal = prevIdle + prevIowait;
            long long idleTotal = idle + iowait;

            long long prevTotal = prevActive + prevIdleTotal;
            long long total = active + idleTotal;

            long long diffTotal = total - prevTotal;
            long long diffIdle = idleTotal - prevIdleTotal;

            float usage = 0.0f;
            if (diffTotal > 0)
            {
                usage = (float)(diffTotal - diffIdle) / diffTotal;
            }

            prevUser = user;
            prevNice = nice;
            prevSystem = system;
            prevIdle = idle;
            prevIowait = iowait;
            prevIrq = irq;
            prevSoftirq = softirq;
            prevSteal = steal;

            return usage * 100.0f;
        }
    }
    // Fallback/Mock for macOS testing (simple random oscillation between 20% and 40%)
    static float mockUsage = 30.0f;
    mockUsage += ((rand() % 100) - 50) / 50.0f;
    if (mockUsage < 5.0f) mockUsage = 5.0f;
    if (mockUsage > 95.0f) mockUsage = 95.0f;
    return mockUsage;
}

float getTemperature()
{
    // Try /proc/acpi/ibm/thermal first
    ifstream ibmFile("/proc/acpi/ibm/thermal");
    if (ibmFile.is_open())
    {
        string label;
        ibmFile >> label;
        if (label == "temperatures:")
        {
            float temp;
            if (ibmFile >> temp)
            {
                return temp;
            }
        }
    }

    // Try /sys/class/thermal/thermal_zone0/temp
    ifstream sysFile("/sys/class/thermal/thermal_zone0/temp");
    if (sysFile.is_open())
    {
        float tempMs;
        if (sysFile >> tempMs)
        {
            return tempMs / 1000.0f;
        }
    }

    // Fallback/Mock for macOS testing (simple random oscillation between 45C and 55C)
    static float mockTemp = 50.0f;
    mockTemp += ((rand() % 100) - 50) / 100.0f;
    if (mockTemp < 30.0f) mockTemp = 30.0f;
    if (mockTemp > 90.0f) mockTemp = 90.0f;
    return mockTemp;
}

FanStats getFanStats()
{
    FanStats stats;
    ifstream file("/proc/acpi/ibm/fan");
    if (file.is_open())
    {
        string line;
        while (getline(file, line))
        {
            if (line.rfind("status:", 0) == 0)
            {
                size_t tab = line.find('\t');
                if (tab != string::npos)
                {
                    stats.status = line.substr(tab + 1);
                }
                else
                {
                    size_t colon = line.find(':');
                    if (colon != string::npos)
                    {
                        stats.status = line.substr(colon + 1);
                    }
                }
                // Trim stats.status
                size_t first = stats.status.find_first_not_of(" \t");
                if (first != string::npos) stats.status = stats.status.substr(first);
                size_t last = stats.status.find_last_not_of(" \t\r\n");
                if (last != string::npos) stats.status = stats.status.substr(0, last + 1);
            }
            else if (line.rfind("speed:", 0) == 0)
            {
                size_t tab = line.find('\t');
                if (tab != string::npos)
                {
                    stats.speed = atoi(line.substr(tab + 1).c_str());
                }
                else
                {
                    size_t colon = line.find(':');
                    if (colon != string::npos)
                    {
                        stats.speed = atoi(line.substr(colon + 1).c_str());
                    }
                }
            }
            else if (line.rfind("level:", 0) == 0)
            {
                size_t tab = line.find('\t');
                if (tab != string::npos)
                {
                    stats.level = line.substr(tab + 1);
                }
                else
                {
                    size_t colon = line.find(':');
                    if (colon != string::npos)
                    {
                        stats.level = line.substr(colon + 1);
                    }
                }
                // Trim stats.level
                size_t first = stats.level.find_first_not_of(" \t");
                if (first != string::npos) stats.level = stats.level.substr(first);
                size_t last = stats.level.find_last_not_of(" \t\r\n");
                if (last != string::npos) stats.level = stats.level.substr(0, last + 1);
            }
        }
        return stats;
    }

    // Try reading fan speed from standard sysfs if /proc/acpi/ibm/fan is not present
    ifstream sysFile("/sys/class/hwmon/hwmon0/device/fan_speed");
    if (sysFile.is_open())
    {
        sysFile >> stats.speed;
        stats.status = (stats.speed > 0) ? "active" : "inactive";
        stats.level = "auto";
        return stats;
    }

    // Fallback/Mock for macOS testing (simple random oscillation between 2000 and 4000 RPM)
    stats.status = "active";
    stats.level = "auto";
    static int mockSpeed = 3000;
    mockSpeed += (rand() % 200) - 100;
    if (mockSpeed < 1000) mockSpeed = 1000;
    if (mockSpeed > 5000) mockSpeed = 5000;
    stats.speed = mockSpeed;
    return stats;
}
