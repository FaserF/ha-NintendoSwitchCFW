"""Tests for the Switch CFW buttons."""

import pytest
from unittest.mock import MagicMock, AsyncMock
from custom_components.switch_cfw.button import SwitchButton


@pytest.mark.asyncio
async def test_button_press(mock_hass, mock_config_entry):
    """Test button press actions."""
    coordinator = MagicMock()
    coordinator.api = AsyncMock()

    # Test Reboot Button
    button = SwitchButton(coordinator, mock_config_entry, "reboot")
    await button.async_press()
    coordinator.api.reboot.assert_called_once()

    # Test Shutdown Button
    button = SwitchButton(coordinator, mock_config_entry, "shutdown")
    await button.async_press()
    coordinator.api.shutdown.assert_called_once()
