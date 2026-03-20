"""Tests for the Switch CFW config flow."""

import pytest
from unittest.mock import MagicMock, patch
import aiohttp
from homeassistant.const import CONF_HOST
from custom_components.switch_cfw.config_flow import ConfigFlow
from custom_components.switch_cfw.const import (
    CONF_API_TOKEN,
    ATTR_APP_VERSION,
    MIN_APP_VERSION,
)


@pytest.mark.asyncio
async def test_config_flow_manual_success():
    """Test manual entry in config flow - success."""
    flow = ConfigFlow()
    flow.hass = MagicMock()

    # Step 1: User chooses manual
    result = await flow.async_step_user({"flow_type": "manual"})
    assert result["type"] == "form"
    assert result["step_id"] == "manual_entry"

    # Step 2: User enters IP and token
    with patch(
        "custom_components.switch_cfw.api.SwitchAPI.get_info",
        return_value={ATTR_APP_VERSION: MIN_APP_VERSION},
    ):
        result = await flow.async_step_manual_entry(
            {CONF_HOST: "1.2.3.4", CONF_API_TOKEN: "test-token"}
        )
        assert result["type"] == "create_entry"
        assert result["title"] == "Nintendo Switch"
        assert result["data"] == {CONF_HOST: "1.2.3.4", CONF_API_TOKEN: "test-token"}


@pytest.mark.asyncio
async def test_config_flow_manual_outdated():
    """Test manual entry in config flow - outdated app."""
    flow = ConfigFlow()
    flow.hass = MagicMock()

    result = await flow.async_step_manual_entry(
        {CONF_HOST: "1.2.3.4", CONF_API_TOKEN: "test-token"}
    )

    with patch(
        "custom_components.switch_cfw.api.SwitchAPI.get_info",
        return_value={ATTR_APP_VERSION: "0.1.0"},
    ):
        result = await flow.async_step_manual_entry(
            {CONF_HOST: "1.2.3.4", CONF_API_TOKEN: "test-token"}
        )
        assert result["type"] == "form"
        assert result["errors"]["base"] == "app_version_outdated"


@pytest.mark.asyncio
async def test_config_flow_manual_auth_error():
    """Test manual entry in config flow - invalid auth."""
    flow = ConfigFlow()
    flow.hass = MagicMock()

    with patch(
        "custom_components.switch_cfw.api.SwitchAPI.get_info",
        side_effect=aiohttp.ClientResponseError(MagicMock(), MagicMock(), status=401),
    ):
        result = await flow.async_step_manual_entry(
            {CONF_HOST: "1.2.3.4", CONF_API_TOKEN: "wrong-token"}
        )
        assert result["type"] == "form"
        assert result["errors"]["base"] == "invalid_auth"


@pytest.mark.asyncio
async def test_config_flow_discovery_success():
    """Test discovery in config flow - success."""
    flow = ConfigFlow()
    flow.hass = MagicMock()

    # Mock probe success
    with patch(
        "custom_components.switch_cfw.config_flow.ConfigFlow._async_probe_switch",
        return_value={"host": "1.2.3.4", "name": "Switch-LivingRoom"},
    ):
        result = await flow.async_step_discovery()
        assert result["type"] == "form"
        assert result["step_id"] == "discovery"

        # User selects device
        result = await flow.async_step_discovery({"device": "1.2.3.4"})
        assert result["type"] == "form"
        assert result["step_id"] == "discovery_confirm"

        # User confirms with token
        with patch(
            "custom_components.switch_cfw.api.SwitchAPI.get_info",
            return_value={ATTR_APP_VERSION: MIN_APP_VERSION},
        ):
            result = await flow.async_step_discovery_confirm(
                {CONF_API_TOKEN: "test-token"}
            )
            assert result["type"] == "create_entry"
            assert result["data"] == {
                CONF_HOST: "1.2.3.4",
                CONF_API_TOKEN: "test-token",
            }


@pytest.mark.asyncio
async def test_config_flow_zeroconf():
    """Test zeroconf discovery."""
    flow = ConfigFlow()
    flow.hass = MagicMock()

    discovery_info = MagicMock()
    discovery_info.host = "1.2.3.4"
    discovery_info.hostname = "switch-room.local."

    with (
        patch(
            "custom_components.switch_cfw.config_flow.ConfigFlow.async_set_unique_id"
        ),
        patch(
            "custom_components.switch_cfw.config_flow.ConfigFlow._abort_if_unique_id_configured"
        ),
    ):
        result = await flow.async_step_zeroconf(discovery_info)
        assert result["type"] == "form"
        assert result["step_id"] == "discovery_confirm"
        assert flow._host == "1.2.3.4"
        assert flow._name == "switch-room"
