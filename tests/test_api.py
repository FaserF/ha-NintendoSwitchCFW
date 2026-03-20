"""Tests for the Switch API."""

import pytest
from unittest.mock import patch, MagicMock, AsyncMock
from custom_components.switch_cfw.api import SwitchAPI


@pytest.mark.asyncio
async def test_api_get_info():
    """Test get_info method."""
    mock_session = MagicMock()
    api = SwitchAPI("1.2.3.4", "test_token", session=mock_session)

    mock_response = MagicMock()
    mock_response.status = 200
    mock_response.json = AsyncMock(return_value={"firmware_version": "17.0.1"})

    with patch.object(
        mock_session,
        "get",
        return_value=MagicMock(__aenter__=AsyncMock(return_value=mock_response)),
    ):
        info = await api.get_info()
        assert info["firmware_version"] == "17.0.1"


@pytest.mark.asyncio
async def test_api_reboot():
    """Test reboot method."""
    mock_session = MagicMock()
    api = SwitchAPI("1.2.3.4", "test_token", session=mock_session)

    mock_response = MagicMock()
    mock_response.status = 200

    with (
        patch.object(
            mock_session,
            "post",
            return_value=MagicMock(__aenter__=AsyncMock(return_value=mock_response)),
        ),
        patch.object(mock_response, "json", AsyncMock(return_value={"status": "ok"})),
    ):
        success = await api.reboot()
        assert success is True
