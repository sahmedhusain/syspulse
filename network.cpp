#include "header.h"

#ifdef __APPLE__
#include <net/if.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#endif

Networks getNetworkInterfaces()
{
    Networks nets;

#ifdef _WIN32
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    ULONG outBufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
    if (GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &outBufLen) == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses; pCurrAddresses = pCurrAddresses->Next) {
            for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast; pUnicast = pUnicast->Next) {
                if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                    IP4 ip;
                    char buffer[256];
                    wcstombs(buffer, pCurrAddresses->FriendlyName, 256);
                    ip.name = buffer;

                    struct sockaddr_in *sa = (struct sockaddr_in *)pUnicast->Address.lpSockaddr;
                    inet_ntop(AF_INET, &(sa->sin_addr), ip.addressBuffer, INET_ADDRSTRLEN);
                    nets.ip4s.push_back(ip);
                }
            }
        }
    }
    if (pAddresses) free(pAddresses);
    return nets;
#endif

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

#include <sstream>

map<string, pair<TX, RX>> getNetworkStats()
{
    map<string, pair<TX, RX>> statsMap;

#ifdef _WIN32
    PMIB_IF_TABLE2 pIfTable;
    if (GetIfTable2(&pIfTable) == NO_ERROR) {
        for (ULONG i = 0; i < pIfTable->NumEntries; i++) {
            char name[256];
            wcstombs(name, pIfTable->Table[i].Alias, 256);

            TX rxData;
            rxData.bytes = pIfTable->Table[i].InOctets;
            rxData.packets = pIfTable->Table[i].InUcastPkts + pIfTable->Table[i].InNUcastPkts;
            rxData.errs = pIfTable->Table[i].InErrors;
            rxData.drop = pIfTable->Table[i].InDiscards;
            rxData.fifo = 0; rxData.frame = 0; rxData.compressed = 0; rxData.multicast = pIfTable->Table[i].InNUcastPkts;

            RX txData;
            txData.bytes = pIfTable->Table[i].OutOctets;
            txData.packets = pIfTable->Table[i].OutUcastPkts + pIfTable->Table[i].OutNUcastPkts;
            txData.errs = pIfTable->Table[i].OutErrors;
            txData.drop = pIfTable->Table[i].OutDiscards;
            txData.fifo = 0; txData.colls = 0; txData.carrier = 0; txData.compressed = 0;

            statsMap[name] = {rxData, txData};
        }
        FreeMibTable(pIfTable);
    }
    return statsMap;
#endif

#ifdef __APPLE__
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) != -1)
    {
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr != nullptr && ifa->ifa_addr->sa_family == AF_LINK)
            {
                struct if_data *ifd = (struct if_data *)ifa->ifa_data;
                if (ifd != nullptr)
                {
                    TX rxData;
                    rxData.bytes = ifd->ifi_ibytes;
                    rxData.packets = ifd->ifi_ipackets;
                    rxData.errs = ifd->ifi_ierrors;
                    rxData.drop = ifd->ifi_imcasts;
                    rxData.fifo = 0;
                    rxData.frame = 0;
                    rxData.compressed = 0;
                    rxData.multicast = ifd->ifi_imcasts;

                    RX txData;
                    txData.bytes = ifd->ifi_obytes;
                    txData.packets = ifd->ifi_opackets;
                    txData.errs = ifd->ifi_oerrors;
                    txData.drop = 0;
                    txData.fifo = 0;
                    txData.colls = ifd->ifi_collisions;
                    txData.carrier = 0;
                    txData.compressed = 0;

                    statsMap[ifa->ifa_name] = {rxData, txData};
                }
            }
        }
        freeifaddrs(ifaddr);
        return statsMap;
    }
#endif

    ifstream file("/proc/net/dev");
    if (file.is_open())
    {
        string line;
        // Skip first two header lines
        getline(file, line);
        getline(file, line);

        while (getline(file, line))
        {
            size_t colon = line.find(':');
            if (colon != string::npos)
            {
                string name = line.substr(0, colon);
                // Trim name
                size_t first = name.find_first_not_of(" \t");
                if (first != string::npos) name = name.substr(first);
                size_t last = name.find_last_not_of(" \t\r\n");
                if (last != string::npos) name = name.substr(0, last + 1);

                string data = line.substr(colon + 1);
                stringstream ss(data);

                TX rxData;
                RX txData;

                // Receive (mapped to TX struct)
                ss >> rxData.bytes >> rxData.packets >> rxData.errs >> rxData.drop
                   >> rxData.fifo >> rxData.frame >> rxData.compressed >> rxData.multicast;

                // Transmit (mapped to RX struct)
                ss >> txData.bytes >> txData.packets >> txData.errs >> txData.drop
                   >> txData.fifo >> txData.colls >> txData.carrier >> txData.compressed;

                statsMap[name] = {rxData, txData};
            }
        }
        return statsMap;
    }

    // Fallback/Mock for non-macOS/non-Linux testing
    TX mockRx = {452755738, 451234, 0, 2, 0, 0, 0, 4};
    RX mockTx = {23412344, 210456, 0, 0, 0, 0, 0, 0};
    statsMap["wlp5s0"] = {mockRx, mockTx};

    TX mockRxLo = {123456, 1200, 0, 0, 0, 0, 0, 0};
    RX mockTxLo = {123456, 1200, 0, 0, 0, 0, 0, 0};
    statsMap["lo"] = {mockRxLo, mockTxLo};

    return statsMap;
}

string formatBytes(long long bytes)
{
    char buffer[64];
    double kb = 1024.0;
    double mb = 1024.0 * 1024.0;
    double gb = 1024.0 * 1024.0 * 1024.0;

    if (bytes >= gb)
    {
        snprintf(buffer, sizeof(buffer), "%.2f GB", bytes / gb);
    }
    else if (bytes >= mb)
    {
        snprintf(buffer, sizeof(buffer), "%.2f MB", bytes / mb);
    }
    else if (bytes >= kb)
    {
        snprintf(buffer, sizeof(buffer), "%.2f KB", bytes / kb);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "%lld B", bytes);
    }
    return string(buffer);
}
