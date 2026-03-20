"""Select platform for Nintendo Switch CFW."""

from __future__ import annotations


from homeassistant.components.select import SelectEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from typing import cast
from .const import DOMAIN, LOGGER
from .coordinator import SwitchDataUpdateCoordinator
from .entity import SwitchEntity


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the Switch select entities."""
    coordinator = hass.data[DOMAIN][config_entry.entry_id]
    async_add_entities([SwitchGameSelect(coordinator)])


class SwitchGameSelect(SwitchEntity, SelectEntity):
    """Representation of an installed game select entity."""

    _attr_translation_key = "game_list"
    _attr_has_entity_name = True

    def __init__(self, coordinator: SwitchDataUpdateCoordinator) -> None:
        """Initialize the select entity."""
        super().__init__(coordinator)
        self._attr_unique_id = f"{coordinator.entry.entry_id}_games"
        self._titles: dict[str, str] = {}  # Name -> ID

    @property
    def options(self) -> list[str]:
        """Return a list of available games."""
        titles = cast(list[dict[str, str]], self.coordinator.data.get("titles", []))
        self._titles = {t["name"]: t["title_id"] for t in titles}
        return sorted(self._titles.keys())

    @property
    def current_option(self) -> str | None:
        """Return the currently running game."""
        current_id = cast(str | None, self.coordinator.data.get("current_title_id"))
        if not current_id:
            return None

        # Try to find name by ID
        for name, tid in self._titles.items():
            if tid == current_id:
                return name
        return cast(str | None, self.coordinator.data.get("current_game"))

    async def async_select_option(self, option: str) -> None:
        """Launch the selected game."""
        title_id = self._titles.get(option)
        if not title_id:
            LOGGER.error("Selected game %s not found in title list", option)
            return

        LOGGER.debug("Launching game %s (%s)", option, title_id)
        success = await self.coordinator.api.launch_title(title_id)
        if success:
            # Force an immediate refresh to update the "current game" status
            await self.coordinator.async_request_refresh()
        else:
            LOGGER.error("Failed to launch game %s", option)
