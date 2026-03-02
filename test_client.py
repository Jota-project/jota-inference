import asyncio
import websockets
import json
import os

async def test():
    uri = os.environ.get("TEST_URI", "ws://green-house.local/api/inference/")
    client_id = os.environ.get("IC_TEST_CLIENT_ID", "inference_center")
    api_key = os.environ.get("IC_TEST_API_KEY", "test_key_placeholder")

    print(f"Connecting to: {uri}")
    valid_headers = {
        "x-client-id": client_id,
        "x-api-key": api_key
    }

    # Disable ping to avoid timeout during blocking server operations (model load)
    async with websockets.connect(uri, additional_headers=valid_headers) as websocket:
        # 1. Wait for Auth Success
        auth_response = await websocket.recv()
        print(f"< {auth_response}")
        
        # 3. Create Session
        create_session = {"op": "create_session"}
        await websocket.send(json.dumps(create_session))
        print(f"> {create_session}")
        
        session_response = await websocket.recv()
        print(f"< {session_response}")
        session_data = json.loads(session_response)
        session_id = session_data.get("session_id")
        
        if not session_id:
            print("Failed to create session")
            return

        # 4. Send Infer Request
        req = {
            "op": "infer",
            "session_id": session_id,
            "prompt": "Solve this: If I have 3 apples and I eat 1, how many are left? Explain why.",
            "params": {
                "temp": 0.8
            }
        }
        await websocket.send(json.dumps(req))
        print(f"> {req}")

        # 5. Listen for tokens
        while True:
            msg = await websocket.recv()
            data = json.loads(msg)
            op = data.get("op")
            
            if op == "token":
                print(data["content"], end="", flush=True)
            elif op == "end":
                print("\n[DONE]")
                print(f"STATS: {json.dumps(data.get('stats'), indent=2)}")
                break
            elif op == "metrics":
                # Ignore metrics during inference
                pass
            elif op == "error":
                print(f"\n[ERROR] {data}")
                break
            else:
                print(f"< {msg}")

asyncio.run(test())
