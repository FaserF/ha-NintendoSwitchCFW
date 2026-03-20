"""Update platform for Nintendo Switch CFW."""

from __future__ import annotations

from typing import Any, cast

from homeassistant.components.update import (
    UpdateDeviceClass,
    UpdateEntity,
    UpdateEntityFeature,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DOMAIN, LOGGER, ATTR_APP_VERSION
from .coordinator import SwitchDataUpdateCoordinator
from .entity import SwitchEntity


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the Switch update entities."""
    coordinator = hass.data[DOMAIN][config_entry.entry_id]

    entities = [
        SwitchSystemUpdate(coordinator),
        SwitchAppUpdate(coordinator),
    ]
    async_add_entities(entities)


class SwitchUpdateEntity(SwitchEntity, UpdateEntity):
    """Base class for Switch update entities."""

    _attr_has_entity_name = True
    _attr_device_class = UpdateDeviceClass.FIRMWARE
    _attr_supported_features = UpdateEntityFeature.INSTALL

    def __init__(self, coordinator: SwitchDataUpdateCoordinator) -> None:
        """Initialize the update entity."""
        super().__init__(coordinator)


class SwitchSystemUpdate(SwitchUpdateEntity):
    """Representation of a Switch system firmware update."""

    _attr_translation_key = "firmware"

    def __init__(self, coordinator: SwitchDataUpdateCoordinator) -> None:
        """Initialize the system update entity."""
        super().__init__(coordinator)
        self._attr_unique_id = f"{coordinator.entry.entry_id}_firmware"

    @property
    def installed_version(self) -> str | None:
        """Return the current firmware version."""
        return cast(str | None, self.coordinator.data.get("firmware_version"))

    @property
    def latest_version(self) -> str | None:
        """Return the latest available firmware version."""
        return cast(str | None, self.coordinator.data.get("latest_version"))

    async def async_install(
        self, version: str | None, backup: bool, **kwargs: Any
    ) -> None:
        """Install an update."""
        LOGGER.info("Triggering system update (Daybreak) on the Switch")
        await self.coordinator.api.trigger_update()


class SwitchAppUpdate(SwitchUpdateEntity):
    """Representation of a Homebrew App update."""

    _attr_translation_key = "homebrew_app"

    def __init__(self, coordinator: SwitchDataUpdateCoordinator) -> None:
        """Initialize the app update entity."""
        super().__init__(coordinator)
        self._attr_unique_id = f"{coordinator.entry.entry_id}_app_update"
        # For the app update, we'll use our own repo versioning
        self._repo = "FaserF/ha-NintendoSwitchCFW"

    @property
    def installed_version(self) -> str | None:
        """Return the current app version."""
        return cast(str | None, self.coordinator.data.get(ATTR_APP_VERSION))

    @property
    def latest_version(self) -> str | None:
        """Return the latest available app version."""
        # We can reuse the github release check logic for our own repo
        # Actually, let's assume the coordinator fetches it
        return cast(str | None, self.coordinator.data.get("latest_app_version"))

    @property
    def release_summary(self) -> str | None:
        """Return the release notes."""
        return "New version available on GitHub. This will update the NRO and NSO files and reboot the console."

    async def async_install(
        self, version: str | None, backup: bool, **kwargs: Any
    ) -> None:
        """Install the Homebrew app update."""
        LOGGER.info("Triggering self-update for the Homebrew app")
        success = await self.coordinator.api.update_app()
        if not success:
            LOGGER.error("Failed to trigger app update")
