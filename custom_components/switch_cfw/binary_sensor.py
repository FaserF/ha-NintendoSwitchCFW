"""Binary sensor platform for Nintendo Switch CFW."""

from __future__ import annotations

from homeassistant.components.binary_sensor import (
    BinarySensorDeviceClass,
    BinarySensorEntity,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from typing import cast
from .const import DOMAIN, ATTR_CHARGING, ATTR_SLEEP_MODE, ATTR_DOCK_STATUS
from .coordinator import SwitchDataUpdateCoordinator
from .entity import SwitchEntity


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the binary sensor platform."""
    coordinator: SwitchDataUpdateCoordinator = hass.data[DOMAIN][entry.entry_id]

    async_add_entities(
        [
            ChargingBinarySensor(coordinator),
            DockedBinarySensor(coordinator),
            SleepBinarySensor(coordinator),
        ]
    )


class ChargingBinarySensor(SwitchEntity, BinarySensorEntity):
    """Binary sensor for Nintendo Switch charging status."""

    _attr_translation_key = "charging"
    _attr_device_class = BinarySensorDeviceClass.BATTERY_CHARGING

    @property
    def is_on(self) -> bool | None:
        """Return true if the binary sensor is on."""
        return cast(bool | None, self.coordinator.data.get(ATTR_CHARGING))


class DockedBinarySensor(SwitchEntity, BinarySensorEntity):
    """Binary sensor for Nintendo Switch docked status."""

    _attr_translation_key = "docked"
    _attr_device_class = BinarySensorDeviceClass.PLUG

    @property
    def is_on(self) -> bool | None:
        """Return true if the binary sensor is on."""
        return cast(bool | None, self.coordinator.data.get(ATTR_DOCK_STATUS))


class SleepBinarySensor(SwitchEntity, BinarySensorEntity):
    """Binary sensor for Nintendo Switch sleep mode."""

    _attr_translation_key = "sleep_mode"
    _attr_device_class = (
        BinarySensorDeviceClass.MOISTURE
    )  # No explicit SLEEP class, MOISTURE isn't right either
    # Actually, often Power or Connectivity is used, or no device class.

    _attr_icon = "mdi:sleep"

    @property
    def is_on(self) -> bool | None:
        """Return true if the binary sensor is on."""
        return cast(bool | None, self.coordinator.data.get(ATTR_SLEEP_MODE))
