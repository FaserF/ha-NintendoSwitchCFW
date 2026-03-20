"""Base entity for Nintendo Switch CFW."""

from __future__ import annotations

from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN, DEFAULT_PORT
from .coordinator import SwitchDataUpdateCoordinator


class SwitchEntity(CoordinatorEntity[SwitchDataUpdateCoordinator]):
    """Base class for Nintendo Switch CFW entities."""

    _attr_has_entity_name = True

    def __init__(self, coordinator: SwitchDataUpdateCoordinator) -> None:
        """Initialize the entity."""
        super().__init__(coordinator)
        self._attr_device_info = DeviceInfo(
            identifiers={(DOMAIN, coordinator.host)},
            name="Nintendo Switch",
            manufacturer="Nintendo",
            model="Switch (CFW)",
            sw_version=str(coordinator.data.get("firmware_version"))
            if coordinator.data.get("firmware_version")
            else None,
            configuration_url=f"http://{coordinator.host}:{DEFAULT_PORT}/info",
        )
        self._attr_unique_id = f"{coordinator.host}_{self.__class__.__name__}"

    @property
    def available(self) -> bool:
        """Return if entity is available."""
        # Always available if we have last known data and are in sleep mode
        return bool(super().available or self.coordinator.sleep_mode)
