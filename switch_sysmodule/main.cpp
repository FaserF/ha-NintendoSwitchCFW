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
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <switch/services/bpc.h>
#include <switch/services/ns.h>
#include <switch/services/applet.h>
#include <switch/services/pm.h>
#include <switch/services/hiddbg.h>
#include <fcntl.h>

extern "C" {
    u32 __check_mask_save;
    
    // Sysmodules must define their own heap
    void __libnx_initheap(void) {
        static char inner_heap[0x400000]; // Increased to 4MB heap for stability
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
        char* json_out = (char*)malloc(16384);
        strcpy(json_out, "[");
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
                            if (!first) strcat(json_out, ",");
                            char entry[1024];
                            snprintf(entry, sizeof(entry), "{\"title_id\": \"0x%016lX\", \"name\": \"%s\"}", (unsigned long)records[i].application_id, title_name);
                            strcat(json_out, entry);
                            first = false;
                        }
                        free(controlData);
                    }
                }
            }
            if (records) free(records);
        }
        strcat(json_out, "]");
        char resp_head[256];
        snprintf(resp_head, sizeof(resp_head), "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n", strlen(json_out));
        write(client_sock, resp_head, strlen(resp_head));
        write(client_sock, json_out, strlen(json_out));
        free(json_out);
        close(client_sock);
        return;
    }

    if (strstr(buffer, "GET /logs")) {
        u32 count = Logger::getInstance().getLogCount();
        char* json_out = (char*)malloc(16384);
        strcpy(json_out, "[");
        for (u32 i = 0; i < count; i++) {
            if (i > 0) strcat(json_out, ",");
            const LogEntry* log = Logger::getInstance().getLog(i);
            char entry[1024];
            const char* level_str = (log->level == LOG_LEVEL_INFO) ? "INFO" : (log->level == LOG_LEVEL_WARN) ? "WARN" : "ERROR";
            snprintf(entry, sizeof(entry), "{\"level\": \"%s\", \"message\": \"%s\", \"timestamp\": \"%s\"}", 
                     level_str, log->message, log->timestamp);
            strcat(json_out, entry);
        }
        strcat(json_out, "]");
        char resp_head[256];
        snprintf(resp_head, sizeof(resp_head), "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n", strlen(json_out));
        write(client_sock, resp_head, strlen(resp_head));
        write(client_sock, json_out, strlen(json_out));
        free(json_out);
        close(client_sock);
        return;
    }

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
            NcmProgramLocation loc = {tid, NcmStorageId_None};
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
                HiddbgAbstractedPadState state = {0};
                state.state.buttons = btn_bit;
                hiddbgSetAutoPilotVirtualPadState(0, &state);
                usleep(100000); // 100ms hold
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

int main(int argc, char **argv) {
    setsysInitialize();
    psmInitialize();
    tsInitialize();
    nifmInitialize(NifmServiceType_User);
    pdmqryInitialize();
    nsInitialize();
    appletInitialize();
    hiddbgInitialize();
    
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

    // Make server_fd non-blocking
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    int mdns_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (mdns_sock < 0) {
        LOG_E("Failed to create mDNS socket");
    }
    struct sockaddr_in mdns_addr;
    mdns_addr.sin_family = AF_INET;
    mdns_addr.sin_port = htons(5353);
    mdns_addr.sin_addr.s_addr = inet_addr("224.0.0.251");

    u64 last_mdns = 0;

    while (true) {
        u64 now = svcGetSystemTick() / 19200000;
        if (now - last_mdns > 10) { // Every 10 seconds
            unsigned char packet[] = {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x06, 's', 'w', 'i', 't', 'c', 'h', 0x05, 'l', 'o', 'c', 'a', 'l', 0x00,
                0x00, 0x0c, 0x00, 0x01
            };
            if (mdns_sock >= 0) {
                sendto(mdns_sock, packet, sizeof(packet), 0, (struct sockaddr *)&mdns_addr, sizeof(mdns_addr));
            }
            char heartbeat[64];
            snprintf(heartbeat, sizeof(heartbeat), "Sysmodule Heartbeat (Up: %lus)", (unsigned long)now);
            Logger::getInstance().log(LOG_LEVEL_INFO, heartbeat);
            last_mdns = now;
        }

        if ((client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0) {
            handle_client(client_sock);
        }
        svcSleepThread(10000000ULL); // 10ms sleep
    }

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
