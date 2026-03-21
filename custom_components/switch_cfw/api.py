"""API client for Nintendo Switch CFW."""

from __future__ import annotations

import aiohttp
import asyncio
from typing import Any, cast

from .const import (
    ATTR_LATEST_VERSION,
    DEFAULT_PORT,
    FIRMWARE_UPDATE_URL,
    LOGGER,
)


class SwitchAPI:
    """API client for Nintendo Switch CFW Sysmodule."""

    def __init__(
        self,
        host: str,
        api_token: str,
        session: aiohttp.ClientSession | None = None,
    ) -> None:
        """Initialize the API client."""
        self._host = host
        self._api_token = api_token
        self._session = session
        self._base_url = f"http://{host}:{DEFAULT_PORT}"
        self._headers = {"X-API-Token": api_token}

    async def get_info(self) -> dict[str, Any]:
        """Get system information."""
        return cast(dict[str, Any], await self._get("/info"))

    async def reboot(self, target: str = "atmosphere") -> bool:
        """Reboot the console."""
        return await self._post("/reboot", {"target": target})

    async def shutdown(self) -> bool:
        """Shutdown the console."""
        return await self._post("/shutdown")

    async def trigger_update(self) -> bool:
        """Trigger a system update/Daybreak on the console."""
        return await self._post("/update")

    async def get_titles(self) -> list[dict[str, Any]]:
        """Get list of installed titles."""
        return cast(list[dict[str, Any]], await self._get("/titles"))

    async def launch_title(self, title_id: str) -> bool:
        """Launch a title by ID."""
        return await self._post("/command", {"action": "launch_app", "title_id": title_id})

    async def update_app(self) -> bool:
        """Trigger self-update of the Homebrew app."""
        return await self._post("/update_app")

    async def send_button(self, button: str) -> bool:
        """Send a button press command to the sysmodule."""
        return await self._post("/button", {"button": button})

    async def send_macro(self, sequence: list[dict[str, Any]]) -> bool:
        """Send a macro sequence to the sysmodule."""
        return await self._post("/button", {"sequence": sequence})

    async def get_screenshot(self) -> bytes:
        """Fetch a live screenshot from the sysmodule."""
        if not self._session:
            raise ValueError("Session not initialized")
        url = f"{self._base_url}/screenshot"
        try:
            async with asyncio.timeout(10):
                async with self._session.get(url, headers=self._headers) as response:
                    response.raise_for_status()
                    return await response.read()
        except Exception as err:
            LOGGER.error("Error fetching screenshot: %s", err)
            raise

    async def get_logs(self) -> list[dict[str, Any]]:
        """Fetch recent logs from the sysmodule."""
        return cast(list[dict[str, Any]], await self._get("/logs"))

    async def get_firmware_update(self, repository: str) -> dict[str, Any]:
        """Fetch the latest firmware version from GitHub releases."""
        if not self._session:
            return {ATTR_LATEST_VERSION: None}

        try:
            url = FIRMWARE_UPDATE_URL.format(repository=repository)
            async with asyncio.timeout(10):
                async with self._session.get(url) as response:
                    response.raise_for_status()
                    data = await response.json()
                    # The tag_name is usually the version number (e.g. 17.0.1)
                    latest_version = data.get("tag_name")
                    if latest_version and latest_version.startswith("v"):
                        latest_version = latest_version[1:]
                    return {ATTR_LATEST_VERSION: latest_version}
        except Exception as err:
            LOGGER.error("Error fetching firmware from GitHub: %s", err)
        return {ATTR_LATEST_VERSION: None}

    async def _get(self, endpoint: str) -> Any:
        """Perform a GET request."""
        if not self._session:
            return {}
        url = f"{self._base_url}{endpoint}"
        try:
            async with asyncio.timeout(10):
                async with self._session.get(url, headers=self._headers) as response:
                    response.raise_for_status()
                    return await response.json()
        except aiohttp.ClientResponseError as err:
            LOGGER.error(
                "HTTP error during GET %s: %s (status=%s)",
                endpoint,
                err.message,
                err.status,
            )
            raise
        except asyncio.TimeoutError:
            LOGGER.error("Timeout during GET %s", endpoint)
            raise
        except Exception as err:
            LOGGER.error("Unexpected error during GET %s: %s", endpoint, err)
            raise

    async def _post(self, endpoint: str, data: dict[str, Any] | None = None) -> bool:
        """Perform a POST request."""
        if not self._session:
            return False
        url = f"{self._base_url}{endpoint}"
        try:
            async with asyncio.timeout(10):
                async with self._session.post(
                    url, json=data, headers=self._headers
                ) as response:
                    response.raise_for_status()
                    # Some endpoints might not return JSON or might return empty
                    if response.status == 200:
                        try:
                            res_data = await response.json()
                            if isinstance(res_data, dict):
                                return res_data.get("status") == "ok"
                        except Exception:
                            return True # Assume success if 200 and no JSON
                    return response.status == 200
        except aiohttp.ClientResponseError as err:
            LOGGER.error(
                "HTTP error during POST %s: %s (status=%s)",
                endpoint,
                err.message,
                err.status,
            )
            return False
        except asyncio.TimeoutError:
            LOGGER.error("Timeout during POST %s", endpoint)
            return False
        except Exception as err:
            LOGGER.error("Unexpected error during POST %s: %s", endpoint, err)
            return False
