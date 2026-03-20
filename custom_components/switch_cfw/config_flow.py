"""Config flow for Nintendo Switch CFW integration."""

from __future__ import annotations

import asyncio
import socket
from typing import TYPE_CHECKING, Any

import aiohttp
import voluptuous as vol

if TYPE_CHECKING:
    from homeassistant.helpers.service_info.zeroconf import ZeroconfServiceInfo
else:
    # ZeroconfServiceInfo location depends on Home Assistant version
    try:
        from homeassistant.helpers.service_info.zeroconf import ZeroconfServiceInfo
    except ImportError:
        try:
            from homeassistant.components.zeroconf import ZeroconfServiceInfo
        except ImportError:
            from typing import Any as ZeroconfServiceInfo

from homeassistant.components import network
from homeassistant.config_entries import (
    ConfigEntry,
    ConfigFlow as ConfigFlowBase,
    ConfigFlowResult,
    OptionsFlow,
)
from homeassistant.const import CONF_HOST, CONF_NAME
from homeassistant.core import callback


from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers import config_validation as cv

from .api import SwitchAPI
from .const import (
    ATTR_APP_VERSION,
    CONF_API_TOKEN,
    CONF_UPDATE_REPO,
    DEFAULT_PORT,
    DOMAIN,
    LOGGER,
    MIN_APP_VERSION,
)


class ConfigFlow(ConfigFlowBase, domain=DOMAIN):  # type: ignore[call-arg]
    """Handle a config flow for Nintendo Switch CFW."""

    VERSION = 1

    def __init__(self) -> None:
        """Initialize the config flow."""
        self._host: str | None = None
        self._name: str | None = None

    async def async_step_user(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Handle the initial step."""

        if user_input is not None:
            if user_input.get("flow_type") == "manual":
                return await self.async_step_manual_entry()
            return await self.async_step_discovery()

        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema(
                {
                    vol.Required("flow_type", default="discovery"): vol.In(
                        ["discovery", "manual"]
                    )
                }
            ),
        )

    async def async_step_discovery(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Scan for Nintendo Switch consoles in the background."""
        if user_input is not None:
            # We already have discovered devices, let the user select one
            if user_input.get("device"):
                self._host = user_input["device"]
                self._name = "Nintendo Switch"
                return await self.async_step_discovery_confirm()

        # Perform scanning
        potential_hosts: set[str] = set()

        try:
            adapters = await network.async_get_adapters(self.hass)
            for adapter in adapters:
                for ipv4 in adapter.get("ipv4", []):
                    local_ip = ipv4.get("address")
                    if local_ip:
                        parts = local_ip.split(".")
                        if len(parts) == 4:
                            # Guess common IPs or scan the subnet Gateway
                            potential_hosts.add(".".join(parts[:-1] + ["1"]))
                            # Also check the last few IPs if they could be static
                            for i in range(2, 10):
                                potential_hosts.add(".".join(parts[:-1] + [str(i)]))
        except Exception:
            pass

        tasks = [self._async_probe_switch(host) for host in potential_hosts]
        discovered = await asyncio.gather(*tasks)

        discovered_options = {
            res["host"]: f"Nintendo Switch ({res['host']})" for res in discovered if res
        }

        if not discovered_options:
            return self.async_show_form(
                step_id="discovery",
                errors={"base": "no_devices_found"},
            )

        return self.async_show_form(
            step_id="discovery",
            data_schema=vol.Schema(
                {vol.Required("device"): vol.In(discovered_options)}
            ),
        )

    async def _async_probe_switch(self, host: str) -> dict[str, Any] | None:
        """Probe a host to see if it's a Nintendo Switch with our sysmodule."""
        try:
            async with asyncio.timeout(1.0):
                # Try a quick socket connection first to see if port is open
                loop = asyncio.get_running_loop()
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.setblocking(False)
                await loop.sock_connect(sock, (host, DEFAULT_PORT))
                sock.close()

                # If port is open, try HTTP info
                session = async_get_clientsession(self.hass)
                async with session.get(f"http://{host}:{DEFAULT_PORT}/info") as resp:
                    if resp.status == 200:
                        data = await resp.json()
                        return {
                            "host": host,
                            "name": data.get("name", "Nintendo Switch"),
                        }
                    LOGGER.debug("Probe of %s returned status %s", host, resp.status)
        except (aiohttp.ClientError, asyncio.TimeoutError, socket.error) as err:
            LOGGER.debug("Probe of %s failed: %s", host, err)
        except Exception as err:
            LOGGER.debug("Unexpected error during probe of %s: %s", host, err)
        return None

    async def async_step_manual_entry(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Handle manual IP entry."""

        errors: dict[str, str] = {}
        if user_input is not None:
            self._host = user_input[CONF_HOST]
            token = user_input[CONF_API_TOKEN]
            try:
                # Test connection with the provided token
                api = SwitchAPI(self._host, token, async_get_clientsession(self.hass))
                info = await api.get_info()
                if not isinstance(info, dict):
                    errors["base"] = "cannot_connect"
                    return await self.async_step_manual_entry(user_input)

                # Check version
                app_version = info.get(ATTR_APP_VERSION, "0.0.0")
                if app_version < MIN_APP_VERSION:
                    errors["base"] = "app_version_outdated"
                    return self.async_show_form(
                        step_id="manual_entry",
                        data_schema=vol.Schema(
                            {
                                vol.Required(CONF_HOST, default=self._host): str,
                                vol.Required(CONF_API_TOKEN, default=token): str,
                                vol.Optional(
                                    CONF_NAME,
                                    default=user_input.get(
                                        CONF_NAME, "Nintendo Switch"
                                    ),
                                ): str,
                            }
                        ),
                        errors=errors,
                        description_placeholders={
                            "current_version": app_version,
                            "min_version": MIN_APP_VERSION,
                        },
                    )
                return self.async_create_entry(
                    title=user_input.get(CONF_NAME, "Nintendo Switch"),
                    data={
                        CONF_HOST: self._host,
                        CONF_API_TOKEN: token,
                    },
                )
            except aiohttp.ClientResponseError as err:
                if err.status == 401:
                    errors["base"] = "invalid_auth"
                else:
                    errors["base"] = "cannot_connect"
                LOGGER.error(
                    "Manual connection to %s failed with status %s",
                    self._host,
                    err.status,
                )
            except aiohttp.ClientError, asyncio.TimeoutError:
                errors["base"] = "cannot_connect"
                LOGGER.error("Manual connection to %s timed out or failed", self._host)
            except Exception as err:
                errors["base"] = "unknown"
                LOGGER.error(
                    "Unexpected error during manual connection to %s: %s",
                    self._host,
                    err,
                )

        return self.async_show_form(
            step_id="manual_entry",
            data_schema=vol.Schema(
                {
                    vol.Required(CONF_HOST): cv.string,
                    vol.Required(CONF_API_TOKEN): cv.string,
                    vol.Optional(CONF_NAME, default="Nintendo Switch"): cv.string,
                }
            ),
            description_placeholders={
                "token_hint": "Enter the API Token shown in the 'Home Assistant Switch' app on your console."
            },
            errors=errors,
        )

    async def async_step_zeroconf(
        self, discovery_info: ZeroconfServiceInfo
    ) -> ConfigFlowResult:
        """Handle zeroconf discovery."""
        self._host = discovery_info.host
        self._name = "Nintendo Switch"
        if discovery_info.hostname:
            self._name = discovery_info.hostname.split(".")[0]

        await self.async_set_unique_id(self._host)
        self._abort_if_unique_id_configured()

        return await self.async_step_discovery_confirm()

    async def async_step_discovery_confirm(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Confirm discovery and get security token."""

        errors: dict[str, str] = {}

        if user_input is not None:
            token = user_input[CONF_API_TOKEN]
            try:
                # Test connection with the provided token
                if self._host is None:
                    return self.async_abort(reason="cannot_connect")
                api = SwitchAPI(self._host, token, async_get_clientsession(self.hass))
                info = await api.get_info()

                # Check version
                app_version = info.get(ATTR_APP_VERSION, "0.0.0")
                if app_version < MIN_APP_VERSION:
                    errors["base"] = "app_version_outdated"
                    return self.async_show_form(
                        step_id="discovery_confirm",
                        data_schema=vol.Schema(
                            {vol.Required(CONF_API_TOKEN, default=token): str}
                        ),
                        errors=errors,
                        description_placeholders={
                            "current_version": app_version,
                            "min_version": MIN_APP_VERSION,
                        },
                    )
                return self.async_create_entry(
                    title=self._name or "Nintendo Switch",
                    data={
                        CONF_HOST: self._host,
                        CONF_API_TOKEN: token,
                    },
                )
            except aiohttp.ClientResponseError as err:
                if err.status == 401:
                    errors["base"] = "invalid_auth"
                else:
                    errors["base"] = "cannot_connect"
                LOGGER.error(
                    "Discovery confirmation for %s failed with status %s",
                    self._host,
                    err.status,
                )
            except aiohttp.ClientError, asyncio.TimeoutError:
                errors["base"] = "cannot_connect"
                LOGGER.error(
                    "Discovery confirmation for %s timed out or failed", self._host
                )
            except Exception as err:
                errors["base"] = "unknown"
                LOGGER.error(
                    "Unexpected error during discovery confirmation for %s: %s",
                    self._host,
                    err,
                )

        return self.async_show_form(
            step_id="discovery_confirm",
            data_schema=vol.Schema({vol.Required(CONF_API_TOKEN): str}),
            errors=errors,
        )

    @staticmethod
    @callback
    def async_get_options_flow(
        config_entry: ConfigEntry,
    ) -> OptionsFlow:
        """Create the options flow."""
        return OptionsFlowHandler()


class OptionsFlowHandler(OptionsFlow):
    """Handle an options flow for Nintendo Switch CFW."""

    async def async_step_init(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Manage the options."""
        if user_input is not None:
            return self.async_create_entry(title="", data=user_input)

        return self.async_show_form(
            step_id="init",
            data_schema=vol.Schema(
                {
                    vol.Optional(
                        CONF_UPDATE_REPO,
                        default=self.config_entry.options.get(
                            CONF_UPDATE_REPO, "THZoria/NX_Firmware"
                        ),
                    ): str,
                }
            ),
        )
