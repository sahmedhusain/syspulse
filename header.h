// To make sure you don't declare the function more than once by including the header multiple times.
#ifndef header_H
#define header_H

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <dirent.h>
#include <vector>
#include <iostream>
#include <cmath>
// lib to read from file
#include <fstream>
// for the name of the computer and the logged in user
#include <unistd.h>
#include <limits.h>
// this is for us to get the cpu information
// mostly in unix system
// not sure if it will work in windows
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <cpuid.h>
#define CAN_USE_CPUID 1
#endif
// this is for the memory usage and other memory visualization
// for linux gotta find a way for windows
#include <sys/types.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <sys/statvfs.h>
// for time and date
#include <ctime>
// ifconfig ip addresses
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <set>
#include <algorithm>

using namespace std;

struct CPUStats
{
    long long int user;
    long long int nice;
    long long int system;
    long long int idle;
    long long int iowait;
    long long int irq;
    long long int softirq;
    long long int steal;
    long long int guest;
    long long int guestNice;
};

// processes `stat`
struct Proc
{
    int pid;
    string name;
    char state;
    long long int vsize;
    long long int rss;
    long long int utime;
    long long int stime;
    float cpuUsage = 0.0f;
    float memUsage = 0.0f;
};

struct IP4
{
    string name;
    char addressBuffer[INET_ADDRSTRLEN];
};

struct Networks
{
    vector<IP4> ip4s;
};

struct TX
{
    int bytes;
    int packets;
    int errs;
    int drop;
    int fifo;
    int frame;
    int compressed;
    int multicast;
};

struct RX
{
    int bytes;
    int packets;
    int errs;
    int drop;
    int fifo;
    int colls;
    int carrier;
    int compressed;
};

struct TaskCounts
{
    int total = 0;
    int running = 0;
    int sleeping = 0;
    int uninterruptible = 0;
    int zombie = 0;
    int stopped = 0;
};

struct FanStats
{
    string status = "disabled";
    int speed = 0;
    string level = "0";
};

// student TODO : system stats
string CPUinfo();
const char *getOsName();
string getLoggedInUser();
string getHostName();
string getCPUModel();
TaskCounts getTaskCounts();
float getCPUUsage();
float getTemperature();
FanStats getFanStats();

// student TODO : memory and processes
struct MemoryStats
{
    long long ramTotal = 0;  // in bytes
    long long ramUsed = 0;   // in bytes
    long long swapTotal = 0; // in bytes
    long long swapUsed = 0;  // in bytes
};

MemoryStats getMemoryStats();

struct DiskStats
{
    long long totalBytes = 0;
    long long usedBytes = 0;
};

DiskStats getDiskStats();

vector<Proc> getProcesses(long long ramTotal);

// student TODO : network
Networks getNetworkInterfaces();
map<string, pair<TX, RX>> getNetworkStats();
string formatBytes(long long bytes);

#endif
