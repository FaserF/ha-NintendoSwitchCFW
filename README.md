<div align="center">
  <img src="https://raw.githubusercontent.com/FaserF/ha-NintendoSwitchCFW/main/logo.png" alt="Home Assistant Switch Logo" width="150">
</div>

# Nintendo Switch CFW (for Home Assistant)

[![GitHub Release](https://img.shields.io/github/release/FaserF/ha-NintendoSwitchCFW.svg?style=flat-square)](https://github.com/FaserF/ha-NintendoSwitchCFW/releases)
[![License](https://img.shields.io/github/license/FaserF/ha-NintendoSwitchCFW.svg?style=flat-square)](LICENSE)
[![hacs](https://img.shields.io/badge/HACS-custom-orange.svg?style=flat-square)](https://hacs.xyz)
[![Lint](https://github.com/FaserF/ha-NintendoSwitchCFW/actions/workflows/lint.yml/badge.svg)](https://github.com/FaserF/ha-NintendoSwitchCFW/actions/workflows/lint.yml)

A modern, high-quality Home Assistant integration for Nintendo Switch consoles running Atmosphere Custom Firmware. Monitor your console's health, track current games, and execute system commands directly from your dashboard.

## 🧭 Quick Links

| | | | |
| :--- | :--- | :--- | :--- |
| [✨ Features](#-features) | [📦 Installation](#-installation) | [⚙️ Configuration](#️-configuration) | [🤖 Automations](#-automation-examples) |
| [🛡️ Security](#️-security) | [🧑‍💻 Development](#-development--releases) | [📄 License](#-license) | |

## ✨ Features

- **System Monitoring**:
  - **Firmware Version**: Tracks the currently installed HOS version.
  - **Battery Level**: Precise battery percentage sensor.
  - **Charging Status**: Binary sensor for charging/not charging.
  - **Dock Status**: Detects if the console is docked or in handheld mode.
  - **Sleep Mode**: Detects if the console is in sleep mode (keeps last known values active).
  - **Temperatures**: CPU, GPU, and Skin temperature sensors (disabled by default).
  - **Fan Speed**: Real-time fan RPM monitoring (disabled by default).
  - **Storage**: Monitor free/total space on SD Card and NAND (disabled by default).
- **Game Management**:
  - **Current Game**: Shows the title of the game currently running.
  - **Title ID**: Attribute for the current game's Title ID.
  - **Remote Launch**: Select and launch any installed game directly from your dashboard (wakes the console if needed).
- **Remote Control & Input**:
  - **Remote Entity**: A full Home Assistant `remote` platform to send controller button presses (A, B, X, Y, Home, etc.) in the background.
  - **Reboot**: Button to reboot the console (optionally to a specific payload).
  - **Shutdown**: Button to safely power off the console.
- **Update Management**:
  - **System Update**: Notifies you when a new firmware version is available.
  - **Homebrew App Update**: Monitor and trigger automatic updates for the `ha-switch-sysmodule` and companion app directly from HA.
  - **Daybreak Integration**: Trigger firmware updates directly via Daybreak (experimental).
- **Modern Standards**:
  - Full support for **Auto-discovery** via Zeroconf/mDNS.
  - **Device Bundling**: All entities grouped under a single Switch device.
  - **Configuration URL**: "Visit" button links directly to the sysmodule status API.
  - **Localization**: English and German translations included.
  - **Energy Efficient**: Optimized polling and dynamic interval during sleep.

## ❤️ Support This Project

> I maintain this integration in my **free time alongside my regular job** — bug hunting, new features, testing on real devices. Test hardware costs money, and every donation helps me stay independent and dedicate more time to open-source work.
>
> **This project is and will always remain 100% free.** There are no "Premium Upgrades", paid features, or subscriptions. Every feature is available to everyone.
>
> Donations are completely voluntary — but the more support I receive, the less I depend on other income sources and the more time I can realistically invest into these projects. 💪

<div align="center">

[![GitHub Sponsors](https://img.shields.io/badge/Sponsor%20on-GitHub-%23EA4AAA?style=for-the-badge&logo=github-sponsors&logoColor=white)](https://github.com/sponsors/FaserF)&nbsp;&nbsp;
[![PayPal](https://img.shields.io/badge/Donate%20via-PayPal-%2300457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/FaserF)

</div>

### Firmware Updates
The integration checks for the latest firmware versions. By default, it uses [THZoria/NX_Firmware](https://github.com/THZoria/NX_Firmware).

> [!TIP]
> You can change the update repository in the **Integration Options**.

> [!WARNING]
> Support is only provided for the default repository. Custom repositories must follow a similar release structure (tag names as versions, `.zip` or `.nro` assets) to be compatible.

## 🤝 Sponsoring

## 🎮 Supported Hardware

This integration is designed for Nintendo Switch consoles running Atmosphere Custom Firmware.

| Model | Supported | Notes |
| :--- | :---: | :--- |
| **Nintendo Switch V1 (Erista)** | ✅ | **Primary Test Platform.** Full support. |
| **Nintendo Switch V2 (Mariko)** | ⚠️ | Should work with a modchip, but not actively tested. |
| **Nintendo Switch Lite** | ⚠️ | Should work with a modchip, but not actively tested. |
| **Nintendo Switch OLED** | ⚠️ | Should work with a modchip, but not actively tested. |

> [!IMPORTANT]
> The developer primarily tests this integration on a **Nintendo Switch V1**. While it should theoretically work on all models running Atmosphere, support and troubleshooting will be prioritized for the V1 model.

## 📦 Installation

### 1. Nintendo Switch (Sysmodule & Config App)

To allow Home Assistant to communicate with your Switch, you must install the background sysmodule and the companion configuration app.
1. Download the latest `main` (background service), `main.npdm` (boot descriptor) and `homeassistant.nro` (config app) from the [Releases page](https://github.com/FaserF/ha-NintendoSwitchCFW/releases).
2. On your SD card, create the folder: `/atmosphere/contents/4200000000000001/exefs/`.
3. Copy `main` and `main.npdm` to: `/atmosphere/contents/4200000000000001/exefs/`.
4. Copy `homeassistant.nro` to: `/switch/homeassistant.nro`.
5. Reboot your Switch.

### ❓ Why both an .NSO and an .NRO?

Users often ask why this integration requires two separate files. This is due to technical limitations of the Nintendo Switch Operating System (Horizon):

- **The NSO (Sysmodule)**: This is a background service. Standard Switch applications (.nro) are automatically suspended or closed when you launch a game. To allow Home Assistant to monitor your Switch **while you are playing**, a sysmodule is required as it runs in the background at all times.
- **The NRO (Config App)**: Sysmodules cannot have a graphical user interface (GUI). The NRO provides a user-friendly way to see your console's IP address, generate secure API tokens, and check the status of the background service without having to manually edit configuration files on your SD card.

Together, they provide both the persistent background connectivity and a simple setup experience.

### 2. Home Assistant (Integration)

#### HACS (Recommended)

This integration is fully compatible with [HACS](https://hacs.xyz/).

[![Open your Home Assistant instance and open a repository inside the Home Assistant Community Store.](https://my.home-assistant.io/badges/hacs_repository.svg)](https://my.home-assistant.io/redirect/hacs_repository/?repository=FaserF/ha-NintendoSwitchCFW&category=integration)

1. Open HACS in Home Assistant.
2. Click on the three dots in the top right corner and select **Custom repositories**.
3. Add `FaserF/ha-NintendoSwitchCFW` with category **Integration**.
4. Search for "Nintendo Switch CFW" and install.
5. Restart Home Assistant.

### Manual Installation

1. Download the latest release from the [Releases page](https://github.com/FaserF/ha-NintendoSwitchCFW/releases).
2. Extract the `custom_components/switch_cfw` folder into your Home Assistant's `custom_components` directory.
3. Restart Home Assistant.

## ⚙️ Configuration

1. Navigate to **Settings > Devices & Services**.
2. If your Switch is on the same network, it should be **automatically discovered**. Click **Configure**.
3. If not discovered, click **Add Integration** and search for **Nintendo Switch CFW**.
4. Enter the IP address and the **API Token** shown in the Switch app.

## 🛡️ Security

The connection between Home Assistant and the Switch is secured via a **secure API Token**.

- On first launch, the Switch app generates a unique random token.
- This token is saved in `sdmc:/config/HomeAssistantSwitch/settings.json`.
- The background service validates every request against this token.
- You can regenerate the token at any time within the Switch app.

## 🤖 Automation Examples

<details>
<summary><b>Notify when Battery is Low (< 15%)</b></summary>

```yaml
alias: "Switch: Low Battery Notification"
trigger:
  - platform: numeric_state
    entity_id: sensor.nintendo_switch_battery_level
    below: 15
condition:
  - condition: state
    entity_id: binary_sensor.nintendo_switch_charging
    state: "off"
action:
  - service: notify.mobile_app_your_phone
    data:
      title: "Nintendo Switch"
      message: "Battery is low! Please dock the console."
```
</details>

<details>
<summary><b>Alert when a Specific Game is started</b></summary>

```yaml
alias: "Switch: Mario Kart Time!"
trigger:
  - platform: state
    entity_id: sensor.nintendo_switch_current_game
    to: "Mario Kart 8 Deluxe"
action:
  - service: light.turn_on
    target:
      entity_id: light.gaming_room_leds
    data:
      color_name: red
```
</details>

<details>
<summary><b>Notify on Firmware Update</b></summary>

```yaml
alias: "Switch: Firmware Update Available"
trigger:
  - platform: state
    entity_id: update.nintendo_switch_firmware
    to: "on"
action:
  - service: notify.persistent_notification
    data:
      title: "Switch Update"
      message: "A new HOS version {{ state_attr('update.nintendo_switch_firmware', 'latest_version') }} is available!"
```
</details>

<details>
<summary><b>Turn off Lights when Switch is Shutdown</b></summary>

```yaml
alias: "Switch: Power Off Scene"
trigger:
  - platform: state
    entity_id: binary_sensor.nintendo_switch_sleep_mode
    to: "on"
action:
  - service: light.turn_off
    target:
      entity_id: light.living_room
```
</details>

<details>
<summary><b>Pause Media when Switch is Shutdown</b></summary>

```yaml
alias: "Switch: Multi-Command Remote Macro"
trigger:
  - platform: state
    entity_id: sensor.nintendo_switch_current_game
    to: "YouTube"
action:
  - service: remote.send_command
    target:
      entity_id: remote.nintendo_switch
    data:
      command:
        - "UP"
        - "UP"
        - "A"
```
</details>

## 🧑‍💻 Development & Releases

This project uses an automated release workflow.
- Releases are tagged automatically.
- The workflow builds the C++ components (NSP & NRO) using DevkitPro.
- Packages the HA integration and generates a dynamic changelog.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
