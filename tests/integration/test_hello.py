import pytest
import websockets
import json
import os
import asyncio

@pytest.mark.asyncio
class TestHello:
    async def connect(self, uri, headers=None):
        """Setup phase: create connection"""
        self.websocket = await websockets.connect(uri, additional_headers=headers)
        return self.websocket

    async def cleanup(self):
        """Teardown phase: close connection"""
        if hasattr(self, 'websocket') and self.websocket:
            await self.websocket.close()

    async def test_hello_no_auth(self):
        uri = os.environ.get("TEST_URI", "ws://localhost:8001")
        try:
            # Setup
            headers = {
                "x-client-id": os.environ.get("IC_TEST_CLIENT_ID", "inference_center"),
                "x-api-key": os.environ.get("IC_TEST_API_KEY", "inference_center_8f29c1a0e63847b592d8e428f7a6c9d0b51e39a02f374c18a5927d")
            }
            await self.connect(uri, headers=headers)
            
            # Wait for initial HELLO from server
            initial_message = await self.websocket.recv()
            
            # Send HELLO request
            hello_request = {"op": "hello"}
            await self.websocket.send(json.dumps(hello_request))
            
            # Receive response
            response = await self.websocket.recv()
            response_data = json.loads(response)
            
            # Verify response
            assert response_data.get("op") == "hello"
            assert "status" in response_data
            assert "uptime_seconds" in response_data
            assert "message" in response_data
        finally:
            # Teardown
            await self.cleanup()
