"""Remote platform for Nintendo Switch CFW."""

from __future__ import annotations

from collections.abc import Iterable
from typing import Any

from homeassistant.components.remote import RemoteEntity, RemoteEntityFeature
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DOMAIN, LOGGER
from .coordinator import SwitchDataUpdateCoordinator
from .entity import SwitchEntity


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the Switch remote platform."""
    coordinator: SwitchDataUpdateCoordinator = hass.data[DOMAIN][config_entry.entry_id]
    async_add_entities([SwitchRemote(coordinator)])


class SwitchRemote(SwitchEntity, RemoteEntity):
    """Representation of a Nintendo Switch remote."""

    _attr_translation_key = "remote"
    _attr_supported_features = (
        RemoteEntityFeature.RECALL_SCENE | RemoteEntityFeature.ACTIVITY
    )

    def __init__(self, coordinator: SwitchDataUpdateCoordinator) -> None:
        """Initialize the remote."""
        SwitchEntity.__init__(self, coordinator)
        # Unique ID is handled by SwitchEntity

    @property
    def is_on(self) -> bool:
        """Return true if device is on."""
        return not self.coordinator.sleep_mode

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn the device on (Wake from sleep)."""
        # We don't have a direct WAKE command yet, but launching a title often wakes it
        # Or we could implement a /wake endpoint later.
        # For now, let's just log.
        LOGGER.debug("Wake requested via remote")

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn the device off (Enter sleep)."""
        await self.coordinator.api.shutdown()

    async def async_send_command(self, command: Iterable[str], **kwargs: Any) -> None:
        """Send a command to the device."""
        for cmd in command:
            cmd_upper = cmd.upper()
            LOGGER.debug("Sending remote command: %s", cmd_upper)
            success = await self.coordinator.api.send_button(cmd_upper)
            if not success:
                LOGGER.error("Failed to send remote command: %s", cmd_upper)
