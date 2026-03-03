import pytest
import websockets
import json
import os
import asyncio

@pytest.mark.asyncio
class TestMetricsSubscription:
    async def connect(self, uri, headers=None):
        """Setup phase"""
        # Disable ping to prevent timeout during heavy loads
        self.websocket = await websockets.connect(uri, additional_headers=headers, ping_interval=None, close_timeout=120)
        return self.websocket

    async def cleanup(self):
        """Teardown phase"""
        if hasattr(self, 'websocket') and self.websocket:
            await self.websocket.close()

    async def test_metrics_subscription_flow(self):
        uri = os.environ.get("TEST_URI", "ws://localhost:8001")
        client_id = os.environ.get("IC_TEST_CLIENT_ID", "inference_center")
        api_key = os.environ.get("IC_TEST_API_KEY", "inference_center_8f29c1a0e63847b592d8e428f7a6c9d0b51e39a02f374c18a5927d")
        
        try:
            # Setup
            headers = {
                "x-client-id": client_id,
                "x-api-key": api_key
            }
            await self.connect(uri, headers=headers)
            
            # 1. Wait for Hello
            greeting = await asyncio.wait_for(self.websocket.recv(), timeout=2.0)
            
            # 2. Wait for confirmation (optional but handled by handshake now, but server might send a welcome message)
            # We already authenticated via headers, so we can go straight to step 3
            
            # 3. Wait 3 seconds WITHOUT subscribing - should NOT receive metrics
            try:
                msg = await asyncio.wait_for(self.websocket.recv(), timeout=3.0)
                data = json.loads(msg)
                if data.get("op") == "metrics":
                    pytest.fail("Received metrics WITHOUT subscription!")
            except asyncio.TimeoutError:
                pass # Expected
            
            # 4. Subscribe to metrics
            subscribe = {"op": "subscribe_metrics"}
            await self.websocket.send(json.dumps(subscribe))
            
            # Should receive confirmation
            response = await asyncio.wait_for(self.websocket.recv(), timeout=2.0)
            data = json.loads(response)
            assert data.get("op") == "metrics_subscribed", f"Expected metrics_subscribed, got {data.get('op')}"
            
            # 5. Wait for metrics (should receive within 2 seconds)
            msg = await asyncio.wait_for(self.websocket.recv(), timeout=3.0)
            data = json.loads(msg)
            assert data.get("op") == "metrics", f"Expected metrics, got {data.get('op')}"
            assert "gpu" in data
            
            # 6. Unsubscribe
            unsubscribe = {"op": "unsubscribe_metrics"}
            await self.websocket.send(json.dumps(unsubscribe))
            
            response = await asyncio.wait_for(self.websocket.recv(), timeout=2.0)
            data = json.loads(response)
            assert data.get("op") == "metrics_unsubscribed", f"Expected metrics_unsubscribed, got {data.get('op')}"
            
            # 7. Wait 3 seconds - should NOT receive metrics anymore
            try:
                msg = await asyncio.wait_for(self.websocket.recv(), timeout=3.0)
                data = json.loads(msg)
                if data.get("op") == "metrics":
                    pytest.fail("Still receiving metrics after unsubscribe!")
            except asyncio.TimeoutError:
                pass # Expected
        finally:
            # Teardown
            await self.cleanup()
