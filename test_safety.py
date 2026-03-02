import asyncio
import websockets
import json
import logging
import os
import uuid
import sys

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(message)s')

API_KEY = os.environ.get("IC_TEST_API_KEY", "inference_center_8f29c1a0e63847b592d8e428f7a6c9d0b51e39a02f374c18a5927d")
CLIENT_ID = os.environ.get("IC_TEST_CLIENT_ID", "inference_center")
URI = os.environ.get("TEST_URI", "ws://localhost:8001")

async def test_safety_checks():
    logging.info(f"Connecting to {URI}...")
    headers = {
        "x-client-id": CLIENT_ID,
        "x-api-key": API_KEY
    }
    
    async with websockets.connect(URI, additional_headers=headers) as websocket:
        # Wait for auth success
        auth_msg = await websocket.recv()
        logging.info(f"Auth response: {auth_msg}")
        
        # 1. Create session
        session_id = str(uuid.uuid4())
        await websocket.send(json.dumps({
            "op": "create_session"
        }))
        session_resp = json.loads(await websocket.recv())
        logging.info(f"Session response: {session_resp}")
        real_session_id = session_resp.get("session_id")

        if not real_session_id:
            logging.error("Failed to create session.")
            return

        # 2. Try inference (no model loaded / default model might be loaded, let's see)
        # We'll just rely on the server state. If a model is loaded, we'll try something else.
        await websocket.send(json.dumps({
            "op": "infer",
            "session_id": real_session_id,
            "prompt": "Hello",
            "max_tokens": 10
        }))
        
        infer_resp = json.loads(await websocket.recv())
        logging.info(f"Infer response: {infer_resp}")

        # If it was an error about no model, good.
        # Let's force load a model now.
        logging.info("Loading model...")
        await websocket.send(json.dumps({
            "op": "load_model",
            "model_id": "LFM-1.2B" # Just guessing a known good ID or we can expect an error
        }))

        # Wait until it's loaded or error
        while True:
            resp = json.loads(await websocket.recv())
            logging.info(f"Load model resp: {resp}")
            if resp.get("op") in ["load_model_result", "error"]:
                break
        
        # Stop session 
        await websocket.send(json.dumps({
            "op": "close_session",
            "session_id": real_session_id
        }))
        
if __name__ == "__main__":
    asyncio.run(test_safety_checks())
