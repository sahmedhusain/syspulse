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
