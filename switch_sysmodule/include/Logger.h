#pragma once

#include <switch.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAX_LOG_MESSAGE_LEN 256
#define MAX_LOG_ENTRIES 50

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

typedef struct {
    LogLevel level;
    char message[MAX_LOG_MESSAGE_LEN];
    char timestamp[32];
} LogEntry;

class Logger {
public:
    void init() {
        m_head = 0;
        m_count = 0;
        m_errorCount = 0;
        memset(m_logs, 0, sizeof(m_logs));
    }

    void log(LogLevel level, const char* message) {
        if (m_count >= 0xFFFFFFF0) return; // Safeguard

        LogEntry* entry = &m_logs[m_head];
        entry->level = level;
        strncpy(entry->message, message, MAX_LOG_MESSAGE_LEN - 1);
        entry->message[MAX_LOG_MESSAGE_LEN - 1] = '\0';

        time_t now = time(0);
        struct tm* tm_info = localtime(&now);
        if (tm_info) {
            strftime(entry->timestamp, sizeof(entry->timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
        } else {
            strcpy(entry->timestamp, "unknown");
        }

        m_head = (m_head + 1) % MAX_LOG_ENTRIES;
        if (m_count < MAX_LOG_ENTRIES) {
            m_count++;
        }

        if (level == LOG_LEVEL_ERROR) {
            m_errorCount++;
        }
    }

    u32 getErrorCount() const { return m_errorCount; }
    u32 getLogCount() const { return m_count; }

    const LogEntry* getLog(u32 index) const {
        if (index >= m_count) return NULL;
        u32 actual_index = (m_head - m_count + index + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
        return &m_logs[actual_index];
    }

    static Logger& getInstance();

private:
    LogEntry m_logs[MAX_LOG_ENTRIES];
    u32 m_head;
    u32 m_count;
    u32 m_errorCount;
};

extern Logger g_logger;

inline Logger& Logger::getInstance() {
    return g_logger;
}

#define LOG_I(msg) Logger::getInstance().log(LOG_LEVEL_INFO, msg)
#define LOG_W(msg) Logger::getInstance().log(LOG_LEVEL_WARN, msg)
#define LOG_E(msg) Logger::getInstance().log(LOG_LEVEL_ERROR, msg)
