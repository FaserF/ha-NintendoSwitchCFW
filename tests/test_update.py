"""Tests for the Switch CFW update platform."""

import pytest
from unittest.mock import MagicMock, patch, AsyncMock
from custom_components.switch_cfw.update import SwitchAppUpdate, SwitchSystemUpdate


@pytest.mark.asyncio
async def test_system_update():
    """Test system update entity."""
    coordinator = MagicMock()
    coordinator.data = {"firmware_version": "17.0.0", "latest_version": "17.0.1"}

    update = SwitchSystemUpdate(coordinator)
    assert update.installed_version == "17.0.0"
    assert update.latest_version == "17.0.1"


@pytest.mark.asyncio
async def test_app_update():
    """Test app update entity."""
    coordinator = MagicMock()
    coordinator.data = {"app_version": "0.1.2", "latest_app_version": "0.1.3"}
    coordinator.entry_id = "test"

    update = SwitchAppUpdate(coordinator)
    assert update.installed_version == "0.1.2"
    assert update.latest_version == "0.1.3"

    with patch.object(
        coordinator.api, "update_app", AsyncMock(return_value=True)
    ) as mock_update:
        await update.async_install(version="0.1.3", backup=False)
        mock_update.assert_called_once()
