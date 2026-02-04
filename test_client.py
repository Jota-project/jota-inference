import asyncio
import websockets
import json

async def test():
    uri = "ws://localhost:3000"
    async with websockets.connect(uri) as websocket:
        # 1. Wait for Hello
        greeting = await websocket.recv()
        print(f"< {greeting}")

        # 2. Authenticate
        auth = {
            "op": "auth",
            "client_id": "laptop_principal",
            "api_key": "sk_laptop_abc123def456ghi789jkl012mno345pqr678stu901vwx234yz"
        }
        await websocket.send(json.dumps(auth))
        print(f"> {auth}")
        
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
