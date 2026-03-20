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

std::string latest_date = "";
bool g_dev_mode = false;

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
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 5L); // 5s timeout

    res = curl_easy_perform(curl_handle);
    std::string version = "";
    if (res == CURLE_OK) {
        json j = json::parse(chunk.memory, nullptr, false);
        if (!j.is_discarded() && j.contains("tag_name")) {
            version = j.value("tag_name", "");
            date = j.value("published_at", "");
            if (!date.empty() && date.length() > 10) date = date.substr(0, 10); // YYYY-MM-DD
            if (!version.empty() && version[0] == 'v') version = version.substr(1);
        } else {
            version = "none";
        }
    } else {
        version = "error";
    }

    curl_easy_cleanup(curl_handle);
    free(chunk.memory);
    return version;
}

bool download_file(const std::string& url, const std::string& path) {
    CURL *curl;
    FILE *fp;
    CURLcode res;
    curl = curl_easy_init();
    if (curl) {
        fp = fopen(path.c_str(), "wb");
        if (!fp) {
            curl_easy_cleanup(curl);
            return false;
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30s timeout for downloads
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);
        return (res == CURLE_OK);
    }
    return false;
}

void make_dirs(std::string path) {
    size_t pos = 0;
    while ((pos = path.find('/', pos + 1)) != std::string::npos) {
        std::string dir = path.substr(0, pos);
        mkdir(dir.c_str(), 0777);
    }
    mkdir(path.c_str(), 0777);
}

bool file_exists(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

void check_and_fix_sysmodule() {
    const char* paths[] = {
        "sdmc:/atmosphere/contents/42000000000000FF/exefs/main",
        "/atmosphere/contents/42000000000000FF/exefs/main"
    };
    
    bool found = false;
    for (const char* p : paths) {
        if (file_exists(p)) {
            found = true;
            break;
        }
    }

    if (!found) {
        printf("\x1b[1;33mSysmodule missing at expected paths.\x1b[0m\n");
        printf("Attempting to auto-download as safeguard...\n");
        consoleUpdate(NULL);
        
        std::string date;
        std::string version = get_latest_version(date);
        if (version == "error" || version == "none") {
            printf("\x1b[31mCould not fetch latest version for download.\x1b[0m\n");
            consoleUpdate(NULL);
            sleep(2);
            return;
        }

        printf("Downloading sysmodule v%s...\n", version.c_str());
        consoleUpdate(NULL);

        make_dirs("sdmc:/atmosphere/contents/42000000000000FF/exefs");
        
        std::string url = std::string(REPO_URL) + "/releases/download/v" + version + "/" + NSO_FILENAME;
        if (download_file(url, paths[0])) {
            printf("\x1b[32mSysmodule downloaded! Please restart your Switch.\x1b[0m\n");
            consoleUpdate(NULL);
            sleep(3);
        } else {
            printf("\x1b[31mFailed to download sysmodule!\x1b[0m\n");
            consoleUpdate(NULL);
            sleep(2);
        }
    }
}

bool download_update(const std::string& version) {
    bool nro_ok = download_file(REPO_URL "/releases/download/v" + version + "/" + NRO_FILENAME, "sdmc:/switch/" NRO_FILENAME);
    bool nso_ok = download_file(REPO_URL "/releases/download/v" + version + "/" + NSO_FILENAME, "sdmc:/atmosphere/contents/" SYSMODULE_PROGRAM_ID "/exefs/main");
    return nro_ok && nso_ok;
}

std::string get_sysmodule_ip() {
    u32 cur_ip = 0;
    if (R_SUCCEEDED(nifmGetCurrentIpAddress(&cur_ip)) && cur_ip != 0) {
        struct in_addr addr; addr.s_addr = cur_ip;
        // If we are on the known host IP network, we likely need to target the host PC
        if (strcmp(inet_ntoa(addr), "192.168.1.35") == 0) {
            return "192.168.1.35";
        }
    }
    return "127.0.0.1";
}

bool is_sysmodule_running() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(ConfigManager::getInstance().getPort());
    serv_addr.sin_addr.s_addr = inet_addr(get_sysmodule_ip().c_str());

    // Set non-blocking for quick check
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    int res = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (res < 0 && errno == EINPROGRESS) {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 500000; // Increased to 500ms for stability
        fd_set myset;
        FD_ZERO(&myset);
        FD_SET(sock, &myset);
        if (select(sock + 1, NULL, &myset, NULL, &tv) > 0) {
            int valopt;
            socklen_t lon = sizeof(int);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon);
            res = (valopt == 0) ? 0 : -1;
        } else {
            res = -1;
        }
    }
    fcntl(sock, F_SETFL, flags); // Restore flags
    close(sock);
    return (res == 0);
}

std::vector<std::string> g_app_logs;

void add_app_log(const std::string& msg) {
    if (g_app_logs.size() > 15) g_app_logs.erase(g_app_logs.begin());
    g_app_logs.push_back(msg);
}

void fetch_sysmodule_logs() {
    CURL *curl_handle;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = (char*)malloc(1);
    chunk.size = 0;

    curl_handle = curl_easy_init();
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/logs", get_sysmodule_ip().c_str(), ConfigManager::getInstance().getPort());
    
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 2L);

    res = curl_easy_perform(curl_handle);
    if (res == CURLE_OK) {
        json j = json::parse(chunk.memory, nullptr, false);
        if (!j.is_discarded() && j.is_array()) {
            g_app_logs.clear();
            for (auto& item : j) {
                std::string level = item.value("level", "INFO");
                std::string msg = item.value("message", "");
                std::string color = "\x1b[37m"; // White
                if (level == "ERROR") color = "\x1b[31m";
                else if (level == "WARN") color = "\x1b[33m";
                else if (level == "INFO") color = "\x1b[36m";
                
                char entry[256];
                snprintf(entry, sizeof(entry), "%s[%s] %s\x1b[0m", color.c_str(), level.c_str(), msg.c_str());
                g_app_logs.push_back(entry);
            }
        }
    }
    curl_easy_cleanup(curl_handle);
    free(chunk.memory);
}

void draw_ui(const std::string& latest_ver, bool checking_update, bool sysmodule_active, u64 loop_count, u64 kDown) {
    // Reset cursor to top - do NOT clear screen to avoid flickering
    printf("\x1b[1;1H"); 

    // Premium Header with clear-line suffixes
    char header_title[128];
    snprintf(header_title, sizeof(header_title), "        HOME ASSISTANT SWITCH v%s         ", APP_VERSION);
    printf("\x1b[41m\x1b[1;37m%s\x1b[0m\x1b[K\n", header_title);
    printf("\x1b[2m  (c) 2026 FaserF | https://github.com/FaserF/ha-NintendoSwitchCFW\x1b[0m\x1b[K\n\n");

    // System Status Section
    printf("\x1b[1;36m[ SYSTEM STATUS ]\x1b[0m\x1b[K\n");
    printf("  Status:        %s\x1b[K\n", sysmodule_active ? "\x1b[1;32mACTIVE\x1b[0m" : "\x1b[1;31mINACTIVE\x1b[0m");
    
    u32 ip = 0;
    if (R_SUCCEEDED(nifmGetCurrentIpAddress(&ip)) && ip != 0) {
        struct in_addr addr; addr.s_addr = ip;
        printf("  IP Address:    \x1b[1;32m%s\x1b[0m\x1b[K\n", inet_ntoa(addr));
    } else {
        printf("  IP Address:    \x1b[1;31mDisconnected\x1b[0m\x1b[K\n");
    }
    printf("\x1b[K\n");

    // Configuration
    printf("\x1b[1;36m[ CONFIGURATION ]\x1b[0m\x1b[K\n");
    printf("  Port: \x1b[1m%d\x1b[0m  |  Token: \x1b[1;33m%s\x1b[0m\x1b[K\n\n", 
           ConfigManager::getInstance().getPort(), 
           ConfigManager::getInstance().getApiToken()[0] ? ConfigManager::getInstance().getApiToken() : "NONE");

    // Updates
    printf("\x1b[1;36m[ UPDATES ]\x1b[0m\x1b[K\n");
    if (checking_update) {
        printf("  \x1b[5;33mChecking...\x1b[0m\x1b[K\n");
    } else if (latest_ver == "") {
        printf("  \x1b[2mUpdate check pending... (X) to Check\x1b[0m\x1b[K\n");
    } else if (latest_ver != "none" && latest_ver != "error") {
        if (latest_ver != APP_VERSION) {
            if (APP_VERSION > latest_ver) {
                printf("  \x1b[1;32mDEV VERSION (Ahead of GitHub v%s)\x1b[0m\x1b[K\n", latest_ver.c_str());
            } else {
                printf("  \x1b[1;42m\x1b[37m NEW v%s AVAILABLE! \x1b[0m (Y)\x1b[K\n", latest_ver.c_str());
            }
        } else {
            printf("  \x1b[2mLatest version active.\x1b[0m\x1b[K\n");
        }
    } else {
        printf("  \x1b[2mUpdate check failed/skipped.\x1b[0m\x1b[K\n");
    }
    printf("\x1b[K\n");

    // Logs
    printf("\x1b[1;36m[ SYSTEM LOGS ]\x1b[0m\x1b[K\n");
    if (g_app_logs.empty()) {
        printf("  Waiting for bridge...\x1b[K\n");
    } else {
        size_t visible = 10;
        size_t start = (g_app_logs.size() > visible) ? g_app_logs.size() - visible : 0;
        for (size_t i = start; i < g_app_logs.size(); i++) {
            printf("  %s\x1b[K\n", g_app_logs[i].c_str());
        }
    }
    for (int i=0; i<3; ++i) printf("\x1b[K\n"); // Extra padding
    printf("\x1b[K\r"); // Clear line and return carriage

    // Footer
    printf("\x1b[1;34m----------------------------------------------------\x1b[0m\x1b[K\n");
    printf(" (ZL)Reload (ZR)Reset (X)Update (Y)Install (+)Exit (-)Dev\x1b[K\n");
    printf("\x1b[1;34m----------------------------------------------------\x1b[0m\x1b[K\n");
}

int main(int argc, char **argv) {
    consoleInit(NULL);
    consoleClear();
    
    // Enable USB logging immediately for diagnostics (no buttons needed)
    nxlinkStdio();
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    nifmInitialize(NifmServiceType_User);
    curl_global_init(CURL_GLOBAL_ALL);

    // ConfigManager::getInstance().load(); // Skip for barebones test
    std::string latest_ver = ""; // Initial empty
    bool checking_update = false;
    bool nso_checked = false;
    bool sysmodule_active = false;
    u64 loop_count = 0;

    consoleUpdate(NULL);
    sysmodule_active = false;
    u64 last_sysmodule_check = 0;

    // Auto update check every 24h
    time_t now_t = time(NULL);
    if (now_t > 1000000 && (now_t - ConfigManager::getInstance().getLastUpdateCheck() > 86400)) {
        checking_update = true;
        draw_ui(latest_ver, checking_update, sysmodule_active, 0, 0);
        consoleUpdate(NULL);
        latest_ver = get_latest_version(latest_date);
        checking_update = false; 
        ConfigManager::getInstance().setLastUpdateCheck((long)now_t);
        ConfigManager::getInstance().save();
    }

    u64 kDown = 0;

    while (appletMainLoop()) {
        padUpdate(&pad);
        kDown = padGetButtonsDown(&pad);

        // Check if sysmodule is present ( safeguarding )
        if (!nso_checked) {
            u32 ip = 0;
            if (R_SUCCEEDED(nifmGetCurrentIpAddress(&ip)) && ip != 0) {
                check_and_fix_sysmodule();
                nso_checked = true; // Set even if fix fails to avoid hanging loop
            }
        }

        // Periodically check if sysmodule is actually running
        u64 now = svcGetSystemTick() / 19200000;
        if (now - last_sysmodule_check > 5) { // Every 5 seconds
            sysmodule_active = is_sysmodule_running();
            if (sysmodule_active) {
                fetch_sysmodule_logs();
            } else {
                char err[128];
                snprintf(err, sizeof(err), "\x1b[31m[ERROR] Sysmodule connection failed (Port %d)\x1b[0m", ConfigManager::getInstance().getPort());
                add_app_log(err);
            }
            last_sysmodule_check = now;
        }

        draw_ui(latest_ver, checking_update, sysmodule_active, loop_count++, kDown);

        if (kDown & HidNpadButton_Plus) break;
        if (kDown & HidNpadButton_ZL) { ConfigManager::getInstance().load(); }
        if (kDown & HidNpadButton_ZR) { ConfigManager::getInstance().generateDefaultConfig(); ConfigManager::getInstance().save(); }
        
        // Toggle Developer Mode (Minus Only)
        if (kDown & HidNpadButton_Minus) {
            g_dev_mode = !g_dev_mode;
            if (g_dev_mode) {
                nxlinkStdio();
                add_app_log("\x1b[35m[SYSTEM] Developer Mode ENABLED\x1b[0m");
                printf("[DEBUG] Home Assistant Switch Developer Mode Active\n");
            } else {
                add_app_log("\x1b[35m[SYSTEM] Developer Mode DISABLED\x1b[0m");
            }
        }

        if (kDown & HidNpadButton_X) {
            checking_update = true;
            draw_ui(latest_ver, checking_update, sysmodule_active, loop_count++, kDown);
            consoleUpdate(NULL);
            latest_ver = get_latest_version(latest_date);
            checking_update = false;
        }

        if ((kDown & HidNpadButton_Y) && latest_ver != "" && latest_ver != APP_VERSION && latest_ver != "none" && latest_ver != "error") {
            printf("Downloading update...\n");
            consoleUpdate(NULL);
            if (download_update(latest_ver)) {
                printf("\x1b[32mUpdate Successful! Please restart the app.\x1b[0m\n");
                consoleUpdate(NULL);
                sleep(2);
                break;
            } else {
                printf("\x1b[31mUpdate Failed!\x1b[0m\n");
                consoleUpdate(NULL);
                sleep(2);
            }
        }

        consoleUpdate(NULL);
        svcSleepThread(16666666ULL); // ~60fps
    }

    nifmExit();
    curl_global_cleanup();
    socketExit();
    consoleExit(NULL);
    return 0;
}
