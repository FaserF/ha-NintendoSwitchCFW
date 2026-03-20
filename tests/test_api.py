"""Tests for the Switch API."""

import pytest
from unittest.mock import patch, MagicMock
from custom_components.switch_cfw.api import SwitchAPI


@pytest.mark.asyncio
async def test_api_get_info():
    """Test get_info method."""
    api = SwitchAPI("1.2.3.4", "test_token")

    mock_response = MagicMock()
    mock_response.status = 200
    mock_response.json = MagicMock(return_value={"firmware_version": "17.0.1"})

    with patch(
        "aiohttp.ClientSession.get",
        return_value=MagicMock(__aenter__=MagicMock(return_value=mock_response)),
    ):
        info = await api.get_info()
        assert info["firmware_version"] == "17.0.1"


@pytest.mark.asyncio
async def test_api_reboot():
    """Test reboot method."""
    api = SwitchAPI("1.2.3.4", "test_token")

    mock_response = MagicMock()
    mock_response.status = 200

    with patch(
        "aiohttp.ClientSession.post",
        return_value=MagicMock(__aenter__=MagicMock(return_value=mock_response)),
    ):
        success = await api.reboot()
        assert success is True
