"""Tests for the Switch CFW buttons."""

import pytest
from unittest.mock import MagicMock, AsyncMock
from custom_components.switch_cfw.button import RebootButton, ShutdownButton


@pytest.mark.asyncio
async def test_button_press(mock_hass, mock_config_entry):
    """Test button press actions."""
    coordinator = MagicMock()
    coordinator.api = AsyncMock()

    # Test Reboot Button
    button = RebootButton(coordinator)
    await button.async_press()
    coordinator.api.reboot.assert_called_once()

    # Test Shutdown Button
    button = ShutdownButton(coordinator)
    await button.async_press()
    coordinator.api.shutdown.assert_called_once()
