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
#define APP_VERSION "0.2.2"
#endif

#define REPO_NAME "FaserF/ha-NintendoSwitchCFW"
#define REPO_URL "https://github.com/" REPO_NAME

struct ReleaseInfo {
    std::string tag;
    std::string date;
    std::string url_main;
    std::string url_npdm;
    std::string url_flag;
    std::string url_nro;
    bool valid = false;
};

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
ReleaseInfo g_latest_release;
std::string latest_date = "";
bool g_dev_mode = false;
bool g_is_applet_mode = false;
std::vector<std::string> g_app_logs;

void add_app_log(const std::string& msg) {
    if (g_app_logs.size() > 30) g_app_logs.erase(g_app_logs.begin());
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

bool fill_release_info(ReleaseInfo& info) {
    CURL *curl_handle;
    CURLcode res;
    struct MemoryStruct chunk = {(char*)malloc(1), 0};

    auto fetch = [&](const char* url, bool silent = false) -> bool {
        chunk.size = 0;
        curl_handle = curl_easy_init();
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "HomeAssistant-Switch-Integration/1.0"); 
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L); 
        res = curl_easy_perform(curl_handle);
        long response_code = 0;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
        curl_easy_cleanup(curl_handle);
        
        if (res != CURLE_OK) {
            if (!silent) {
                char fail[128]; snprintf(fail, sizeof(fail), "\x1b[31m[ERROR] Curl: %s\x1b[0m", curl_easy_strerror(res));
                add_app_log(fail);
            }
            return false;
        }
        
        if (response_code != 200) {
            if (!silent) {
                char fail[128]; snprintf(fail, sizeof(fail), "\x1b[31m[ERROR] HTTP %ld (Rate limit?)\x1b[0m", response_code);
                add_app_log(fail);
            }
            return false;
        }
        return true;
    };

    // Try /latest (stable), if 404 try /releases (can be pre-releases)
    if (!fetch("https://api.github.com/repos/" REPO_NAME "/releases/latest", true)) {
        if (!fetch("https://api.github.com/repos/" REPO_NAME "/releases", false)) {
            add_app_log("\x1b[31m[ERROR] Release API unreachable\x1b[0m");
            free(chunk.memory); return false;
        }
    }

    json j = json::parse(chunk.memory, nullptr, false);
    free(chunk.memory);
    if (j.is_discarded()) return false;

    json release = j.is_array() && !j.empty() ? j[0] : j;
    if (release.contains("tag_name")) {
        info.tag = release.value("tag_name", "");
        info.date = release.value("published_at", "");
        if (!info.date.empty() && info.date.length() > 10) info.date = info.date.substr(0, 10); 

        if (release.contains("assets") && release["assets"].is_array()) {
            for (auto& asset : release["assets"]) {
                std::string name = asset.value("name", "");
                std::string url = asset.value("browser_download_url", "");
                if (name == "main") info.url_main = url;
                else if (name == "main.npdm") info.url_npdm = url;
                else if (name == "boot2.flag") info.url_flag = url;
                else if (name == "homeassistant.nro") info.url_nro = url;
            }
        }
        info.valid = true;
        return true;
    }
    
    add_app_log("\x1b[31m[ERROR] No release tag in JSON\x1b[0m");
    return false;
}

std::string get_latest_version(std::string& date) {
    if (fill_release_info(g_latest_release)) {
        date = g_latest_release.date;
        std::string v = g_latest_release.tag;
        if (!v.empty() && v[0] == 'v') v = v.substr(1);
        return v;
    }
    return "error";
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

void check_and_fix_sysmodule(bool manual = false) {
    const char* main_path = "sdmc:/atmosphere/contents/010000000000CAFE/exefs/main";
    const char* npdm_path = "sdmc:/atmosphere/contents/010000000000CAFE/exefs/main.npdm";
    
    struct stat st1, st2;
    bool main_exists = (stat(main_path, &st1) == 0 && st1.st_size > 1000);
    bool npdm_exists = (stat(npdm_path, &st2) == 0 && st2.st_size > 100);

    char msg[256]; snprintf(msg, sizeof(msg), "[DEBUG] Verifying: %s", main_path);
    add_app_log(msg);

    if (manual) add_app_log("\x1b[36m[INFO] Manual repair starts...\x1b[0m");

    if (!main_exists || !npdm_exists || manual) {
        if (!manual) add_app_log("\x1b[33m[WARN] Sysmodule missing or invalid size! Recovering...\x1b[0m");
        else add_app_log("\x1b[35m[INFO] Forced re-downloading fresh sysmodule files...\x1b[0m");
        
        if (!g_latest_release.valid) {
            std::string d; get_latest_version(d);
        }

        if (g_latest_release.valid) {
            make_dirs("sdmc:/atmosphere/contents/010000000000CAFE/exefs");
            make_dirs("sdmc:/atmosphere/contents/010000000000CAFE/flags");
            
            add_app_log("[INFO] Requesting files from GitHub...");
            bool ok1 = false;
            if (!g_latest_release.url_main.empty()) {
                ok1 = download_file(g_latest_release.url_main, main_path);
            } else add_app_log("\x1b[31m[ERROR] 'main' asset missing in release\x1b[0m");

            bool ok2 = false;
            if (!g_latest_release.url_npdm.empty()) {
                ok2 = download_file(g_latest_release.url_npdm, npdm_path);
            } else add_app_log("\x1b[31m[ERROR] 'main.npdm' asset missing in release\x1b[0m");
            
            if (ok1 && ok2) {
                // Try to download the official boot2.flag from release, fallback to local empty file
                bool flag_ok = false;
                if (!g_latest_release.url_flag.empty()) {
                    flag_ok = download_file(g_latest_release.url_flag, "sdmc:/atmosphere/contents/010000000000CAFE/flags/boot2.flag");
                }
                
                if (!flag_ok) {
                    FILE* f = fopen("sdmc:/atmosphere/contents/010000000000CAFE/flags/boot2.flag", "w");
                    if (f) {
                        fprintf(f, "boot2\n");
                        fclose(f);
                    }
                }
                
                add_app_log("\x1b[32m[SUCCESS] Sysmodule restored. REBOOT REQUIRED.\x1b[0m");
                add_app_log("\x1b[32m[SYSTEM] Please restart your Switch now!\x1b[0m");
            } else add_app_log("\x1b[31m[ERROR] Download failed. Check internet.\x1b[0m");
        } else {
            add_app_log("\x1b[31m[ERROR] FAILED to fetch release info from GitHub.\x1b[0m");
        }
    } else {
        // Only reached if !manual and both exist
    }
}

bool download_update(const std::string& /*version*/) {
    if (!g_latest_release.valid || g_latest_release.url_nro.empty() || g_latest_release.url_main.empty()) {
        add_app_log("\x1b[31m[ERROR] Assets missing for update\x1b[0m");
        return false;
    }
    
    add_app_log("[INFO] Downloading update assets...");
    bool nro_ok = download_file(g_latest_release.url_nro, "sdmc:/switch/homeassistant.nro");
    bool nso_ok = download_file(g_latest_release.url_main, "sdmc:/atmosphere/contents/010000000000CAFE/exefs/main");
    
    bool npdm_ok = true;
    if (!g_latest_release.url_npdm.empty()) {
        npdm_ok = download_file(g_latest_release.url_npdm, "sdmc:/atmosphere/contents/010000000000CAFE/exefs/main.npdm");
    }
    
    // Also update flag if present
    if (!g_latest_release.url_flag.empty()) {
        download_file(g_latest_release.url_flag, "sdmc:/atmosphere/contents/010000000000CAFE/flags/boot2.flag");
    }

    return nro_ok && nso_ok && npdm_ok;
}

std::string get_sysmodule_ip() {
    u32 cur_ip = 0;
    if (R_SUCCEEDED(nifmGetCurrentIpAddress(&cur_ip)) && cur_ip != 0) {
        struct in_addr addr; addr.s_addr = cur_ip;
        return std::string(inet_ntoa(addr));
    }
    return "127.0.0.1";
}

std::string try_connect(const std::string& ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "Socket Fail";
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    int res = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    int err_code = 0;
    
    if (res < 0) {
        if (errno == EINPROGRESS) {
            struct timeval tv = {2, 0}; // 2s timeout
            fd_set myset; FD_ZERO(&myset); FD_SET(sock, &myset);
            if (select(sock + 1, NULL, &myset, NULL, &tv) > 0) {
                socklen_t l = sizeof(int);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)(&err_code), &l);
                res = (err_code == 0) ? 0 : -1;
            } else {
                res = -1;
                err_code = ETIMEDOUT;
            }
        } else {
            err_code = errno;
        }
    }

    fcntl(sock, F_SETFL, flags);
    close(sock);

    if (res == 0) return "";
    
    if (err_code == ECONNREFUSED) return "Refused (Process Missing?)";
    if (err_code == ETIMEDOUT) return "Timeout (Blocked)";
    if (err_code == EHOSTUNREACH) return "Net Unreachable";
    char generic[32]; snprintf(generic, sizeof(generic), "Error %d", err_code);
    return generic;
}

std::string g_sys_status_msg = "";

bool is_sysmodule_running() {
    int port = ConfigManager::getInstance().getPort();
    // Try localhost first as it's most reliable on-console
    std::string err = try_connect("127.0.0.1", port);
    if (err == "") {
        g_sys_status_msg = "Bridge OK";
        return true;
    }
    g_sys_status_msg = err;
    return false;
}

void fetch_sysmodule_logs() {
    struct MemoryStruct chunk = {(char*)malloc(1), 0};
    CURL *curl = curl_easy_init();
    char url[256];
    // Use localhost for log fetching too
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/logs", ConfigManager::getInstance().getPort());
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
    static long last_pos = 0;
    FILE *f = fopen("sdmc:/config/HomeAssistantSwitch/ha_sysmodule_boot.log", "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long current_size = ftell(f);
        if (current_size < last_pos) last_pos = 0; // Reset if file was recreated

        fseek(f, last_pos, SEEK_SET);
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (line[0] == '\0') continue;
            char entry[300];
            snprintf(entry, sizeof(entry), "\x1b[35m[BOOT] %s\x1b[0m", line);
            add_app_log(entry);
        }
        last_pos = ftell(f);
        fclose(f);
    }
}

void draw_ui(const std::string& latest_ver, bool checking_update, bool sysmodule_active, u64) {
    printf("\x1b[H"); // Home
    // Header (80 chars wide)
    printf("\x1b[44m\x1b[1;37m                                                                                \x1b[0m\r");
    printf("\x1b[44m\x1b[1;37m          HOME ASSISTANT SWITCH v%-10s                                    \x1b[0m\n", APP_VERSION);
    printf("\x1b[44m\x1b[1;37m                                                                                \x1b[0m\n");
    
    printf("+------------------------------------+-------------------------------------+\n");
    
    // Line 1: Status & Update info
    printf("| Status:");
    printf("\x1b[11G"); // Move to col 11
    if (sysmodule_active) printf("\x1b[1;32mACTIVE (Bridge OK)  \x1b[0m");
    else printf("\x1b[1;31mINACTIVE (%s)  \x1b[0m", g_sys_status_msg.substr(0,18).c_str());
    
    printf("\x1b[38G| "); // Move to col 38
    if (checking_update) printf("\x1b[5;33mChecking updates...\x1b[0m");
    else if (latest_ver == "") printf("\x1b[2mPending check (X)\x1b[0m");
    else if (latest_ver != "none" && latest_ver != "error") {
        if (latest_ver != APP_VERSION) {
            if (APP_VERSION > latest_ver) printf("\x1b[1;34mRunning newer dev build\x1b[0m");
            else printf("\x1b[1;42m\x1b[37m NEW v%-6s READY! (Y) \x1b[0m", latest_ver.c_str());
        }
        else printf("\x1b[2mLatest version (v%-6s)\x1b[0m", latest_ver.c_str());
    } else printf("\x1b[31mUpdate check failed\x1b[0m");
    printf("\x1b[76G|\n");

    // Line 2: IP & Port
    printf("| IP:");
    printf("\x1b[11G");
    u32 ip = 0; struct in_addr addr;
    if (R_SUCCEEDED(nifmGetCurrentIpAddress(&ip)) && ip != 0) {
        addr.s_addr = ip;
        printf("\x1b[1;32m%-15s\x1b[0m", inet_ntoa(addr));
    } else printf("\x1b[1;31mDisconnected\x1b[0m");
    
    printf("\x1b[38G| Port: \x1b[1m%-5d\x1b[0m", ConfigManager::getInstance().getPort());
    printf("\x1b[76G|\n");

    // Line 3: Pwr Mode & Token
    printf("| Mode: \x1b[1;32mBalanced  \x1b[0m");
    printf("\x1b[38G| Token: \x1b[1;33m%-25s\x1b[0m", ConfigManager::getInstance().getApiToken());
    printf("\x1b[76G|\n");

    printf("+------------------------------------+-------------------------------------+\n");
    printf("| \x1b[1;34m[ SYSTEM LOGS ]\x1b[0m                                                          |\n");
    
    const int log_lines = 27;
    if (g_app_logs.empty()) {
        for(int i=0; i<log_lines; i++) printf("| %-72s |\n", i == 0 ? "Waiting for sysmodule bridge..." : "");
    } else {
        size_t start = (g_app_logs.size() > (size_t)log_lines) ? g_app_logs.size() - (size_t)log_lines : 0;
        for (int i = 0; i < log_lines; i++) {
            printf("| ");
            if (start + i < g_app_logs.size()) {
                printf("%s", g_app_logs[start + i].c_str());
            }
            printf("\x1b[76G|\n");
        }
    }

    printf("+--------------------------------------------------------------------------+\n");
    
    // Fixed Footer at Row 40+
    printf("\x1b[40;1H (X)Check  (Y)Update  (ZR)Reset  (-)DevMode  (+)Exit\n");
    printf("\x1b[42;1H Brought to you by \x1b[1;32mFaserF\x1b[0m - \x1b[1;94m" REPO_URL "\x1b[0m\n");
    
    if (g_dev_mode) {
        printf("\x1b[43;1H \x1b[45m\x1b[1;37m DEV MODE ACTIVE \x1b[0m  UDP: 2828                                      \n");
    } else {
        printf("\x1b[43;1H                                                                                \n");
    }

    if (g_is_applet_mode) {
        printf("\x1b[44;1H \x1b[41m\x1b[1;37m APPLET MODE \x1b[0m Limited RAM. Hold R on game for full mode.         \n");
    } else {
        printf("\x1b[44;1H                                                                                \n");
    }

    if (!sysmodule_active) {
        printf("\x1b[20;20H\x1b[1;41m\x1b[37m  !!! SYSTEM REBOOT REQUIRED !!!  \x1b[0m");
        printf("\x1b[21;18H\x1b[1;31m   Atmosphere must restart to load   \x1b[0m");
        printf("\x1b[22;18H\x1b[1;31m   the sysmodule from /contents/     \x1b[0m");
    }
}

int main(int, char **) {
    consoleInit(NULL); consoleClear();
    g_is_applet_mode = (appletGetAppletType() == AppletType_LibraryApplet);
    
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
        if (now - last_sysmodule_check > 2) { // 2s check interval for faster response
            fetch_offline_boot_logs(); 
            sysmodule_active = is_sysmodule_running();
            if (sysmodule_active) {
                fetch_sysmodule_logs();
            } else { 
                char err[128]; snprintf(err, sizeof(err), "\x1b[31m[ERROR] Bridge: %s (Port %d)\x1b[0m", g_sys_status_msg.c_str(), ConfigManager::getInstance().getPort()); 
                add_app_log(err); 
                if (loop_count % 10 == 0) {
                    add_app_log("\x1b[33m[HINT] No bridge? Check /config/HomeAssistantSwitch/boot.log\x1b[0m");
                    check_and_fix_sysmodule(); // Aggressive retry
                }
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
        if (kDown & HidNpadButton_ZR) {
            add_app_log("\x1b[33m[SYSTEM] Resetting and verifying integrity...\x1b[0m");
            ConfigManager::getInstance().generateDefaultConfig();
            ConfigManager::getInstance().save();
            ConfigManager::getInstance().load();
            check_and_fix_sysmodule(true); // Manual verbose repair
        }
        if (kDown & HidNpadButton_X) { 
            checking_update = true; draw_ui(latest_ver, checking_update, sysmodule_active, loop_count); consoleUpdate(NULL); 
            latest_ver = get_latest_version(latest_date); checking_update = false; 
        }
        if ((kDown & HidNpadButton_Y) && latest_ver != "" && latest_ver != APP_VERSION && latest_ver != "none" && latest_ver != "error") {
            if (download_update(latest_ver)) { 
                add_app_log("\x1b[32mUpdate Done! Exiting...\x1b[0m"); 
                for(int i=0; i<60; i++) { consoleUpdate(NULL); svcSleepThread(16666666ULL); }
                break; 
            }
        }
        consoleUpdate(NULL); svcSleepThread(16666666ULL);
    }
    
    curl_global_cleanup();
    socketExit();
    nifmExit();
    consoleExit(NULL);
    return 0;
}
