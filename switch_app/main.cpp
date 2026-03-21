#include <switch.h>
#include <switch/services/nifm.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <vector>
#include <string>
#include <errno.h>
#include "../switch_sysmodule/include/ConfigManager.h"
#include "../switch_sysmodule/include/SysmoduleConstants.h"
#include "json.hpp"

using json = nlohmann::json;

#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
#endif

#define REPO_NAME "FaserF/ha-NintendoSwitchCFW"
#define REPO_URL "https://github.com/" REPO_NAME

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

// Global state
std::string latest_date = "";
bool g_dev_mode = false;
std::vector<std::string> g_app_logs;

void add_app_log(const std::string& msg) {
    if (g_app_logs.size() > 15) g_app_logs.erase(g_app_logs.begin());
    g_app_logs.push_back(msg);

    if (g_dev_mode) {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) {
            int opt = 1;
            setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt));
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(2828);
            addr.sin_addr.s_addr = INADDR_BROADCAST;
            sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr*)&addr, sizeof(addr));
            close(sock);
        }
    }
}

std::string get_latest_version(std::string& date) {
    CURL *curl_handle;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = (char*)malloc(1);
    chunk.size = 0;

    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, "https://api.github.com/repos/" REPO_NAME "/releases/latest");
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 5L); 

    res = curl_easy_perform(curl_handle);
    std::string version = "";
    if (res == CURLE_OK) {
        json j = json::parse(chunk.memory, nullptr, false);
        if (!j.is_discarded() && j.contains("tag_name")) {
            version = j.value("tag_name", "");
            date = j.value("published_at", "");
            if (!date.empty() && date.length() > 10) date = date.substr(0, 10); 
            if (!version.empty() && version[0] == 'v') version = version.substr(1);
        } else {
            std::string errMsg = j.value("message", "API Limit or Missing Release");
            char errLog[256];
            snprintf(errLog, sizeof(errLog), "\x1b[31m[ERROR] Update check: %s\x1b[0m", errMsg.substr(0, 50).c_str());
            add_app_log(errLog);
            version = "none";
        }
    } else {
        char err[128];
        snprintf(err, sizeof(err), "\x1b[31m[ERROR] Update fetch failed: %s\x1b[0m", curl_easy_strerror(res));
        add_app_log(err);
        version = "error";
    }

    curl_easy_cleanup(curl_handle);
    free(chunk.memory);
    return version;
}

bool download_file(const std::string& url, const std::string& path) {
    CURL *curl = curl_easy_init();
    if (!curl) return false;
    FILE *fp = fopen(path.c_str(), "wb");
    if (!fp) { curl_easy_cleanup(curl); return false; }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(fp);
    return (res == CURLE_OK);
}

void make_dirs(std::string path) {
    size_t pos = 0;
    while ((pos = path.find('/', pos + 1)) != std::string::npos) {
        mkdir(path.substr(0, pos).c_str(), 0777);
    }
    mkdir(path.c_str(), 0777);
}

bool file_exists(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

void check_and_fix_sysmodule() {
    const char* path = "sdmc:/atmosphere/contents/010000000000CAFE/exefs/main";
    if (!file_exists(path)) {
        add_app_log("\x1b[33m[WARN] Sysmodule missing! Auto-safeguard active...\x1b[0m");
        std::string date;
        std::string version = get_latest_version(date);
        if (version != "error" && version != "none") {
            make_dirs("sdmc:/atmosphere/contents/010000000000CAFE/exefs");
            std::string url = std::string(REPO_URL) + "/releases/download/v" + version + "/" + NSO_FILENAME;
            if (download_file(url, path)) add_app_log("\x1b[32m[SUCCESS] Sysmodule restored. Restart required.\x1b[0m");
        }
    }
}

bool download_update(const std::string& version) {
    bool nro_ok = download_file(REPO_URL "/releases/download/v" + version + "/" + NRO_FILENAME, "sdmc:/switch/" NRO_FILENAME);
    bool nso_ok = download_file(REPO_URL "/releases/download/v" + version + "/" + NSO_FILENAME, "sdmc:/atmosphere/contents/010000000000CAFE/exefs/main");
    return nro_ok && nso_ok;
}

std::string get_sysmodule_ip() {
    u32 cur_ip = 0;
    if (R_SUCCEEDED(nifmGetCurrentIpAddress(&cur_ip)) && cur_ip != 0) {
        struct in_addr addr; addr.s_addr = cur_ip;
        return std::string(inet_ntoa(addr));
    }
    return "127.0.0.1";
}

bool try_connect(const std::string& ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    int res = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (res < 0 && errno == EINPROGRESS) {
        struct timeval tv = {2, 0}; // 2s timeout
        fd_set myset; FD_ZERO(&myset); FD_SET(sock, &myset);
        if (select(sock + 1, NULL, &myset, NULL, &tv) > 0) {
            int v; socklen_t l = sizeof(int);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)(&v), &l);
            res = (v == 0) ? 0 : -1;
        } else res = -1;
    }

    fcntl(sock, F_SETFL, flags);
    close(sock);
    return (res == 0);
}

bool is_sysmodule_running() {
    int port = ConfigManager::getInstance().getPort();
    std::string ip = get_sysmodule_ip();
    if (try_connect(ip, port)) return true;
    if (ip != "127.0.0.1" && try_connect("127.0.0.1", port)) return true;
    return false;
}

void fetch_sysmodule_logs() {
    struct MemoryStruct chunk = {(char*)malloc(1), 0};
    CURL *curl = curl_easy_init();
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/logs", get_sysmodule_ip().c_str(), ConfigManager::getInstance().getPort());
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
    if (curl_easy_perform(curl) == CURLE_OK) {
        json j = json::parse(chunk.memory, nullptr, false);
        if (!j.is_discarded() && j.is_array()) {
            g_app_logs.clear();
            for (auto& item : j) {
                std::string lvl = item.value("level", "INFO");
                std::string msg = item.value("message", "");
                const char* clr = (lvl == "ERROR") ? "\x1b[31m" : (lvl == "WARN" ? "\x1b[33m" : "\x1b[36m");
                char entry[256]; snprintf(entry, sizeof(entry), "%s[%s] %s\x1b[0m", clr, lvl.c_str(), msg.c_str());
                g_app_logs.push_back(entry);
            }
        }
    }
    curl_easy_cleanup(curl); free(chunk.memory);
}

void fetch_offline_boot_logs() {
    FILE *f = fopen("sdmc:/ha_sysmodule_boot.log", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            char entry[300];
            snprintf(entry, sizeof(entry), "\x1b[35m[BOOT] %s\x1b[0m", line);
            add_app_log(entry);
        }
        fclose(f);
        remove("sdmc:/ha_sysmodule_boot.log");
    }
}

void draw_ui(const std::string& latest_ver, bool checking_update, bool sysmodule_active, u64 loop_count) {
    printf("\x1b[H"); // Home
    printf("\x1b[36m┌──────────────────────────────────────────────────────────────────────────┐\x1b[0m\x1b[K\n");
    printf("\x1b[36m│\x1b[0m\x1b[1;37m        HOME ASSISTANT SWITCH v%-10s                           \x1b[0m\x1b[36m│\x1b[0m\x1b[K\n", APP_VERSION);
    printf("\x1b[36m├───────────────────────────────┬──────────────────────────────────────────┤\x1b[0m\x1b[K\n");
    
    // Left Pane: System Status
    printf("\x1b[36m│\x1b[0m \x1b[1;34m[ SYSTEM STATUS ]\x1b[0m             \x1b[36m│\x1b[0m \x1b[1;34m[ UPDATES & CONFIG ]\x1b[0m                   \x1b[36m│\x1b[0m\x1b[K\n");
    
    printf("\x1b[36m│\x1b[0m Status:   %-18s \x1b[36m│\x1b[0m ", sysmodule_active ? "\x1b[1;32mACTIVE\x1b[0m" : "\x1b[1;31mINACTIVE\x1b[0m");
    if (checking_update) printf("\x1b[5;33mChecking for updates...\x1b[0m              ");
    else if (latest_ver == "") printf("\x1b[2mUpdate check pending... (X)      \x1b[0m     ");
    else if (latest_ver != "none" && latest_ver != "error") {
        if (latest_ver != APP_VERSION) printf("\x1b[1;42m\x1b[37m NEW v%-6s AVAILABLE! (Y) \x1b[0m      ", latest_ver.c_str());
        else printf("\x1b[2mLatest version active (v%-6s)    \x1b[0m", latest_ver.c_str());
    } else printf("\x1b[31mUpdate check failed              \x1b[0m     ");
    printf("\x1b[36m│\x1b[0m\x1b[K\n");

    u32 ip = 0;
    struct in_addr addr;
    if (R_SUCCEEDED(nifmGetCurrentIpAddress(&ip)) && ip != 0) {
        addr.s_addr = ip;
        printf("\x1b[36m│\x1b[0m IP:       \x1b[1;32m%-15s\x1b[0m    \x1b[36m│\x1b[0m ", inet_ntoa(addr));
    } else {
        printf("\x1b[36m│\x1b[0m IP:       \x1b[1;31mDisconnected\x1b[0m       \x1b[36m│\x1b[0m ");
    }
    printf("Port: \x1b[1m%-5d\x1b[0m                        \x1b[36m│\x1b[0m\x1b[K\n", ConfigManager::getInstance().getPort());

    NifmInternetConnectionStatus net_status;
    const char* net_s = "Unknown";
    if (R_SUCCEEDED(nifmGetInternetConnectionStatus(NULL, NULL, &net_status))) {
        net_s = (net_status == NifmInternetConnectionStatus_Connected) ? "\x1b[1;32mOnline\x1b[0m" : "\x1b[1;31mOffline\x1b[0m";
    }
    printf("\x1b[36m│\x1b[0m Network:  %-25s \x1b[36m│\x1b[0m ", net_s);
    printf("Token: \x1b[1;33m%-15s\x1b[0m           \x1b[36m│\x1b[0m\x1b[K\n", ConfigManager::getInstance().getApiToken()[0] ? ConfigManager::getInstance().getApiToken() : "MISSING");

    printf("\x1b[36m├───────────────────────────────┴──────────────────────────────────────────┤\x1b[0m\x1b[K\n");
    printf("\x1b[36m│\x1b[0m \x1b[1;34m[ SYSTEM LOGS ]\x1b[0m                                                          \x1b[36m│\x1b[0m\x1b[K\n");
    
    if (g_app_logs.empty()) {
        for(int i=0; i<10; i++) printf("\x1b[36m│\x1b[0m %-72s \x1b[36m│\x1b[0m\x1b[K\n", i == 0 ? "Waiting for sysmodule bridge..." : "");
    } else {
        size_t start = (g_app_logs.size() > 10) ? g_app_logs.size() - 10 : 0;
        for (size_t i = 0; i < 10; i++) {
            printf("\x1b[36m│\x1b[0m ");
            if (start + i < g_app_logs.size()) {
                printf("%s", g_app_logs[start + i].c_str());
                printf("\x1b[75G\x1b[36m│\x1b[0m\x1b[K\n");
            } else {
                printf("%-72s \x1b[36m│\x1b[0m\x1b[K\n", "");
            }
        }
    }

    printf("\x1b[36m└──────────────────────────────────────────────────────────────────────────┘\x1b[0m\x1b[K\n");
    printf(" \x1b[1;37m(X)Check  (Y)Update/Install  (ZR)Reset  (-)DevMode  (+)Exit\x1b[0m\x1b[K\n");
    if (g_dev_mode) printf(" \x1b[45m\x1b[1;37m DEV MODE ACTIVE \x1b[0m  UDP Broadcast on Port 2828\x1b[K\n");
    else printf("\x1b[K\n");
}

int main(int argc, char **argv) {
    consoleInit(NULL); consoleClear();
    
    // CRITICAL: Initialize sockets before curl or any network activity
    if (R_FAILED(socketInitializeDefault())) {
        printf("Failed to init sockets!\n");
        consoleUpdate(NULL);
        sleep(5);
        return 1;
    }

    nxlinkStdio();
    PadState pad; padConfigureInput(1, HidNpadStyleSet_NpadStandard); padInitializeDefault(&pad);
    nifmInitialize(NifmServiceType_User); curl_global_init(CURL_GLOBAL_ALL);
    ConfigManager::getInstance().load(); add_app_log("[CONFIG] Settings loaded...");

    std::string latest_ver = ""; bool checking_update = false; bool nso_checked = false;
    bool sysmodule_active = false; u64 last_sysmodule_check = 0; u64 loop_count = 0;

    time_t now_t = time(NULL);
    if (now_t > 1000000 && (now_t - ConfigManager::getInstance().getLastUpdateCheck() > 86400)) {
        checking_update = true; draw_ui(latest_ver, checking_update, false, 0); consoleUpdate(NULL);
        latest_ver = get_latest_version(latest_date); checking_update = false;
        ConfigManager::getInstance().setLastUpdateCheck((long)now_t); ConfigManager::getInstance().save();
    }

    while (appletMainLoop()) {
        padUpdate(&pad); u32 kDown = padGetButtonsDown(&pad);
        if (!nso_checked) { u32 ip = 0; if (R_SUCCEEDED(nifmGetCurrentIpAddress(&ip)) && ip != 0) { check_and_fix_sysmodule(); nso_checked = true; } }
        
        u64 now = svcGetSystemTick() / 19200000;
        if (now - last_sysmodule_check > 5) {
            sysmodule_active = is_sysmodule_running();
            if (sysmodule_active) fetch_sysmodule_logs();
            else { 
                fetch_offline_boot_logs();
                char err[128]; snprintf(err, sizeof(err), "\x1b[31m[ERROR] Connection failed (%s:%d)\x1b[0m", get_sysmodule_ip().c_str(), ConfigManager::getInstance().getPort()); 
                add_app_log(err); 
            }
            last_sysmodule_check = now;
        }

        draw_ui(latest_ver, checking_update, sysmodule_active, loop_count++);
        if (kDown & HidNpadButton_Plus) break;
        if (kDown & HidNpadButton_Minus) { 
            g_dev_mode = !g_dev_mode; 
            if (g_dev_mode) add_app_log("\x1b[35m[SYSTEM] Dev Mode ON\x1b[0m");
            else add_app_log("\x1b[35m[SYSTEM] Dev Mode OFF\x1b[0m");
        }
        if (kDown & HidNpadButton_X) { checking_update = true; draw_ui(latest_ver, checking_update, sysmodule_active, loop_count); consoleUpdate(NULL); latest_ver = get_latest_version(latest_date); checking_update = false; }
        if ((kDown & HidNpadButton_Y) && latest_ver != "" && latest_ver != APP_VERSION && latest_ver != "none" && latest_ver != "error") {
            if (download_update(latest_ver)) { add_app_log("\x1b[32mUpdate Done! Restart.\x1b[0m"); consoleUpdate(NULL); sleep(2); break; }
        }
        consoleUpdate(NULL); svcSleepThread(16666666ULL);
    }
    nifmExit(); curl_global_cleanup(); socketExit(); consoleExit(NULL); return 0;
}
