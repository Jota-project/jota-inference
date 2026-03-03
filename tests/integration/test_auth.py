import pytest
import websockets
import json
import os
import asyncio

@pytest.mark.asyncio
class TestAuth:
    async def connect(self, uri, headers=None):
        """Setup phase: create connection"""
        self.websocket = await websockets.connect(uri, additional_headers=headers)
        return self.websocket

    async def cleanup(self):
        """Teardown phase: close connection"""
        if hasattr(self, 'websocket') and self.websocket:
            await self.websocket.close()

    @pytest.fixture(autouse=True)
    def setup_vars(self):
        self.uri = os.environ.get("TEST_URI", "ws://localhost:8001")
        self.client_id = os.environ.get("IC_TEST_CLIENT_ID", "inference_center")
        self.api_key = os.environ.get("IC_TEST_API_KEY", "test_key_placeholder")

    async def test_valid_credentials(self):
        valid_headers = {
            "x-client-id": self.client_id,
            "x-api-key": self.api_key
        }
        try:
            # Setup
            await self.connect(self.uri, headers=valid_headers)
            
            # Wait for auth response
            response = await self.websocket.recv()
            data = json.loads(response)
            
            assert data.get("op") == "auth_success"
        finally:
            # Teardown
            await self.cleanup()

    async def test_invalid_credentials(self):
        invalid_headers = {
            "x-client-id": self.client_id,
            "x-api-key": "wrong_key"
        }
        try:
            # Setup / Test
            with pytest.raises(websockets.exceptions.InvalidStatus) as exc_info:
                await self.connect(self.uri, headers=invalid_headers)
            
            assert exc_info.value.response.status_code == 401
        finally:
            # Teardown
            await self.cleanup()

    async def test_unauthenticated_connection(self):
        try:
            # Setup / Test
            with pytest.raises(websockets.exceptions.InvalidStatus) as exc_info:
                await self.connect(self.uri)
                
            assert exc_info.value.response.status_code == 401
        finally:
            # Teardown
            await self.cleanup()
