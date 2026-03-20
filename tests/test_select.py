"""Tests for the Switch CFW select platform."""

import pytest
from unittest.mock import MagicMock, patch
from custom_components.switch_cfw.select import SwitchGameSelect


@pytest.mark.asyncio
async def test_game_select_options():
    """Test game select options."""
    coordinator = MagicMock()
    coordinator.data = {
        "titles": [
            {"title_id": "0x0100000000001000", "name": "Mario Kart 8 Deluxe"},
            {"title_id": "0x0100000000002000", "name": "Zelda: BOTW"},
        ]
    }
    coordinator.config_entry.entry_id = "test_entry"
    coordinator.device_info = {}

    select = SwitchGameSelect(coordinator)
    assert "Mario Kart 8 Deluxe" in select.options
    assert "Zelda: BOTW" in select.options
    assert len(select.options) == 2


@pytest.mark.asyncio
async def test_game_select_current():
    """Test current game selection."""
    coordinator = MagicMock()
    coordinator.data = {
        "titles": [{"title_id": "0x0100000000001000", "name": "Mario Kart 8"}],
        "current_title_id": "0x0100000000001000",
    }

    select = SwitchGameSelect(coordinator)
    # Trigger options to populate internal cache
    _ = select.options
    assert select.current_option == "Mario Kart 8"


@pytest.mark.asyncio
async def test_game_select_action():
    """Test selecting a game triggers launch."""
    coordinator = MagicMock()
    coordinator.data = {
        "titles": [{"title_id": "0x0100000000001000", "name": "Mario Kart 8"}]
    }

    select = SwitchGameSelect(coordinator)
    _ = select.options

    with patch.object(
        coordinator.api, "launch_title", return_value=True
    ) as mock_launch:
        await select.async_select_option("Mario Kart 8")
        mock_launch.assert_called_once_with("0x0100000000001000")
