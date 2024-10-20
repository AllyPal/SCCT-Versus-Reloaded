#include "pch.h"
#include "Networking.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <format>
#include <set>
#include <timeapi.h>
#include <Windows.h>
#include <chrono>
#include "StringOperations.h"
#include "MemoryWriter.h"
#include "Input.h"
#include "GameStructs.h"
#include "Config.h"
#include "Graphics.h"
#include "Engine.h"

static std::pair<uint32_t, uint16_t> GetCachedDnsIp(const std::string& dnsName);
static void CacheDnsIp(const std::string& dnsName);

static std::vector<std::string> ipPortList;

static std::pair<uint32_t, uint16_t> GetOrCacheDnsIpThreaded(const std::string& dnsName) {
    auto ip = GetCachedDnsIp(dnsName);
    if (ip.first == 0 || ip.second == 0) {
        std::thread cacheThread(CacheDnsIp, dnsName);
        cacheThread.detach();
    }
    return ip;
}

std::string convertToIpPortString(const uint32_t* ip, const uint16_t* port);
//typedef wchar_t* (__cdecl* GetFriendlyErrorSig)(int Error);
//
//wchar_t* GetFriendlyError(int Error) {
//
//    void* functionAddress = reinterpret_cast<void*>(0x10A85BB0);
//    // Cast the address to the function pointer type
//    GetFriendlyErrorSig func = reinterpret_cast<GetFriendlyErrorSig>(functionAddress);
//
//    // Call the function using the function pointer
//    return func(Error);
//}

//_declspec(naked) void ToggleBroadcast() {
//    const char optVal = 1;
//    setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &optVal, 4);
//}

int SendPacket(sockaddr_in& to, uintptr_t messagePtr, SOCKET _socket, int messageLength)
{
    char ipAddress[20];
    inet_ntop(AF_INET, &to.sin_addr, ipAddress, sizeof(ipAddress));

   /* std::cout << "Preparing to send message:" << std::endl;
    std::cout << "  sin_family: " << to.sin_family << std::endl;
    std::cout << "  sin_port: " << std::dec << ntohs(to.sin_port) << std::endl;
    std::cout << "  sin_addr.s_addr: " << ipAddress << std::endl;*/

    auto message = reinterpret_cast<char*>(messagePtr);
    int result = sendto(_socket, message, messageLength, 0, (struct sockaddr*)&to, sizeof(to));

    if (result == SOCKET_ERROR) {
        std::cerr << "sendto failed with error: " << WSAGetLastError() << std::endl;
    }
    else {
        //std::cout << "Message sent successfully. Bytes sent: " << result << std::endl;
    }

    return result;
}

static std::chrono::steady_clock::time_point nextServerListUpdateTime;

int result;
int sendMessage(SOCKET _socket, u_short hostshort, u_long hostlong, uintptr_t messagePtr, int messageLength) {
    struct sockaddr_in to;
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;

    // Send broadcast packet
    to.sin_port = htons(hostshort);
    to.sin_addr.s_addr = htonl(hostlong);
    int result = SendPacket(to, messagePtr, _socket, messageLength);

    // TODO: retire
    if (Config::useDirectConnect) {
#pragma warning(push)
#pragma warning(disable: 4996)
        to.sin_addr.s_addr = inet_addr(StringOperations::wStringToString(Config::directConnectIp).c_str());
#pragma warning(pop)
        to.sin_port = htons(hostshort);
        auto directConnectResult = SendPacket(to, messagePtr, _socket, messageLength);
        if (directConnectResult != SOCKET_ERROR) {
            result = directConnectResult;
        }
    }

    std::vector<std::string> combinedServerList;

    static auto masterIpPort = GetOrCacheDnsIpThreaded(Config::master_server_dns);
    if (masterIpPort.first != 0 && masterIpPort.second != 0 && nextServerListUpdateTime < Graphics::lastFrameTime) {
        nextServerListUpdateTime = Graphics::lastFrameTime + std::chrono::seconds(4);
        // very hacky swapping of ip representation - done for speed
        uint32_t reversedIp = ntohl(masterIpPort.first);
        uint16_t reversedPort = ntohs(masterIpPort.second);
        auto masterServerIpString = convertToIpPortString(&reversedIp, &reversedPort);
        combinedServerList.push_back(masterServerIpString);

        std::cout << "Requesting server list" << std::endl;
    }
    else {
        std::cout << "Not requesting server list" << std::endl;
    }

    combinedServerList.insert(combinedServerList.end(), ipPortList.begin(), ipPortList.end());
    combinedServerList.insert(combinedServerList.end(), Config::server_list.begin(), Config::server_list.end());

    for (const auto& ipPortStr : combinedServerList) {
        size_t colonPos = ipPortStr.find(':');
        if (colonPos == std::string::npos) {
            std::cerr << "Invalid IP:PORT format: " << ipPortStr << std::endl;
            continue;
        }

        std::string ip = ipPortStr.substr(0, colonPos);
        std::string portStr = ipPortStr.substr(colonPos + 1);

        unsigned short port = static_cast<unsigned short>(std::stoi(portStr));

        sockaddr_in to = {};
        to.sin_family = AF_INET;

        if (inet_pton(AF_INET, ip.c_str(), &to.sin_addr) != 1) {
            std::cerr << "Invalid IP address format: " << ip << std::endl;
            continue;
        }

        to.sin_port = htons(port);

        int slResult = SendPacket(to, messagePtr, _socket, messageLength);
        if (slResult == SOCKET_ERROR) {
            std::cerr << "Failed to send packet to " << ipPortStr << std::endl;
        }
        else {
            result = slResult;
        }
    }

    return result;
}

static int sendBroadcastLanMessageEntry = 0x10AB3911;
__declspec(naked) void sendBroadcastLanMessage() {
    static SOCKET _socket;
    static u_short hostshort;
    static u_long hostlong;
    static uintptr_t messagePtr;
    __asm {
        lea     edx, [esp + 0x24]
        mov     dword ptr[messagePtr], edx
        mov     dx, [edi + 0x300]
        mov     word ptr[hostshort], dx
        mov     edx, dword ptr[edi + 0x2FC]
        mov     dword ptr[_socket], edx
        mov     edx, [edi + 0x314]
        mov     dword ptr[hostlong], edx
    }

    result = sendMessage(_socket, hostshort, hostlong, messagePtr, 40);

    static int sendBroadcastLanMessageReturn = 0x10AB3953;
    __asm {
        mov     eax, dword ptr[result]
        jmp     dword ptr[sendBroadcastLanMessageReturn]
    }
}

int ServerListHostshortFixEntry = 0x10A7FD70;
__declspec(naked) void ServerListHostshortFix() {
    static int Return = 0x10A7FD77;
    __asm {
        mov ax, [esp+0x20]
        jmp dword ptr[Return]
    }
}

int ServerListHostlongFixEntry = 0x10AB3E35;
__declspec(naked) void ServerListHostlongFix() {
    //if (!Engine::configRef.useDirectConnect) {
        /*__asm {
            mov[esp + 0x20], ecx
        }*/
        //}

    static int ServerInfoBroadcastReturn = 0x10AB3E3B;
    __asm {
        call edi
        jmp dword ptr[ServerInfoBroadcastReturn]
    }
}

static std::unordered_map<std::string, std::pair<uint32_t, uint16_t>> dnsCache;

static std::pair<uint32_t, uint16_t> GetCachedDnsIp(const std::string& dnsName) {
    if (dnsCache.find(dnsName) != dnsCache.end()) {
        return dnsCache[dnsName];
    }
    return { 0, 0 };
}

static void CacheDnsIp(const std::string& dnsName) {
    std::cout << "Caching dns: " << dnsName << std::endl;
    size_t colonPos = dnsName.find(':');

    if (colonPos == std::string::npos) {
        return;
    }

    std::string hostname = dnsName.substr(0, colonPos);
    int port = std::stoi(dnsName.substr(colonPos + 1));

    struct addrinfo hints, *res;
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
    if (status != 0) {
        std::wcerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;     
        return;
    }

    struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, sizeof ipStr);

    // Replace inet_addr with inet_pton
    uint32_t ipAddr;
    if (inet_pton(AF_INET, ipStr, &ipAddr) != 1) {
        freeaddrinfo(res);
        return;
    }
    freeaddrinfo(res);

    dnsCache[dnsName] = { ntohl(ipAddr), port };
    std::cout << "IP cached fo " << dnsName << std::endl;
}

std::string convertToIpPortString(const uint32_t* ip, const uint16_t* port) {
    char ip_str[INET_ADDRSTRLEN];  // Buffer for storing the string representation of the IP address

    // Convert IP from binary to string using inet_ntop (IPv4)
    struct in_addr ip_addr;
    ip_addr.s_addr = *ip;
    inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN);

    // Convert port from network byte order to host byte order
    uint16_t port_host_order = ntohs(*port);

    // Create the "IP:PORT" string
    std::ostringstream oss;
    oss << ip_str << ":" << port_host_order;

    return oss.str();
}

void handleListPacket(std::wstring packetId, uint8_t* buffer, uint32_t recvBytes) {
    const size_t ipPortSize = 6;
    std::cout << "Server list packet: " << StringOperations::wStringToString(packetId) << std::endl;
    const int ipDataOffset = 24;
    if (ipDataOffset >= recvBytes) {
        std::cout << "Server list is empty" << std::endl;
        return;
    }

    if ((recvBytes - ipDataOffset) % ipPortSize != 0) {
        std::cout << "Invalid payload" << std::endl;
        return;
    }

    ipPortList.clear();

    // Process the buffer
    for (size_t i = 0; i < recvBytes - ipDataOffset; i += ipPortSize) {
        uint32_t* ip = reinterpret_cast<uint32_t*>(&buffer[ipDataOffset + i]);
        uint16_t* port = reinterpret_cast<uint16_t*>(&buffer[ipDataOffset + i + 4]);

        // Convert IP and port to "IP:PORT" string
        std::string ipPortStr = convertToIpPortString(ip, port);
        std::cout << ipPortStr << std::endl;
        // Add the string to the vector
        ipPortList.push_back(ipPortStr);
}
}

void handleOther(std::wstring packetId, uint8_t* buffer) {
    std::cout << "Unknown master server packet: " << StringOperations::wStringToString(packetId) << std::endl;
}

void handlePacket(uint32_t clientIP, uint16_t clientPort, uint32_t recvBytes, uint8_t* buffer) {
    //struct in_addr ipAddr;
    //ipAddr.s_addr = htonl(clientIP);

    //char ipStr[INET_ADDRSTRLEN];
    //inet_ntop(AF_INET, &ipAddr, ipStr, INET_ADDRSTRLEN);

    /*std::cout << "From IP: " << ipStr << ":" << clientPort << " Bytes: " << recvBytes << std::endl;
    std::wstring localWString(buffer, recvBytes/2);
    std::cout << StringOperations::wStringToString(localWString) << std::endl;*/

    static auto masterIpPort = GetOrCacheDnsIpThreaded(Config::master_server_dns);
    if (clientIP == masterIpPort.first && clientPort == masterIpPort.second && recvBytes >= 24) {
        std::wstring packetId(reinterpret_cast<const wchar_t*>(buffer), 8);

        static std::unordered_map<std::wstring, std::function<void(std::wstring, uint8_t*, uint32_t)>> handlers = {
            {L"SCLILIST", handleListPacket},
        };

        auto it = handlers.find(packetId);
        if (it != handlers.end()) {
            it->second(packetId, buffer, recvBytes);
        }
        else {
            handleOther(packetId, buffer);
        }
    }
}

int InterceptMasterServerPacketEntry = 0x10A80004;
__declspec(naked) void InterceptMasterServerPacket() {
    static uint16_t clientPort;
    static uint32_t clientIP;
    static uint32_t recvBytes;
    static uint8_t* buffer;
    __asm {
        movzx   ebx, ax
        mov [clientPort], ax
        mov [clientIP], ebp
        mov [recvBytes], esi

        // restore eax later
        lea eax, [esp + 0x140]
        mov [buffer], eax
        add eax, esi
        mov byte ptr[eax], 0
        pushad
    }
    handlePacket(clientIP, clientPort, recvBytes, buffer);

    static int Return = 0x10A8000D;
    __asm {
        popad
        mov     al, [edi + 0x2F8]
        jmp dword ptr[Return]
    }
}

static std::chrono::steady_clock::time_point nextNetUpdateTime;
static void LongIntervalNetcode() {
    if (nextNetUpdateTime < Graphics::lastFrameTime) {
        static auto masterIpPort = GetOrCacheDnsIpThreaded(Config::master_server_dns);
        if (masterIpPort.first != 0 && masterIpPort.second != 0) {            
            SOCKET socket = Engine::gameState.lvIn->sGaIn()->IamLANServer()->Socket();

            const wchar_t* message = L"SCLIREGI";
            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_addr.s_addr = ntohl(masterIpPort.first);
            serverAddr.sin_port = ntohs(masterIpPort.second);

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(serverAddr.sin_addr), ip, INET_ADDRSTRLEN);
            uint16_t port = ntohs(serverAddr.sin_port);
            std::cout << "Sending master server packet to " << ip << ":" << port << std::endl;

            int result = sendto(socket, (const char*)message, wcslen(message) * sizeof(wchar_t), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
            if (result == SOCKET_ERROR) {
                std::cerr << "sendto failed with error: " << WSAGetLastError() << std::endl;
            }
            else {
                std::cout << "Packet sent successfully!" << std::endl;
            }
        }

        nextNetUpdateTime = Graphics::lastFrameTime + std::chrono::seconds(60);
    }
}

static void OnLanFrame() {
    LongIntervalNetcode();
}

static int RunOncePerServerLanFrameEntry = 0x10BF8D48;
__declspec(naked) void RunOncePerServerLanFrame() {
    static int Return = 0x10A29470;
    __asm {
        pushad
    }
    OnLanFrame();
    __asm {
        popad
        jmp dword ptr[Return]
    }
}

void CacheMasterServerIpStartup() {
    std::cout << "Caching master server ip" << std::endl;
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return; 
    }
    CacheDnsIp(Config::master_server_dns);
    WSACleanup();
}

void Networking::Initialize()
{
    MemoryWriter::WriteJump(sendBroadcastLanMessageEntry, sendBroadcastLanMessage);
    MemoryWriter::WriteJump(ServerListHostlongFixEntry, ServerListHostlongFix);
    MemoryWriter::WriteJump(ServerListHostshortFixEntry, ServerListHostshortFix);
    MemoryWriter::WriteJump(InterceptMasterServerPacketEntry, InterceptMasterServerPacket);
    MemoryWriter::WriteFunctionPtr(RunOncePerServerLanFrameEntry, RunOncePerServerLanFrame);
    std::thread cacheThread(CacheMasterServerIpStartup);
    cacheThread.detach();
}