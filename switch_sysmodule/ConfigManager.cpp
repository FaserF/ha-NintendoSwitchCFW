#include "ConfigManager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <new>

// Simple manual JSON-like parsing/saving for sysmodule to avoid large libraries and relocations
static ConfigManager* g_configInstance = NULL;

ConfigManager& ConfigManager::getInstance() {
    if (!g_configInstance) {
        g_configInstance = (ConfigManager*)malloc(sizeof(ConfigManager));
        new (g_configInstance) ConfigManager();
    }
    return *g_configInstance;
}

void ConfigManager::init() {
    getInstance().load();
}

ConfigManager::ConfigManager() : m_port(1337), m_lastUpdateCheck(0) {
    memset(m_apiToken, 0, sizeof(m_apiToken));
}

bool ConfigManager::load() {
    FILE* f = fopen(m_configPath, "r");
    if (!f) {
        generateDefaultConfig();
        return save();
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "\"api_token\"")) {
            char* start = strchr(line, ':');
            if (start) {
                start = strchr(start, '\"');
                if (start) {
                    char* end = strchr(start + 1, '\"');
                    if (end) {
                        size_t len = end - (start + 1);
                        if (len >= sizeof(m_apiToken)) len = sizeof(m_apiToken) - 1;
                        strncpy(m_apiToken, start + 1, len);
                        m_apiToken[len] = '\0';
                    }
                }
            }
        } else if (strstr(line, "\"port\"")) {
            char* start = strchr(line, ':');
            if (start) {
                m_port = atoi(start + 1);
            }
        } else if (strstr(line, "\"last_update_check\"")) {
            char* start = strchr(line, ':');
            if (start) {
                m_lastUpdateCheck = atol(start + 1);
            }
        }
    }
    fclose(f);

    if (m_apiToken[0] == '\0') {
        generateDefaultConfig();
        save();
    }
    return true;
}

bool ConfigManager::save() {
    mkdir("sdmc:/config", 0777);
    mkdir("sdmc:/config/HomeAssistantSwitch", 0777);

    FILE* f = fopen(m_configPath, "w");
    if (!f) return false;

    fprintf(f, "{\n  \"api_token\": \"%s\",\n  \"port\": %d,\n  \"last_update_check\": %ld\n}\n", m_apiToken, m_port, m_lastUpdateCheck);
    fclose(f);
    return true;
}

const char* ConfigManager::getApiToken() { return m_apiToken; }
void ConfigManager::setApiToken(const char* token) { 
    strncpy(m_apiToken, token, sizeof(m_apiToken) - 1);
    m_apiToken[sizeof(m_apiToken) - 1] = '\0';
}

int ConfigManager::getPort() { return m_port; }
void ConfigManager::setPort(int port) { m_port = port; }

long ConfigManager::getLastUpdateCheck() { return m_lastUpdateCheck; }
void ConfigManager::setLastUpdateCheck(long timestamp) { m_lastUpdateCheck = timestamp; }

void ConfigManager::generateDefaultConfig() {
    generatePassphrase(m_apiToken, sizeof(m_apiToken));
    m_port = 1337;
}

void ConfigManager::generatePassphrase(char* out, size_t max_len) {
    static const char* adjectives[] = {"Super", "Magic", "Hyper", "Ultra", "Mega", "Cool", "Smart", "Happy"};
    static const char* nouns[] = {"Switch", "Home", "Dash", "Cloud", "Star", "Link", "Pixel", "Node"};
    
    u64 tick = svcGetSystemTick();
    srand((unsigned int)tick);
    
    snprintf(out, max_len, "%s-%s-%u", adjectives[rand() % 8], nouns[rand() % 8], (unsigned int)(tick % 100));
}

void ConfigManager::generateRandomToken(char* out, size_t length) {
    const char characters[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    u64 tick = svcGetSystemTick();
    srand((unsigned int)tick);

    for (size_t i = 0; i < length; ++i) {
        out[i] = characters[rand() % 62];
    }
    out[length] = '\0';
}
