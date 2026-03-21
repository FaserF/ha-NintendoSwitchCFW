#include <switch.h>
#include <switch/services/set.h>
#include <switch/services/psm.h>
#include <switch/services/ts.h>
#include <switch/services/nifm.h>
#include <switch/services/pdm.h>
#include <switch/services/fs.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <switch/services/bpc.h>
#include <switch/services/ns.h>
#include <switch/services/applet.h>
#include <switch/services/pm.h>
#include <switch/services/hiddbg.h>
#include <fcntl.h>
#include <thread>
#include <string>
#include "include/json.hpp"

using json = nlohmann::json;

extern "C" {
    u32 __check_mask_save;
    
    // Sysmodules must define their own heap
    void __libnx_initheap(void) {
        static char inner_heap[0x800000]; // Increased to 8MB for screenshot buffer
        extern char* fake_heap_start;
        extern char* fake_heap_end;
        fake_heap_start = inner_heap;
        fake_heap_end   = inner_heap + sizeof(inner_heap);
    }
}

// Optimization flags for sysmodules
u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;

extern "C" {
    Result hiddbgSetAutoPilotVirtualPadState(s8 AbstractedVirtualPadId, const HiddbgAbstractedPadState *state);
}

#include "include/ConfigManager.h"
#include "include/Logger.h"

Logger g_logger;

#ifndef IP_ADD_MEMBERSHIP
#define IP_ADD_MEMBERSHIP 12
#endif

// Manually define ip_mreq since it might be missing in some libnx environments
struct switch_ip_mreq {
    struct in_addr imr_multiaddr;
    struct in_addr imr_interface;
};

void mdns_responder() {
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0) return;

    int reuse = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5353);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sd);
        return;
    }

    struct switch_ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    unsigned char buffer[1024];
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int n = recvfrom(sd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len);
        if (n > 12) {
            // Check for "_homeassistant" label in query
            if ((buffer[2] & 0x80) == 0 && strstr((char*)buffer + 12, "_homeassistant")) {
                u32 ip = 0; nifmGetCurrentIpAddress(&ip);
                unsigned char response[] = {
                    0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                    0x06, 's', 'w', 'i', 't', 'c', 'h', 0x05, 'l', 'o', 'c', 'a', 'l', 0x00,
                    0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x04, 
                    (unsigned char)(ip & 0xFF), (unsigned char)((ip >> 8) & 0xFF), 
                    (unsigned char)((ip >> 16) & 0xFF), (unsigned char)((ip >> 24) & 0xFF)
                };
                struct sockaddr_in target_addr;
                target_addr.sin_family = AF_INET;
                target_addr.sin_port = htons(5353);
                target_addr.sin_addr.s_addr = inet_addr("224.0.0.251");
                sendto(sd, response, sizeof(response), 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
            }
        }
        svcSleepThread(500000000ULL); // 0.5s yield
    }
}

void handle_client(int client_sock) {
    struct timeval tv;
    tv.tv_sec = 2; // 2s timeout
    tv.tv_usec = 0;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(client_sock, &readfds);

    if (select(client_sock + 1, &readfds, NULL, NULL, &tv) <= 0) {
        close(client_sock);
        return;
    }

    char buffer[2048] = {0};
    int bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        close(client_sock);
        return;
    }

    bool requires_auth = true;
    if (strncmp(buffer, "GET /info", 9) == 0 || strncmp(buffer, "GET /health", 11) == 0) {
        requires_auth = false;
    }

    if (requires_auth) {
        const char* api_token = ConfigManager::getInstance().getApiToken();
        char token_header[128];
        snprintf(token_header, sizeof(token_header), "X-API-Token: %s", api_token);
        
        if (!strstr(buffer, token_header)) {
            ConfigManager::getInstance().load();
            api_token = ConfigManager::getInstance().getApiToken();
            snprintf(token_header, sizeof(token_header), "X-API-Token: %s", api_token);
            if (!strstr(buffer, token_header)) {
                const char *resp = "HTTP/1.1 401 Unauthorized\r\nContent-Type: application/json\r\n\r\n{\"error\": \"Unauthorized\"}";
                write(client_sock, resp, strlen(resp));
                close(client_sock);
                return;
            }
        }
    }

    if (strstr(buffer, "GET /info")) {
        char response[2048];
        u32 hos_version = 0;
        setsysGetFirmwareVersion((SetSysFirmwareVersion*)&hos_version);
        
        u32 battery_percent = 0;
        psmGetBatteryChargePercentage(&battery_percent);
        u32 charger_type = 0;
        psmGetChargerType((PsmChargerType*)&charger_type);

        s32 cpu_temp = 0, gpu_temp = 0, skin_temp = 0;
        tsGetTemperature(TsLocation_Internal, &cpu_temp);
        gpu_temp = cpu_temp;
        skin_temp = cpu_temp;

        u64 uptime_s = svcGetSystemTick() / 19200000;
        u32 rssi = 0;
        nifmGetInternetConnectionStatus(NULL, &rssi, NULL);

        u64 total_mem = 0, used_mem = 0;
        svcGetInfo(&total_mem, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
        svcGetInfo(&used_mem, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);

        u64 sd_total = 0;
        s64 sd_free = 0;
        FsDeviceOperator fs_op;
        if (R_SUCCEEDED(fsOpenDeviceOperator(&fs_op))) {
            s64 temp_total = 0;
            if (R_SUCCEEDED(fsDeviceOperatorGetSdCardUserAreaSize(&fs_op, &temp_total))) {
                sd_total = (u64)temp_total;
            }
            fsDeviceOperatorClose(&fs_op);
        }

        FsFileSystem sd_fs;
        if (R_SUCCEEDED(fsOpenSdCardFileSystem(&sd_fs))) {
            fsFsGetFreeSpace(&sd_fs, "/", &sd_free);
            fsFsClose(&sd_fs);
        }

        u64 title_id = 0;
        char title_name[512] = "None";
        u64 titles[1];
        s32 total_titles = 0;
        AccountUid uid = {0};
        if (R_SUCCEEDED(pdmqryQueryRecentlyPlayedApplication(uid, false, titles, 1, &total_titles)) && total_titles > 0) {
            title_id = titles[0];
            NsApplicationControlData* controlData = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
            if (controlData) {
                u64 actual_size = 0;
                if (R_SUCCEEDED(nsGetApplicationControlData(NsApplicationControlSource_Storage, title_id, controlData, sizeof(NsApplicationControlData), &actual_size))) {
                    NacpLanguageEntry* langentry = NULL;
                    if (R_SUCCEEDED(nacpGetLanguageEntry(&controlData->nacp, &langentry))) {
                        strncpy(title_name, langentry->name, sizeof(title_name) - 1);
                        title_name[sizeof(title_name) - 1] = '\0';
                    }
                }
                free(controlData);
            }
        }

        AppletOperationMode op_mode = appletGetOperationMode();
        bool is_docked = (op_mode != AppletOperationMode_Handheld);
        bool is_sleeping = false;

        snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
            "{\"firmware_version\": \"%u.%u.%u\", \"app_version\": \"" APP_VERSION "\", \"battery_level\": %u, \"charging\": %s, "
            "\"cpu_temp\": %d, \"gpu_temp\": %d, \"skin_temp\": %d, "
            "\"uptime\": %lu, \"wifi_rssi\": %u, \"mem_total\": %lu, \"mem_used\": %lu, "
            "\"sd_total\": %lu, \"sd_free\": %lu, \"current_title_id\": \"0x%016lX\", "
            "\"current_game\": \"%s\", \"docked\": %s, \"sleep_mode\": %s, \"error_count\": %u}",
            (unsigned int)((hos_version >> 16) & 0xFF), (unsigned int)((hos_version >> 8) & 0xFF), (unsigned int)(hos_version & 0xFF),
            (unsigned int)battery_percent, (charger_type != 0) ? "true" : "false",
            (int)(cpu_temp / 1000), (int)(gpu_temp / 1000), (int)(skin_temp / 1000),
            (unsigned long)uptime_s, (unsigned int)rssi, (unsigned long)total_mem, (unsigned long)used_mem,
            (unsigned long)sd_total, (unsigned long)sd_free, (unsigned long)title_id,
            title_name, (is_docked) ? "true" : "false", (is_sleeping) ? "true" : "false",
            (unsigned int)Logger::getInstance().getErrorCount()
        );
        write(client_sock, response, strlen(response));
        close(client_sock);
        return;
    }

    if (strstr(buffer, "GET /titles")) {
        std::string json_out = "[";
        s32 total_records = 0;
        nsListApplicationRecord(NULL, 0, 0, &total_records);
        if (total_records > 0) {
            NsApplicationRecord* records = (NsApplicationRecord*)malloc(sizeof(NsApplicationRecord) * total_records);
            s32 actual_count = 0;
            if (records && R_SUCCEEDED(nsListApplicationRecord(records, total_records, 0, &actual_count))) {
                bool first = true;
                for (s32 i = 0; i < actual_count; i++) {
                    NsApplicationControlData* controlData = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
                    if (controlData) {
                        u64 actual_size = 0;
                        if (R_SUCCEEDED(nsGetApplicationControlData(NsApplicationControlSource_Storage, records[i].application_id, controlData, sizeof(NsApplicationControlData), &actual_size))) {
                            char title_name[0x201] = {0};
                            NacpLanguageEntry* langentry = NULL;
                            if (R_SUCCEEDED(nacpGetLanguageEntry(&controlData->nacp, &langentry)) && langentry) {
                                snprintf(title_name, sizeof(title_name), "%s", langentry->name);
                            }
                            if (!first) json_out += ",";
                            char entry[1024];
                            snprintf(entry, sizeof(entry), "{\"title_id\": \"0x%016lX\", \"name\": \"%s\"}", (unsigned long)records[i].application_id, title_name);
                            json_out += entry;
                            first = false;
                        }
                        free(controlData);
                    }
                }
            }
            if (records) free(records);
        }
        json_out += "]";
        char resp_head[256];
        snprintf(resp_head, sizeof(resp_head), "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n", json_out.length());
        write(client_sock, resp_head, strlen(resp_head));
        write(client_sock, json_out.c_str(), json_out.length());
        close(client_sock);
        return;
    }

    if (strstr(buffer, "GET /screenshot")) {
        u8* screen_buf = (u8*)malloc(1280 * 720 * 4);
        if (screen_buf) {
            if (R_SUCCEEDED(capsscCaptureRawImageWithTimeout(screen_buf, 1280 * 720 * 4, (ViLayerStack)0, 1280, 720, 1, 0, 1000000000LL))) {
                #pragma pack(push, 1)
                struct BMPHeader {
                    uint16_t bfType;
                    uint32_t bfSize;
                    uint16_t bfReserved1;
                    uint16_t bfReserved2;
                    uint32_t bfOffBits;
                } bmp;
                struct BMPInfoHeader {
                    uint32_t biSize;
                    int32_t  biWidth;
                    int32_t  biHeight;
                    uint16_t biPlanes;
                    uint16_t biBitCount;
                    uint32_t biCompression;
                    uint32_t biSizeImage;
                    int32_t  biXPelsPerMeter;
                    int32_t  biYPelsPerMeter;
                    uint32_t biClrUsed;
                    uint32_t biClrImportant;
                } info;
                #pragma pack(pop)

                memset(&bmp, 0, sizeof(bmp));
                bmp.bfType = 0x4D42;
                bmp.bfSize = sizeof(bmp) + sizeof(info) + (1280 * 720 * 3);
                bmp.bfOffBits = sizeof(bmp) + sizeof(info);
                
                memset(&info, 0, sizeof(info));
                info.biSize = sizeof(info);
                info.biWidth = 1280;
                info.biHeight = -720; // Top-down
                info.biPlanes = 1;
                info.biBitCount = 24;
                info.biCompression = 0;
                info.biSizeImage = 1280 * 720 * 3;

                const char *hdr = "HTTP/1.1 200 OK\r\nContent-Type: image/bmp\r\nConnection: close\r\n\r\n";
                write(client_sock, hdr, strlen(hdr));
                write(client_sock, &bmp, sizeof(bmp));
                write(client_sock, &info, sizeof(info));

                u8* row_buf = (u8*)malloc(1280 * 3);
                if (row_buf) {
                    for (int y = 0; y < 720; y++) {
                        u8* src = &screen_buf[y * 1280 * 4];
                        for (int x = 0; x < 1280; x++) {
                            row_buf[x * 3 + 0] = src[x * 4 + 2]; // B
                            row_buf[x * 3 + 1] = src[x * 4 + 1]; // G
                            row_buf[x * 3 + 2] = src[x * 4 + 0]; // R
                        }
                        write(client_sock, row_buf, 1280 * 3);
                    }
                    free(row_buf);
                }
            } else {
                const char *resp = "HTTP/1.1 500 Internal Server Error\r\n\r\n{\"error\": \"Capture failed\"}";
                write(client_sock, resp, strlen(resp));
            }
            free(screen_buf);
        } else {
            const char *resp = "HTTP/1.1 500 Internal Server Error\r\n\r\n{\"error\": \"OOM\"}";
            write(client_sock, resp, strlen(resp));
        }
        close(client_sock);
        return;
    }

    if (strstr(buffer, "POST /command")) {
        const char* body = strstr(buffer, "\r\n\r\n");
        if (body) {
            body += 4;
            json j = json::parse(body, nullptr, false);
            if (!j.is_discarded() && j.contains("action")) {
                std::string action = j.value("action", "");
                Result rc = 0;
                if (action == "reboot") {
                    bpcRebootSystem();
                } else if (action == "shutdown") {
                    bpcShutdownSystem();
                } else if (action == "launch_app" && j.contains("title_id")) {
                    std::string tid_str = j.value("title_id", "");
                    u64 tid = strtoull(tid_str.c_str(), NULL, 16);
                    rc = appletRequestLaunchApplication(tid, NULL);
                }
                
                std::string resp_body = "{\"status\": \"ok\", \"rc\": " + std::to_string(rc) + "}";
                char resp[256];
                snprintf(resp, sizeof(resp), "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n", resp_body.length());
                write(client_sock, resp, strlen(resp));
                write(client_sock, resp_body.c_str(), resp_body.length());
            } else {
                const char* resp = "HTTP/1.1 400 Bad Request\r\n\r\n{\"error\": \"Missing action\"}";
                write(client_sock, resp, strlen(resp));
            }
        }
        close(client_sock);
        return;
    }

    if (strstr(buffer, "GET /logs")) {
        u32 count = Logger::getInstance().getLogCount();
        std::string json_out = "[";
        for (u32 i = 0; i < count; i++) {
            if (i > 0) json_out += ",";
            const LogEntry* log = Logger::getInstance().getLog(i);
            char entry[1024];
            const char* level_str = (log->level == LOG_LEVEL_INFO) ? "INFO" : (log->level == LOG_LEVEL_WARN) ? "WARN" : "ERROR";
            snprintf(entry, sizeof(entry), "{\"level\": \"%s\", \"message\": \"%s\", \"timestamp\": \"%s\"}", 
                     level_str, log->message, log->timestamp);
            json_out += entry;
        }
        json_out += "]";
        char resp_head[256];
        snprintf(resp_head, sizeof(resp_head), "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n", json_out.length());
        write(client_sock, resp_head, strlen(resp_head));
        write(client_sock, json_out.c_str(), json_out.length());
        close(client_sock);
        return;
    }

    if (strstr(buffer, "POST /reboot")) {
        LOG_I("System reboot requested");
        const char *resp = "HTTP/1.1 200 OK\r\n\r\n{\"status\": \"ok\"}";
        write(client_sock, resp, strlen(resp));
        bpcInitialize(); bpcRebootSystem(); bpcExit();
    } else if (strstr(buffer, "POST /shutdown")) {
        LOG_I("System shutdown requested");
        const char *resp = "HTTP/1.1 200 OK\r\n\r\n{\"status\": \"ok\"}";
        write(client_sock, resp, strlen(resp));
        bpcInitialize(); bpcShutdownSystem(); bpcExit();
    } else if (strstr(buffer, "POST /sleep")) {
        LOG_I("System sleep requested");
        const char *resp = "HTTP/1.1 200 OK\r\n\r\n{\"status\": \"ok\"}";
        write(client_sock, resp, strlen(resp));
        appletRequestToSleep();
    } else if (strstr(buffer, "POST /reload_config")) {
        ConfigManager::getInstance().load();
        LOG_I("Config reloaded");
        const char *resp = "HTTP/1.1 200 OK\r\n\r\n{\"status\": \"ok\"}";
        write(client_sock, resp, strlen(resp));
    } else if (strstr(buffer, "POST /launch")) {
        char* id_ptr = strstr(buffer, "title_id=0x");
        if (id_ptr) {
            u64 tid = strtoull(id_ptr + 9, NULL, 16);
            const char *resp = "HTTP/1.1 200 OK\r\n\r\n{\"status\": \"ok\"}";
            write(client_sock, resp, strlen(resp));
            char log_msg[128];
            snprintf(log_msg, sizeof(log_msg), "Launching title ID: 0x%016lX", (unsigned long)tid);
            LOG_I(log_msg);
            pmshellInitialize();
            NcmProgramLocation loc = {tid, NcmStorageId_None, {0}};
            if (R_FAILED(pmshellLaunchProgram(0, &loc, NULL))) {
                snprintf(log_msg, sizeof(log_msg), "Failed to launch title ID: 0x%016lX", (unsigned long)tid);
                LOG_E(log_msg);
            }
            pmshellExit();
        } else {
            const char *resp = "HTTP/1.1 400 Bad Request\r\n\r\n";
            write(client_sock, resp, strlen(resp));
        }
    } else if (strstr(buffer, "POST /update_app")) {
        const char *resp = "HTTP/1.1 200 OK\r\n\r\n{\"status\": \"ok\", \"message\": \"App update triggered\"}";
        write(client_sock, resp, strlen(resp));
        bpcInitialize(); bpcRebootSystem(); bpcExit();
    } else if (strstr(buffer, "POST /button")) {
        char* body = strstr(buffer, "\r\n\r\n");
        if (body) {
            body += 4;
            if (body[0] == '{') {
                json req = json::parse(body, nullptr, false);
                if (!req.is_discarded() && req.contains("sequence") && req["sequence"].is_array()) {
                    for (auto& step : req["sequence"]) {
                        std::string btn_name = step.value("button", "");
                        int duration = step.value("duration_ms", 100);
                        u64 btn_bit = 0;
                        if (btn_name == "A") btn_bit = HidNpadButton_A;
                        else if (btn_name == "B") btn_bit = HidNpadButton_B;
                        else if (btn_name == "X") btn_bit = HidNpadButton_X;
                        else if (btn_name == "Y") btn_bit = HidNpadButton_Y;
                        else if (btn_name == "L") btn_bit = HidNpadButton_L;
                        else if (btn_name == "R") btn_bit = HidNpadButton_R;
                        else if (btn_name == "ZL") btn_bit = HidNpadButton_ZL;
                        else if (btn_name == "ZR") btn_bit = HidNpadButton_ZR;
                        else if (btn_name == "PLUS") btn_bit = HidNpadButton_Plus;
                        else if (btn_name == "MINUS") btn_bit = HidNpadButton_Minus;
                        else if (btn_name == "LEFT") btn_bit = HidNpadButton_Left;
                        else if (btn_name == "RIGHT") btn_bit = HidNpadButton_Right;
                        else if (btn_name == "UP") btn_bit = HidNpadButton_Up;
                        else if (btn_name == "DOWN") btn_bit = HidNpadButton_Down;
                        else if (btn_name == "HOME") btn_bit = HiddbgNpadButton_Home;
                        else if (btn_name == "CAPTURE") btn_bit = HiddbgNpadButton_Capture;
                        
                        if (btn_bit != 0) {
                            HiddbgAbstractedPadState state;
                            memset(&state, 0, sizeof(state));
                            state.state.buttons = btn_bit;
                            hiddbgSetAutoPilotVirtualPadState(0, &state);
                            svcSleepThread(duration * 1000000ULL);
                            state.state.buttons = 0;
                            hiddbgSetAutoPilotVirtualPadState(0, &state);
                            svcSleepThread(50000000ULL); // 50ms pause
                        }
                    }
                    const char *resp = "HTTP/1.1 200 OK\r\n\r\n{\"status\": \"ok\"}";
                    write(client_sock, resp, strlen(resp));
                    close(client_sock);
                    return;
                }
            }
        }

        char* name_ptr = strstr(buffer, "name=");
        if (name_ptr) {
            char btn_name[32] = {0};
            sscanf(name_ptr + 5, "%31s", btn_name);
            // Remove potential & or space if it's there
            char* end = strpbrk(btn_name, " \r\n&");
            if (end) *end = '\0';

            u64 btn_bit = 0;
            if (strcmp(btn_name, "A") == 0) btn_bit = HidNpadButton_A;
            else if (strcmp(btn_name, "B") == 0) btn_bit = HidNpadButton_B;
            else if (strcmp(btn_name, "X") == 0) btn_bit = HidNpadButton_X;
            else if (strcmp(btn_name, "Y") == 0) btn_bit = HidNpadButton_Y;
            else if (strcmp(btn_name, "L") == 0) btn_bit = HidNpadButton_L;
            else if (strcmp(btn_name, "R") == 0) btn_bit = HidNpadButton_R;
            else if (strcmp(btn_name, "ZL") == 0) btn_bit = HidNpadButton_ZL;
            else if (strcmp(btn_name, "ZR") == 0) btn_bit = HidNpadButton_ZR;
            else if (strcmp(btn_name, "PLUS") == 0) btn_bit = HidNpadButton_Plus;
            else if (strcmp(btn_name, "MINUS") == 0) btn_bit = HidNpadButton_Minus;
            else if (strcmp(btn_name, "LEFT") == 0) btn_bit = HidNpadButton_Left;
            else if (strcmp(btn_name, "RIGHT") == 0) btn_bit = HidNpadButton_Right;
            else if (strcmp(btn_name, "UP") == 0) btn_bit = HidNpadButton_Up;
            else if (strcmp(btn_name, "DOWN") == 0) btn_bit = HidNpadButton_Down;
            else if (strcmp(btn_name, "HOME") == 0) btn_bit = HiddbgNpadButton_Home;
            else if (strcmp(btn_name, "CAPTURE") == 0) btn_bit = HiddbgNpadButton_Capture;

            if (btn_bit != 0) {
                HiddbgAbstractedPadState state;
                memset(&state, 0, sizeof(state));
                state.state.buttons = btn_bit;
                hiddbgSetAutoPilotVirtualPadState(0, &state);
                svcSleepThread(100000000ULL); // 100ms hold
                state.state.buttons = 0;
                hiddbgSetAutoPilotVirtualPadState(0, &state);
                
                const char *resp = "HTTP/1.1 200 OK\r\n\r\n{\"status\": \"ok\"}";
                write(client_sock, resp, strlen(resp));
            } else {
                const char *resp = "HTTP/1.1 400 Bad Request\r\n\r\n{\"error\": \"Invalid button\"}";
                write(client_sock, resp, strlen(resp));
            }
        } else {
            const char *resp = "HTTP/1.1 400 Bad Request\r\n\r\n{\"error\": \"Button name missing\"}";
            write(client_sock, resp, strlen(resp));
        }
    } else {
        const char *resp = "HTTP/1.1 404 Not Found\r\n\r\n";
        write(client_sock, resp, strlen(resp));
    }
    close(client_sock);
}

int main(int, char **) {
    setsysInitialize();
    psmInitialize();
    tsInitialize();
    nifmInitialize(NifmServiceType_User);
    pdmqryInitialize();
    nsInitialize();
    appletInitialize();
    hiddbgInitialize();
    capsscInitialize();
    
    std::thread(mdns_responder).detach();
    
    g_logger.init();
    LOG_I("Home Assistant Sysmodule started (v" APP_VERSION ")");
    
    // Heartbeat log for physical Switch troubleshooting
    FILE *f = fopen("sdmc:/ha_sysmodule_boot.log", "a");
    if (f) {
        time_t t = time(NULL);
        fprintf(f, "[%ld] Sysmodule Main Started\n", t);
        fclose(f);
    }

    ConfigManager::getInstance().load();

    const SocketInitConfig* default_cfg = socketGetDefaultInitConfig();
    SocketInitConfig sock_cfg = *default_cfg;
    sock_cfg.bsd_service_type = BsdServiceType_System;

    if (R_FAILED(socketInitialize(&sock_cfg))) {
        FILE *f = fopen("sdmc:/ha_sysmodule_boot.log", "a");
        if (f) { fprintf(f, "[ERR] socketInitialize failed\n"); fclose(f); }
        LOG_E("Failed to initialize sockets");
        return 1;
    }

    int server_fd, client_sock;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG_E("Failed to create socket");
        socketExit();
        return 1;
    }
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(ConfigManager::getInstance().getPort());
    address.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        FILE *f = fopen("sdmc:/ha_sysmodule_boot.log", "a");
        if (f) { fprintf(f, "[ERR] bind failed: %d\n", errno); fclose(f); }
        LOG_E("Failed to bind socket");
        return 1;
    }
    if (listen(server_fd, 5) < 0) {
        FILE *f = fopen("sdmc:/ha_sysmodule_boot.log", "a");
        if (f) { fprintf(f, "[ERR] listen failed\n"); fclose(f); }
        LOG_E("Failed to listen on socket");
        return 1;
    }
    
    {
        FILE *f = fopen("sdmc:/ha_sysmodule_boot.log", "a");
        if (f) { fprintf(f, "[OK] Server listening on port %d\n", ConfigManager::getInstance().getPort()); fclose(f); }
    }

    // Power Optimization: Leave server_fd in blocking mode. 
    // This allows the server thread to spend most of its time suspended, saving battery.
    
    while (true) {
        if ((client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0) {
            std::thread(handle_client, client_sock).detach();
        }
    }

    capsscExit();
    hiddbgExit();
    appletExit();
    nsExit();
    pdmqryExit();
    nifmExit();
    tsExit();
    setsysExit();
    psmExit();
    socketExit();
    return 0;
}
