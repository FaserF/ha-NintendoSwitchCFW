#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#ifdef UNIT_TEST
#include "MockLibnx.h"
#else
#include <switch.h>
#endif

class ConfigManager {
public:
    static ConfigManager& getInstance();
    static void init();

    bool load();
    bool save();

    const char* getApiToken();
    void setApiToken(const char* token);

    int getPort();
    void setPort(int port);

    long getLastUpdateCheck();
    void setLastUpdateCheck(long timestamp);

    void generateDefaultConfig();
    void generatePassphrase(char* out, size_t max_len);
    
    void setConfigPath(const char* path) { m_configPath = path; }

private:
    ConfigManager();
    char m_apiToken[128];
    int m_port;
    long m_lastUpdateCheck;
    const char* m_configPath = "sdmc:/config/HomeAssistantSwitch/settings.json";

    void generateRandomToken(char* out, size_t length);
};

#endif
