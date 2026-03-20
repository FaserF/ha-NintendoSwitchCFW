"""Tests for the Switch CFW coordinator."""

import pytest
from datetime import timedelta
from homeassistant.helpers.update_coordinator import UpdateFailed
from custom_components.switch_cfw.coordinator import SwitchDataUpdateCoordinator


@pytest.mark.asyncio
async def test_coordinator_update_success(
    mock_hass, mock_config_entry, mock_switch_api
):
    """Test successful coordinator update."""
    coordinator = SwitchDataUpdateCoordinator(mock_hass, mock_config_entry)

    # Trigger update
    data = await coordinator._async_update_data()

    assert data["firmware_version"] == "17.0.1"
    assert data["battery_level"] == 85
    assert data["sleep_mode"] is False
    assert coordinator.sleep_mode is False
    assert coordinator.update_interval == timedelta(seconds=30)


@pytest.mark.asyncio
async def test_coordinator_sleep_mode(mock_hass, mock_config_entry, mock_switch_api):
    """Test sleep mode logic in coordinator."""
    coordinator = SwitchDataUpdateCoordinator(mock_hass, mock_config_entry)

    # 1. First update succeeds
    await coordinator._async_update_data()
    assert coordinator.sleep_mode is False

    # 2. API fails - should enter sleep mode
    import aiohttp

    mock_switch_api.get_info.side_effect = aiohttp.ClientError("Connection error")
    data = await coordinator._async_update_data()

    assert coordinator.sleep_mode is True
    assert data["sleep_mode"] is True
    assert data["battery_level"] == 85  # Preserved last value
    assert coordinator.update_interval == timedelta(minutes=5)

    # 3. API succeeds - should wake up
    mock_switch_api.get_info.side_effect = None
    mock_switch_api.get_info.return_value = {
        "firmware_version": "17.0.1",
        "battery_level": 80,
        "charging": False,
    }
    data = await coordinator._async_update_data()

    assert coordinator.sleep_mode is False
    assert data["sleep_mode"] is False
    assert data["battery_level"] == 80
    assert coordinator.update_interval == timedelta(seconds=30)


@pytest.mark.asyncio
async def test_coordinator_initial_failure(
    mock_hass, mock_config_entry, mock_switch_api
):
    """Test failure on very first update (no last data)."""
    coordinator = SwitchDataUpdateCoordinator(mock_hass, mock_config_entry)
    mock_switch_api.get_info.side_effect = Exception("Initial connection error")

    with pytest.raises(UpdateFailed):
        await coordinator._async_update_data()
