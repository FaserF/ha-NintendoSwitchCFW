"""Sensor platform for Nintendo Switch CFW."""

from __future__ import annotations
from typing import Any, cast

from homeassistant.components.sensor import (
    SensorDeviceClass,
    SensorEntity,
    SensorStateClass,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import (
    PERCENTAGE,
    UnitOfTemperature,
    UnitOfInformation,
    UnitOfTime,
    SIGNAL_STRENGTH_DECIBELS_MILLIWATT,
)
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import (
    DOMAIN,
    ATTR_FIRMWARE_VERSION,
    ATTR_BATTERY_LEVEL,
    ATTR_CURRENT_GAME,
    ATTR_CPU_TEMP,
    ATTR_GPU_TEMP,
    ATTR_SKIN_TEMP,
    ATTR_FAN_SPEED,
    ATTR_SD_TOTAL,
    ATTR_SD_FREE,
    ATTR_NAND_TOTAL,
    ATTR_NAND_FREE,
    ATTR_UPTIME,
    ATTR_WIFI_RSSI,
    ATTR_MEM_TOTAL,
    ATTR_MEM_USED,
    ATTR_ERROR_COUNT,
)
from .coordinator import SwitchDataUpdateCoordinator
from .entity import SwitchEntity


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the sensor platform."""
    coordinator: SwitchDataUpdateCoordinator = hass.data[DOMAIN][entry.entry_id]

    entities = [
        FirmwareSensor(coordinator),
        BatterySensor(coordinator),
        CurrentGameSensor(coordinator),
        # Advanced sensors (disabled by default)
        TemperatureSensor(coordinator, ATTR_CPU_TEMP, "CPU Temperature"),
        TemperatureSensor(coordinator, ATTR_GPU_TEMP, "GPU Temperature"),
        TemperatureSensor(coordinator, ATTR_SKIN_TEMP, "Skin Temperature"),
        FanSpeedSensor(coordinator),
        StorageSensor(coordinator, ATTR_SD_FREE, ATTR_SD_TOTAL, "sd_card_storage"),
        StorageSensor(
            coordinator, ATTR_NAND_FREE, ATTR_NAND_TOTAL, "nand_user_storage"
        ),
        UptimeSensor(coordinator),
        WifiRSSISensor(coordinator),
        MemorySensor(coordinator),
        LogSensor(coordinator),
    ]

    async_add_entities(entities)


class FirmwareSensor(SwitchEntity, SensorEntity):
    """Sensor for Nintendo Switch firmware version."""

    _attr_translation_key = "firmware_version"
    _attr_icon = "mdi:chip"

    @property
    def native_value(self) -> str | None:
        val = self.coordinator.data.get(ATTR_FIRMWARE_VERSION)
        return str(val) if val is not None else None


class BatterySensor(SwitchEntity, SensorEntity):
    """Sensor for Nintendo Switch battery level."""

    _attr_translation_key = "battery_level"
    _attr_device_class = SensorDeviceClass.BATTERY
    _attr_state_class = SensorStateClass.MEASUREMENT
    _attr_native_unit_of_measurement = PERCENTAGE

    @property
    def native_value(self) -> int | None:
        val = self.coordinator.data.get(ATTR_BATTERY_LEVEL)
        return int(val) if val is not None else None


class CurrentGameSensor(SwitchEntity, SensorEntity):
    """Sensor for current game on Nintendo Switch."""

    _attr_translation_key = "current_game"
    _attr_icon = "mdi:controller"

    @property
    def native_value(self) -> str | None:
        val = self.coordinator.data.get(ATTR_CURRENT_GAME)
        return str(val) if val is not None else None


class TemperatureSensor(SwitchEntity, SensorEntity):
    """Sensor for Nintendo Switch temperatures."""

    _attr_device_class = SensorDeviceClass.TEMPERATURE
    _attr_state_class = SensorStateClass.MEASUREMENT
    _attr_native_unit_of_measurement = UnitOfTemperature.CELSIUS
    _attr_entity_registry_enabled_default = False

    def __init__(
        self, coordinator: SwitchDataUpdateCoordinator, attr: str, name: str
    ) -> None:
        super().__init__(coordinator)
        self._attr = attr
        self._attr_name = name
        self._attr_unique_id = f"{coordinator.host}_{attr}"

    @property
    def native_value(self) -> int | None:
        val = self.coordinator.data.get(self._attr)
        return int(val) if val is not None else None


class FanSpeedSensor(SwitchEntity, SensorEntity):
    """Sensor for Nintendo Switch fan speed."""

    _attr_translation_key = "fan_speed"
    _attr_icon = "mdi:fan"
    _attr_native_unit_of_measurement = "RPM"
    _attr_entity_registry_enabled_default = False

    @property
    def native_value(self) -> int | None:
        val = self.coordinator.data.get(ATTR_FAN_SPEED)
        return int(val) if val is not None else None


class StorageSensor(SwitchEntity, SensorEntity):
    """Sensor for Nintendo Switch storage."""

    _attr_device_class = SensorDeviceClass.DATA_SIZE
    _attr_state_class = SensorStateClass.MEASUREMENT
    _attr_native_unit_of_measurement = UnitOfInformation.BYTES
    _attr_entity_registry_enabled_default = False

    def __init__(
        self,
        coordinator: SwitchDataUpdateCoordinator,
        free_attr: str,
        total_attr: str,
        key: str,
    ) -> None:
        super().__init__(coordinator)
        self._free_attr = free_attr
        self._total_attr = total_attr
        self._attr_translation_key = key
        self._attr_unique_id = f"{coordinator.host}_{free_attr}"

    @property
    def native_value(self) -> int | None:
        val = self.coordinator.data.get(self._free_attr)
        return int(val) if val is not None else None

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        return {"total_size": self.coordinator.data.get(self._total_attr)}


class UptimeSensor(SwitchEntity, SensorEntity):
    """Sensor for Nintendo Switch uptime."""

    _attr_translation_key = "system_uptime"
    _attr_device_class = SensorDeviceClass.DURATION
    _attr_state_class = SensorStateClass.TOTAL_INCREASING
    _attr_native_unit_of_measurement = UnitOfTime.SECONDS
    _attr_entity_registry_enabled_default = False

    @property
    def native_value(self) -> int | None:
        val = self.coordinator.data.get(ATTR_UPTIME)
        return int(val) if val is not None else None


class WifiRSSISensor(SwitchEntity, SensorEntity):
    """Sensor for Nintendo Switch WiFi signal strength."""

    _attr_translation_key = "wifi_signal_strength"
    _attr_device_class = SensorDeviceClass.SIGNAL_STRENGTH
    _attr_state_class = SensorStateClass.MEASUREMENT
    _attr_native_unit_of_measurement = SIGNAL_STRENGTH_DECIBELS_MILLIWATT
    _attr_entity_registry_enabled_default = False

    @property
    def native_value(self) -> int | None:
        val = self.coordinator.data.get(ATTR_WIFI_RSSI)
        return int(val) if val is not None else None


class MemorySensor(SwitchEntity, SensorEntity):
    """Sensor for Nintendo Switch memory usage."""

    _attr_translation_key = "memory_usage"
    _attr_icon = "mdi:memory"
    _attr_native_unit_of_measurement = PERCENTAGE
    _attr_state_class = SensorStateClass.MEASUREMENT
    _attr_entity_registry_enabled_default = False

    @property
    def native_value(self) -> float | None:
        total = self.coordinator.data.get(ATTR_MEM_TOTAL)
        used = self.coordinator.data.get(ATTR_MEM_USED)
        if total is not None and used is not None:
            return round((float(used) / float(total)) * 100, 1)
        return None


class LogSensor(SwitchEntity, SensorEntity):
    """Sensor for Nintendo Switch sysmodule logs."""

    _attr_translation_key = "sysmodule_logs"
    _attr_icon = "mdi:text-long"
    _attr_entity_category = "diagnostic"

    @property
    def native_value(self) -> int | None:
        """Return the error count."""
        return cast(int | None, self.coordinator.data.get(ATTR_ERROR_COUNT, 0))

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        """Return raw logs as attributes."""
        return {"logs": self.coordinator.data.get("logs", [])}
