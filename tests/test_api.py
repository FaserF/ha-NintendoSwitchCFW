"""Tests for the Switch API."""

from unittest.mock import AsyncMock, MagicMock, patch

import pytest

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


@pytest.mark.asyncio
async def test_get_firmware_update_caching():
    """Test get_firmware_update caching to prevent GitHub rate limits."""
    mock_session = MagicMock()
    api = SwitchAPI("1.2.3.4", "test_token", session=mock_session)

    mock_response = MagicMock()
    mock_response.status = 200
    mock_response.json = AsyncMock(return_value={"tag_name": "v18.0.0"})

    with patch.object(
        mock_session,
        "get",
        return_value=MagicMock(__aenter__=AsyncMock(return_value=mock_response)),
    ) as mock_get:
        # First call fetches from remote
        res1 = await api.get_firmware_update("THZoria/NX_Firmware")
        assert res1["latest_version"] == "18.0.0"
        assert mock_get.call_count == 1

        # Second call uses cache within TTL without hitting session.get
        res2 = await api.get_firmware_update("THZoria/NX_Firmware")
        assert res2["latest_version"] == "18.0.0"
        assert mock_get.call_count == 1

        # Force call bypasses cache
        res3 = await api.get_firmware_update("THZoria/NX_Firmware", force=True)
        assert res3["latest_version"] == "18.0.0"
        assert mock_get.call_count == 2
