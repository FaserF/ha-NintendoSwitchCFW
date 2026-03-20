"""Button platform for Nintendo Switch CFW."""

from __future__ import annotations

from homeassistant.components.button import ButtonEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DOMAIN
from .coordinator import SwitchDataUpdateCoordinator
from .entity import SwitchEntity


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the button platform."""
    coordinator: SwitchDataUpdateCoordinator = hass.data[DOMAIN][entry.entry_id]

    async_add_entities(
        [
            RebootButton(coordinator),
            ShutdownButton(coordinator),
        ]
    )


class RebootButton(SwitchEntity, ButtonEntity):
    """Button to reboot the Nintendo Switch."""

    _attr_translation_key = "reboot"
    _attr_icon = "mdi:restart"

    async def async_press(self) -> None:
        """Handle the button press."""
        await self.coordinator.api.reboot()


class ShutdownButton(SwitchEntity, ButtonEntity):
    """Button to shutdown the Nintendo Switch."""

    _attr_translation_key = "shutdown"
    _attr_icon = "mdi:power"

    async def async_press(self) -> None:
        """Handle the button press."""
        await self.coordinator.api.shutdown()
