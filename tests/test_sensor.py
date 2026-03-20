"""Tests for the Switch CFW sensors."""

import pytest
from unittest.mock import MagicMock
from custom_components.switch_cfw.sensor import (
    FirmwareSensor,
    BatterySensor,
    CurrentGameSensor,
    TemperatureSensor,
    StorageSensor,
)


@pytest.mark.asyncio
async def test_sensor_states(mock_hass, mock_config_entry):
    """Test sensor states and attributes."""
    coordinator = MagicMock()
    coordinator.host = "1.2.3.4"
    coordinator.sleep_mode = False
    coordinator.data = {
        "firmware_version": "17.0.1",
        "battery_level": 85,
        "current_game": "Super Mario Odyssey",
        "cpu_temp": 42,
        "sd_free": 64000000000,
        "sd_total": 128000000000,
    }

    # Test Firmware Sensor
    sensor = FirmwareSensor(coordinator)
    assert sensor.native_value == "17.0.1"

    # Test Battery Sensor
    sensor = BatterySensor(coordinator)
    assert sensor.native_value == 85

    # Test Current Game Sensor
    sensor = CurrentGameSensor(coordinator)
    assert sensor.native_value == "Super Mario Odyssey"

    # Test Temperature Sensor
    sensor = TemperatureSensor(coordinator, "cpu_temp", "CPU Temp")
    assert sensor.native_value == 42

    # Test Storage Sensor
    sensor = StorageSensor(coordinator, "sd_free", "sd_total", "SD Card")
    assert sensor.native_value == 64000000000
    assert sensor.extra_state_attributes["total_size"] == 128000000000
