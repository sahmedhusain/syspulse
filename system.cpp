#include "header.h"

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <libproc.h>
#include <IOKit/IOKitLib.h>

struct SMCVal {
    char key[5];
    uint32_t dataSize;
    char dataType[5];
    uint8_t bytes[32];
};

struct SMCKeyData_vers_t {
    uint8_t major;
    uint8_t minor;
    uint8_t build;
    uint8_t reserved[9];
};

struct SMCKeyData_pLimitData_t {
    uint16_t version;
    uint16_t length;
    uint32_t cpuPLimit;
    uint32_t gpuPLimit;
    uint32_t memPLimit;
};

struct SMCKeyData_keyInfo_t {
    uint32_t dataSize;
    uint32_t dataType;
    uint8_t  dataAttributes;
};

struct SMCKeyData_t {
    uint32_t key;
    SMCKeyData_vers_t vers;
    SMCKeyData_pLimitData_t pLimitData;
    SMCKeyData_keyInfo_t keyInfo;
    uint8_t result;
    uint8_t status;
    uint8_t data8;
    uint32_t data32;
    uint8_t bytes[32];
};

#define KERNEL_INDEX_SMC 2
#define CONN_TYPE_SMC 0

static uint32_t getSMCKey(const char *keyStr)
{
    uint32_t key = 0;
    for (int i = 0; i < 4; ++i)
    {
        key = (key << 8) | (unsigned char)keyStr[i];
    }
    return key;
}

static io_connect_t openSMC()
{
    CFMutableDictionaryRef matchingDict = IOServiceMatching("AppleSMC");
    if (!matchingDict)
        return 0;

    io_iterator_t iterator;
    if (IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iterator) != kIOReturnSuccess)
        return 0;

    io_object_t device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0)
        return 0;

    io_connect_t conn = 0;
    kern_return_t kr = IOServiceOpen(device, mach_task_self(), CONN_TYPE_SMC, &conn);
    IOObjectRelease(device);
    if (kr != kIOReturnSuccess)
        return 0;

    return conn;
}

static void closeSMC(io_connect_t conn)
{
    IOServiceClose(conn);
}

static kern_return_t SMCCall(io_connect_t conn, int cmd, SMCKeyData_t *input, SMCKeyData_t *output)
{
    size_t inSize = sizeof(SMCKeyData_t);
    size_t outSize = sizeof(SMCKeyData_t);
    return IOConnectCallStructMethod(conn, KERNEL_INDEX_SMC, input, inSize, output, &outSize);
}

static bool SMCGetKeyInfo(io_connect_t conn, uint32_t key, SMCKeyData_keyInfo_t *info)
{
    SMCKeyData_t input;
    memset(&input, 0, sizeof(input));
    input.key = key;
    input.data8 = 9; // Get Key Info

    SMCKeyData_t output;
    memset(&output, 0, sizeof(output));

    if (SMCCall(conn, KERNEL_INDEX_SMC, &input, &output) == kIOReturnSuccess)
    {
        *info = output.keyInfo;
        return output.result == 0;
    }
    return false;
}

static bool SMCReadValue(io_connect_t conn, const char *keyStr, SMCVal *val)
{
    uint32_t key = getSMCKey(keyStr);

    SMCKeyData_keyInfo_t info;
    if (!SMCGetKeyInfo(conn, key, &info))
        return false;

    SMCKeyData_t input;
    memset(&input, 0, sizeof(input));
    input.key = key;
    input.keyInfo.dataSize = info.dataSize;
    input.data8 = 5; // Read Key Value

    SMCKeyData_t output;
    memset(&output, 0, sizeof(output));

    if (SMCCall(conn, KERNEL_INDEX_SMC, &input, &output) == kIOReturnSuccess)
    {
        if (output.result == 0)
        {
            strcpy(val->key, keyStr);
            val->dataSize = info.dataSize;

            uint32_t type = info.dataType;
            val->dataType[0] = (type >> 24) & 0xff;
            val->dataType[1] = (type >> 16) & 0xff;
            val->dataType[2] = (type >> 8) & 0xff;
            val->dataType[3] = type & 0xff;
            val->dataType[4] = '\0';
            memcpy(val->bytes, output.bytes, info.dataSize);
            return true;
        }
    }
    return false;
}
#endif

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
#ifdef __APPLE__
    char model[256];
    size_t size = sizeof(model);
    if (sysctlbyname("machdep.cpu.brand_string", model, &size, NULL, 0) == 0)
    {
        return string(model);
    }
#endif
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
#ifdef __APPLE__
    int num_procs = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (num_procs > 0)
    {
        counts.total = num_procs / sizeof(pid_t);
        counts.running = 2; // typical macOS estimate
        counts.sleeping = counts.total - counts.running;
    }
    return counts;
#endif

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
#ifdef __APPLE__
    static long long prevUser = 0, prevSystem = 0, prevIdle = 0, prevNice = 0;

    host_cpu_load_info_data_t cpu_load;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpu_load, &count) == KERN_SUCCESS)
    {
        long long user = cpu_load.cpu_ticks[CPU_STATE_USER];
        long long system = cpu_load.cpu_ticks[CPU_STATE_SYSTEM];
        long long idle = cpu_load.cpu_ticks[CPU_STATE_IDLE];
        long long nice = cpu_load.cpu_ticks[CPU_STATE_NICE];

        long long total = user + system + idle + nice;
        long long prevTotal = prevUser + prevSystem + prevIdle + prevNice;

        long long diffTotal = total - prevTotal;
        long long diffIdle = idle - prevIdle;

        float usage = 0.0f;
        if (diffTotal > 0)
        {
            usage = (float)(diffTotal - diffIdle) / diffTotal;
        }

        prevUser = user;
        prevSystem = system;
        prevIdle = idle;
        prevNice = nice;

        return usage * 100.0f;
    }
    return 0.0f;
#else
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
    // Fallback/Mock for non-macOS/non-Linux testing
    static float mockUsage = 30.0f;
    mockUsage += ((rand() % 100) - 50) / 50.0f;
    if (mockUsage < 5.0f) mockUsage = 5.0f;
    if (mockUsage > 95.0f) mockUsage = 95.0f;
    return mockUsage;
#endif
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

    // Fallback/Mock for macOS testing (handles fanless designs like MacBook Air)
#ifdef __APPLE__
    io_connect_t conn = openSMC();
    if (conn != 0)
    {
        SMCVal val;
        int fanCount = 0;
        if (SMCReadValue(conn, "FNum", &val))
        {
            fanCount = val.bytes[0];
        }

        if (fanCount <= 0)
        {
            stats.status = "N/A (Fanless)";
            stats.level = "N/A";
            stats.speed = 0;
        }
        else
        {
            stats.status = "active";
            stats.level = "auto";
            if (SMCReadValue(conn, "F0Ac", &val))
            {
                stats.speed = (int)(((float)(((int)val.bytes[0] << 8) | val.bytes[1]) / 4.0f) + 0.5f);
            }
            else
            {
                stats.speed = 0;
            }
        }
        closeSMC(conn);
        return stats;
    }
#endif

    stats.status = "active";
    stats.level = "auto";
    static int mockSpeed = 3000;
    mockSpeed += (rand() % 200) - 100;
    if (mockSpeed < 1000) mockSpeed = 1000;
    if (mockSpeed > 5000) mockSpeed = 5000;
    stats.speed = mockSpeed;
    return stats;
}
