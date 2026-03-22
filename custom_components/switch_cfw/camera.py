"""Camera platform for Nintendo Switch CFW."""

from __future__ import annotations

from homeassistant.components.camera import Camera
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DOMAIN
from .entity import SwitchEntity


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the Nintendo Switch CFW camera."""
    coordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities([SwitchCamera(coordinator)])


class SwitchCamera(SwitchEntity, Camera):
    """Nintendo Switch CFW Camera Entity."""

    def __init__(self, coordinator) -> None:
        """Initialize the camera."""
        SwitchEntity.__init__(self, coordinator)
        Camera.__init__(self)
        self._attr_name = "Nintendo Switch Screenshot"
        self._attr_unique_id = f"{coordinator.config_entry.entry_id}_screenshot"

    async def async_camera_image(
        self, width: int | None = None, height: int | None = None
    ) -> bytes | None:
        """Return a still image response from the camera."""
        try:
            image = await self.coordinator.api.get_screenshot()
            if isinstance(image, bytes):
                return image
            return None
        except Exception:
            return None
