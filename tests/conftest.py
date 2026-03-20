"""Pytest configuration and fixtures for the Switch CFW integration tests."""

import pytest
from unittest.mock import AsyncMock, MagicMock, patch
from homeassistant.const import CONF_HOST


@pytest.fixture
def mock_hass():
    """Mock HomeAssistant object."""
    hass = MagicMock()
    hass.data = {}

    # Set the global hass for frame helper to avoid RuntimeError
    try:
        from homeassistant.helpers import frame

        frame._hass.hass = hass
    except ImportError, AttributeError:
        pass

    return hass


@pytest.fixture
def mock_config_entry():
    """Mock a config entry."""
    from custom_components.switch_cfw.const import CONF_API_TOKEN

    entry = MagicMock()
    entry.data = {CONF_HOST: "1.2.3.4", CONF_API_TOKEN: "test_token"}
    entry.entry_id = "test_entry_id"
    return entry


@pytest.fixture
def mock_switch_api():
    """Mock the Switch API client."""
    with patch(
        "custom_components.switch_cfw.coordinator.SwitchAPI", autospec=True
    ) as mock:
        client = mock.return_value
        # Removed unsupported __init__ mocking
        client.get_info = AsyncMock(
            return_value={
                "firmware_version": "17.0.1",
                "battery_level": 85,
                "charging": True,
                "cpu_temp": 42,
                "gpu_temp": 40,
                "skin_temp": 35,
                "fan_speed": 1200,
                "sd_total": 128000000000,
                "sd_free": 64000000000,
                "current_game": "Super Mario Odyssey",
            }
        )
        client.reboot = AsyncMock(return_value=True)
        client.shutdown = AsyncMock(return_value=True)
        client.get_firmware_update = AsyncMock(
            return_value={"latest_version": "18.0.0"}
        )
        yield client
