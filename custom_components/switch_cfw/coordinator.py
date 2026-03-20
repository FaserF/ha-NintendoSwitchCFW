"""Data coordinator for Nintendo Switch CFW."""

from __future__ import annotations

from datetime import timedelta
from typing import Any
import asyncio
import aiohttp
import async_timeout

from homeassistant.config_entries import ConfigEntry
from homeassistant.const import CONF_HOST
from homeassistant.core import HomeAssistant
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .api import SwitchAPI
from .const import DOMAIN, LOGGER, CONF_UPDATE_REPO


class SwitchDataUpdateCoordinator(DataUpdateCoordinator[dict[str, Any]]):
    """Class to manage fetching Nintendo Switch data."""

    def __init__(self, hass: HomeAssistant, entry: ConfigEntry) -> None:
        """Initialize the coordinator."""
        from .const import CONF_API_TOKEN

        self.host = entry.data[CONF_HOST]
        self.api_token = entry.data[CONF_API_TOKEN]
        self.entry = entry
        self.api = SwitchAPI(self.host, self.api_token, async_get_clientsession(hass))
        self.sleep_mode = False
        self._normal_interval = timedelta(seconds=30)
        self._sleep_interval = timedelta(minutes=5)

        super().__init__(
            hass,
            LOGGER,
            name=DOMAIN,
            update_interval=self._normal_interval,
        )

    async def _async_update_data(self) -> dict[str, Any]:
        """Fetch data from the Nintendo Switch and GitHub."""
        try:
            async with async_timeout.timeout(10):
                # Fetch system info from Switch
                data = await self.api.get_info()

                # If we get here, the Switch is online
                if self.sleep_mode:
                    LOGGER.info("Nintendo Switch %s has woken up", self.host)
                    self.sleep_mode = False
                    self.update_interval = self._normal_interval

                # Fetch latest firmware info from GitHub
                update_repo = self.entry.options.get(
                    CONF_UPDATE_REPO, "THZoria/NX_Firmware"
                )
                fw_update = await self.api.get_firmware_update(repository=update_repo)
                data.update(fw_update)

                # Fetch latest app info from GitHub
                app_update = await self.api.get_firmware_update(
                    repository="FaserF/ha-NintentdoSwitchCFW"
                )
                if app_update:
                    data["latest_app_version"] = app_update.get("latest_version")

                # Fetch titles
                titles = await self.api.get_titles()
                data["titles"] = titles

                # Fetch logs
                logs = await self.api.get_logs()
                data["logs"] = logs

                if "sleep_mode" not in data:
                    data["sleep_mode"] = False

                return data
        except (aiohttp.ClientError, TimeoutError, asyncio.TimeoutError) as err:
            if not self.sleep_mode and self.data:
                LOGGER.info(
                    "Nintendo Switch %s is unreachable, entering sleep mode", self.host
                )
                self.sleep_mode = True
                self.update_interval = self._sleep_interval

            if self.sleep_mode:
                # Return last known data with sleep_mode = True
                new_data = dict(self.data)
                new_data["sleep_mode"] = True
                return new_data

            raise UpdateFailed(
                f"Failed to connect to the Nintendo Switch: {err}"
            ) from err
        except Exception as err:
            raise UpdateFailed(f"Unknown error occurred: {err}") from err
