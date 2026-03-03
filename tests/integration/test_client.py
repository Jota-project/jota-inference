import pytest
import websockets
import json
import os
import asyncio

@pytest.mark.asyncio
class TestClientInference:
    async def connect(self, uri, headers=None):
        """Setup phase"""
        self.websocket = await websockets.connect(uri, additional_headers=headers)
        return self.websocket

    async def cleanup(self):
        """Teardown phase"""
        if hasattr(self, 'websocket') and self.websocket:
            await self.websocket.close()

    async def test_full_inference_flow(self):
        uri = os.environ.get("TEST_URI", "ws://localhost:8001")
        client_id = os.environ.get("IC_TEST_CLIENT_ID", "inference_center")
        api_key = os.environ.get("IC_TEST_API_KEY", "test_key_placeholder")

        valid_headers = {
            "x-client-id": client_id,
            "x-api-key": api_key
        }

        try:
            # Setup
            await self.connect(uri, headers=valid_headers)
            
            # 1. Wait for Auth Success
            auth_response = await self.websocket.recv()
            
            # 3. Create Session
            create_session = {"op": "create_session"}
            await self.websocket.send(json.dumps(create_session))
            
            session_response = await self.websocket.recv()
            session_data = json.loads(session_response)
            session_id = session_data.get("session_id")
            
            assert session_id is not None, "Failed to create session"

            # 3.5 List Models and Load
            await self.websocket.send(json.dumps({"op": "COMMAND_LIST_MODELS"}))
            list_resp = json.loads(await self.websocket.recv())
            assert list_resp.get("op") == "list_models_result", f"Expected list_models_result, got {list_resp.get('op')}"
            
            models = list_resp.get("models", [])
            assert len(models) > 0, "No models available from JotaDB for testing"
            model_id = models[0].get("id")
            assert model_id is not None, "Model ID is missing"
            
            await self.websocket.send(json.dumps({
                "op": "COMMAND_LOAD_MODEL",
                "model_id": model_id
            }))
            
            # Wait until loaded
            while True:
                resp = json.loads(await asyncio.wait_for(self.websocket.recv(), timeout=60))
                if resp.get("op") == "load_model_result":
                    break
                elif resp.get("op") == "error":
                    pytest.fail(f"Failed to load model: {resp}")

            # 4. Send Infer Request
            req = {
                "op": "infer",
                "session_id": session_id,
                "prompt": "Solve this: If I have 3 apples and I eat 1, how many are left? Explain why.",
                "params": {
                    "temp": 0.8
                }
            }
            await self.websocket.send(json.dumps(req))

            # 5. Listen for tokens
            completed = False
            while True:
                msg = await self.websocket.recv()
                data = json.loads(msg)
                op = data.get("op")
                
                if op == "token":
                    pass
                elif op == "end":
                    assert "stats" in data
                    completed = True
                    break
                elif op == "metrics":
                    # Ignore metrics during inference
                    pass
                elif op == "error":
                    pytest.fail(f"Received error: {data}")
                    break

            assert completed, "Inference did not finish successfully"

            # 6. Close Session
            await self.websocket.send(json.dumps({
                "op": "close_session",
                "session_id": session_id
            }))
            
            close_resp = json.loads(await asyncio.wait_for(self.websocket.recv(), timeout=5.0))
            assert close_resp.get("op") == "session_closed"
            
        finally:
            # Teardown
            await self.cleanup()
