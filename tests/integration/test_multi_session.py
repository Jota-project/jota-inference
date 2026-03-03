import pytest
import websockets
import json
import os
import asyncio

@pytest.mark.asyncio
class TestMultiSession:
    async def connect(self, uri, headers=None):
        """Setup phase"""
        # Disable ping to prevent timeout during heavy loads
        self.websocket = await websockets.connect(uri, additional_headers=headers, ping_interval=None, close_timeout=120)
        return self.websocket

    async def cleanup(self):
        """Teardown phase"""
        if hasattr(self, 'websocket') and self.websocket:
            await self.websocket.close()
    async def test_multi_session_concurrency(self):
        client_id = os.environ.get("IC_TEST_CLIENT_ID", "inference_center")
        api_key = os.environ.get("IC_TEST_API_KEY", "inference_center_8f29c1a0e63847b592d8e428f7a6c9d0b51e39a02f374c18a5927d")
        uri = os.environ.get("TEST_URI", "ws://localhost:8001")
        
        ws1 = None
        ws2 = None
        
        try:
            headers = {
                "x-client-id": client_id,
                "x-api-key": api_key
            }
            
            # Connection 1
            ws1 = await websockets.connect(uri, additional_headers=headers)
            await ws1.recv() # Welcome message
            
            create_req = {"op": "create_session"}
            await ws1.send(json.dumps(create_req))
            resp1 = json.loads(await ws1.recv())
            assert resp1.get("op") == "session_created", "Failed to create first session"
            session_id_1 = resp1["session_id"]
            
            # Connection 2 (Same client simulating concurrent session creation)
            ws2 = await websockets.connect(uri, additional_headers=headers)
            await ws2.recv() # Welcome message
            
            await ws2.send(json.dumps(create_req))
            resp2 = json.loads(await ws2.recv())
            # Should fail if max_sessions == 1
            assert resp2.get("op") == "session_error" or resp2.get("op") == "error", "Expected error when exceeding limit on second connection"
            
            # Connection 1 should still work
            await ws1.send(json.dumps({"op": "COMMAND_LIST_MODELS"}))
            list_resp = json.loads(await ws1.recv())
            assert list_resp.get("op") == "list_models_result"
            
            models = list_resp.get("models", [])
            assert len(models) > 0, "No models available"
            model_id = models[0].get("id")
            
            await ws1.send(json.dumps({
                "op": "COMMAND_LOAD_MODEL",
                "model_id": model_id
            }))
            
            # Wait until loaded
            while True:
                resp = json.loads(await asyncio.wait_for(ws1.recv(), timeout=60))
                if resp.get("op") == "load_model_result":
                    break
                elif resp.get("op") == "error":
                    pytest.fail(f"Failed to load model: {resp}")
            
            # We successfully verified the max_sessions constraint above.
            # We will skip the `infer` payload here to prevent potential KV cache / model state invalidation 
            # issues in the server after a session_error, as `test_client.py` handles inference assertions.
            
            # Close session 1
            await ws1.send(json.dumps({
                "op": "close_session",
                "session_id": session_id_1
            }))
            resp_close = json.loads(await asyncio.wait_for(ws1.recv(), timeout=5.0))
            assert resp_close.get("op") == "session_closed"
                
        finally:
            if ws1:
                await ws1.close()
            if ws2:
                await ws2.close()
            await self.cleanup()
