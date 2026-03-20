"""Tests for the Switch CFW binary sensors."""

import pytest
from unittest.mock import MagicMock
from custom_components.switch_cfw.binary_sensor import (
    ChargingBinarySensor,
    DockedBinarySensor,
    SleepBinarySensor,
)


@pytest.mark.asyncio
async def test_binary_sensor_states(mock_hass, mock_config_entry):
    """Test binary sensor states."""
    coordinator = MagicMock()
    coordinator.host = "1.2.3.4"
    coordinator.sleep_mode = False
    coordinator.data = {
        "charging": True,
        "dock_status": "docked",
        "sleep_mode": False,
    }

    # Test Charging Binary Sensor
    sensor = ChargingBinarySensor(coordinator)
    assert sensor.is_on is True

    # Test Docked Binary Sensor
    sensor = DockedBinarySensor(coordinator)
    assert sensor.is_on is True

    # Test Sleep Mode Binary Sensor
    sensor = SleepBinarySensor(coordinator)
    assert sensor.is_on is False

    # Test Sleep Mode On
    coordinator.data["sleep_mode"] = True
    assert sensor.is_on is True
