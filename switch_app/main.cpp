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

#define REPO_NAME "FaserF/ha-NintentdoSwitchCFW"
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

std::string get_latest_version(std::string& date) {
    CURL *curl_handle;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = (char*)malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, "https://api.github.com/repos/" REPO_NAME "/releases/latest");
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

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
        "sdmc:/atmosphere/contents/4200000000000001/exefs/main",
        "/atmosphere/contents/4200000000000001/exefs/main"
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

        make_dirs("sdmc:/atmosphere/contents/4200000000000001/exefs");
        
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

bool is_sysmodule_running() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(1337);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Set non-blocking for quick check
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    int res = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (res < 0 && errno == EINPROGRESS) {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout
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

    close(sock);
    return (res == 0);
}

void draw_ui(const std::string& latest_ver, bool checking_update, bool sysmodule_active, u64 loop_count, u64 kDown) {
    // Only clear if completely redrawing or use cursor positioning
    printf("\x1b[1;1H"); // Move to top-left
    printf("\x1b[1;36m+--------------------------------------------------+\x1b[0m\n");
    printf("\x1b[1;36m|\x1b[0m             \x1b[1mHOME ASSISTANT SWITCH\x1b[0m              \x1b[1;36m|\x1b[0m\n");
    printf("\x1b[1;36m+--------------------------------------------------+\x1b[0m\n");
    printf("  \x1b[2mBrought to you by FaserF\x1b[0m\n");
    printf("  \x1b[2mhttps://github.com/FaserF/ha-NintendoSwitchCFW/releases/latest/download/homeassistant.nrous\x1b[0m\n\n");
    printf("\x1b[1m[ SYSTEM STATUS ]\x1b[0m\n");
    printf("  Service Status: %s\n", sysmodule_active ? "\x1b[1;32m[X] ACTIVE\x1b[0m" : "\x1b[1;31m[ ] INACTIVE\x1b[0m");
    if (!sysmodule_active) {
        printf("  \x1b[1;31mSearch Path: /atmosphere/contents/4200000000000001/exefs/main\x1b[0m\n");
    }

    u32 ip = 0;
    if (R_SUCCEEDED(nifmGetCurrentIpAddress(&ip)) && ip != 0) {
        struct in_addr addr; addr.s_addr = ip;
        printf("  IP Address:     \x1b[1;32m%s\x1b[0m\n", inet_ntoa(addr));
    } else {
        printf("  IP Address:     \x1b[1;31mNot Connected\x1b[0m\n");
    }
    printf("  App Version:    \x1b[2m%s\x1b[0m\n\n", APP_VERSION);

    // Configuration
    printf("\x1b[1m[ CONFIGURATION ]\x1b[0m\n");
    printf("  API Token:      \x1b[1;33m%s\x1b[0m\n", ConfigManager::getInstance().getApiToken());
    printf("  Service Port:   \x1b[2m%d\x1b[0m\n\n", ConfigManager::getInstance().getPort());

    // Updates
    printf("\x1b[1m[ UPDATES ]\x1b[0m\n");
    if (checking_update) {
        printf("  \x1b[5mChecking for updates...\x1b[0m\n");
    } else if (latest_ver != "" && latest_ver != APP_VERSION && latest_ver != "none" && latest_ver != "error") {
        printf("  \x1b[1;32m* New Version Available: v%s!\x1b[0m\n", latest_ver.c_str());
        printf("  Press (Y) to start automatic update.\n");
    } else if (latest_ver == APP_VERSION) {
        printf("  \x1b[2mYou are running the latest version.\x1b[0m\n");
    } else if (latest_ver == "error") {
        printf("  \x1b[31mError checking for updates.\x1b[0m\n");
    } else {
        printf("  Press (X) to check for updates.\n");
    }
    printf("\n");

    // Controls Footer
    printf("\x1b[1;34m----------------------------------------------------\x1b[0m\n");
    printf(" (ZL) Reload Settings    (ZR) Reset API Token\n");
    printf(" (X)  Check Update       (Y)  Install Update\n");
    printf(" (+)  Exit Application\n");
    printf("\x1b[1;34m----------------------------------------------------\x1b[0m\n");
    
    // Debug info at the bottom to avoid flickering/clearing
    printf("\n\x1b[1m[ DEBUG INFO ]\x1b[0m\n");
    printf("  Loop Count: %llu | Buttons: 0x%016llX    \n", (unsigned long long)loop_count, (unsigned long long)kDown);
}

int main(int argc, char **argv) {
    consoleInit(NULL);
    consoleClear();
    printf("\x1b[1;1H[DEBUG] NRO Booting v" APP_VERSION "...");
    
    PadState pad;
    padInitializeDefault(&pad);
    socketInitializeDefault();
    nxlinkStdio();
    nifmInitialize(NifmServiceType_User);

    ConfigManager::getInstance().load();
    std::string latest_ver = "";
    bool checking_update = false;
    bool nso_checked = false;
    bool sysmodule_active = false;
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

    u64 loop_count = 0;
    u64 kDown = 0;

    while (appletMainLoop()) {
        padUpdate(&pad);
        kDown = padGetButtonsDown(&pad);

        // Check if sysmodule is present ( safeguarding )
        if (!nso_checked) {
            u32 ip = 0;
            if (R_SUCCEEDED(nifmGetCurrentIpAddress(&ip)) && ip != 0) {
                check_and_fix_sysmodule();
                nso_checked = true;
            }
        }

        // Periodically check if sysmodule is actually running
        u64 now = svcGetSystemTick() / 19200000;
        if (now - last_sysmodule_check > 5) { // Every 5 seconds
            sysmodule_active = is_sysmodule_running();
            last_sysmodule_check = now;
        }

        draw_ui(latest_ver, checking_update, sysmodule_active, loop_count++, kDown);

        if (kDown & HidNpadButton_Plus) break;
        if (kDown & HidNpadButton_ZL) { ConfigManager::getInstance().load(); }
        if (kDown & HidNpadButton_ZR) { ConfigManager::getInstance().generateDefaultConfig(); ConfigManager::getInstance().save(); }
        
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
    }

    nifmExit();
    socketExit();
    consoleExit(NULL);
    return 0;
}
