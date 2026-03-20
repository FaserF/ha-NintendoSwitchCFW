"""Constants for Nintendo Switch CFW integration."""

import logging

DOMAIN = "switch_cfw"
LOGGER = logging.getLogger(DOMAIN)

STARTUP_MESSAGE = f"""
-------------------------------------------------------------------
{DOMAIN}
Version: 0.1.6
This is a custom integration!
If you have any issues with this you need to open an issue here:
https://github.com/FaserF/ha-NintentdoSwitchCFW
-------------------------------------------------------------------
"""

DEFAULT_PORT = 1337

# Configuration
CONF_API_TOKEN = "api_token"
CONF_UPDATE_REPO = "update_repo"

# Attributes from Sysmodule
ATTR_FIRMWARE_VERSION = "0.1.6"
ATTR_BATTERY_LEVEL = "battery_level"
ATTR_CHARGING = "charging"
ATTR_CURRENT_GAME = "current_game"
ATTR_CPU_TEMP = "cpu_temp"
ATTR_GPU_TEMP = "gpu_temp"
ATTR_SKIN_TEMP = "skin_temp"
ATTR_FAN_SPEED = "fan_speed"
ATTR_SD_TOTAL = "sd_total"
ATTR_SD_FREE = "sd_free"
ATTR_NAND_TOTAL = "nand_total"
ATTR_NAND_FREE = "nand_free"
ATTR_UPTIME = "uptime"
ATTR_WIFI_RSSI = "wifi_rssi"
ATTR_MEM_TOTAL = "mem_total"
ATTR_MEM_USED = "mem_used"
ATTR_SLEEP_MODE = "sleep_mode"
ATTR_ERROR_COUNT = "error_count"
ATTR_LOGS = "logs"
ATTR_DOCK_STATUS = "dock_status"

# Attributes for HA
ATTR_LATEST_VERSION = "0.1.6"
ATTR_TITLE_ID = "current_title_id"
ATTR_APP_VERSION = "0.1.6"

# Minimum required version of the Homebrew App/Sysmodule
MIN_APP_VERSION = "0.1.6"

FIRMWARE_UPDATE_URL = "https://api.github.com/repos/{repository}/releases/latest"
GITHUB_RELEASE_URL = "https://github.com/FaserF/ha-NintentdoSwitchCFW/releases/latest"
