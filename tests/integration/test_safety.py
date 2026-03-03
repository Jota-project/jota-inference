import pytest
import websockets
import json
import os
import asyncio
import uuid

@pytest.mark.asyncio
class TestSafetyChecks:
    async def connect(self, uri, headers=None):
        """Setup phase"""
        self.websocket = await websockets.connect(uri, additional_headers=headers)
        return self.websocket

    async def cleanup(self):
        """Teardown phase"""
        if hasattr(self, 'websocket') and self.websocket:
            await self.websocket.close()

    async def test_safety_checks(self):
        uri = os.environ.get("TEST_URI", "ws://localhost:8001")
        client_id = os.environ.get("IC_TEST_CLIENT_ID", "inference_center")
        api_key = os.environ.get("IC_TEST_API_KEY", "inference_center_8f29c1a0e63847b592d8e428f7a6c9d0b51e39a02f374c18a5927d")
        
        headers = {
            "x-client-id": client_id,
            "x-api-key": api_key
        }
        try:
            # Setup
            await self.connect(uri, headers=headers)
            
            # Wait for auth success
            auth_msg = await self.websocket.recv()
            data = json.loads(auth_msg)
            assert data.get("op") == "auth_success"
            
            # 1. Create session
            session_id = str(uuid.uuid4())
            await self.websocket.send(json.dumps({
                "op": "create_session"
            }))
            session_resp = json.loads(await self.websocket.recv())
            real_session_id = session_resp.get("session_id")

            assert real_session_id is not None, "Failed to create session."

            # 2. Try inference
            await self.websocket.send(json.dumps({
                "op": "infer",
                "session_id": real_session_id,
                "prompt": "Hello",
                "max_tokens": 10
            }))
            
            infer_resp = json.loads(await self.websocket.recv())
            
            # Wait for the inference request to complete (it should probably fail quickly because no model is loaded, but we must wait for 'end' or 'error')
            while True:
                if infer_resp.get("op") in ["end", "error"]:
                    break
                infer_resp = json.loads(await asyncio.wait_for(self.websocket.recv(), timeout=10))

            # 2.5 List models first
            await self.websocket.send(json.dumps({"op": "COMMAND_LIST_MODELS"}))
            list_resp = json.loads(await self.websocket.recv())
            models = list_resp.get("models", [])
            assert len(models) > 0, "No models available to test load"
            target_model_id = models[0]["id"]

            # 3. Load model (this triggers safety load checks)
            await self.websocket.send(json.dumps({
                "op": "COMMAND_LOAD_MODEL",
                "model_id": target_model_id 
            }))

            # Wait until it's loaded or error
            while True:
                resp = json.loads(await asyncio.wait_for(self.websocket.recv(), timeout=60))
                if resp.get("op") in ["load_model_result", "error"]:
                    break
            
            # Stop session 
            await self.websocket.send(json.dumps({
                "op": "close_session",
                "session_id": real_session_id
            }))
            
        finally:
            # Teardown
            await self.cleanup()
