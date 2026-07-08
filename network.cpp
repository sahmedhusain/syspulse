#include "header.h"

Networks getNetworkInterfaces()
{
    Networks nets;
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1)
    {
        return nets;
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr)
            continue;

        // We only care about IPv4 interfaces
        if (ifa->ifa_addr->sa_family == AF_INET)
        {
            IP4 ip;
            ip.name = ifa->ifa_name;

            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &(sa->sin_addr), ip.addressBuffer, INET_ADDRSTRLEN);

            nets.ip4s.push_back(ip);
        }
    }

    freeifaddrs(ifaddr);
    return nets;
}
