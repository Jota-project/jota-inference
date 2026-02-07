import asyncio
import websockets
import json
import time
import os

async def test_multi_session():
    """Test multiple concurrent sessions"""
    uri = os.environ.get("TEST_URI", "ws://localhost/api/inference/")
    
    print("=== Testing Multiple Concurrent Sessions ===\n")
    print(f"Connecting to: {uri}")
    
    # Disable ping to prevent timeout during heavy loads
    async with websockets.connect(uri, ping_interval=None, close_timeout=120) as websocket:
        # 1. Authenticate
        await websocket.recv()  # Hello
        
        auth = {
            "op": "auth",
            "client_id": "desktop_oficina",  # This client has max_sessions: 4
            "api_key": "sk_desktop_xyz789uvw012abc345def678ghi901jkl234mno567pqr890st"
        }
        await websocket.send(json.dumps(auth))
        response = await websocket.recv()
        auth_data = json.loads(response)
        
        if auth_data.get("op") != "auth_success":
            print("❌ Authentication failed")
            return
        
        print(f"✅ Authenticated as {auth_data['client_id']}")
        print(f"   Max sessions: {auth_data['max_sessions']}\n")
        
        # 2. Create multiple sessions
        session_ids = []
        for i in range(3):
            create_req = {"op": "create_session"}
            await websocket.send(json.dumps(create_req))
            response = await websocket.recv()
            data = json.loads(response)
            
            if data.get("op") == "session_created":
                session_id = data["session_id"]
                session_ids.append(session_id)
                print(f"✅ Created session {i+1}: {session_id}")
            else:
                print(f"❌ Failed to create session {i+1}")
        
        print(f"\n📊 Created {len(session_ids)} sessions\n")
        
        # 3. Run concurrent inference on all sessions
        print("Running concurrent inference on all sessions...\n")
        
        prompts = [
            "Count from 1 to 5",
            "List 3 colors",
            "Name 2 animals"
        ]
        
        # Send all inference requests
        for i, session_id in enumerate(session_ids):
            infer_req = {
                "op": "infer",
                "session_id": session_id,
                "prompt": prompts[i],
                "params": {"temp": 0.7}
            }
            await websocket.send(json.dumps(infer_req))
            print(f"→ Session {i+1}: {prompts[i]}")
        
        print("\n📥 Receiving responses...\n")
        
        # Collect responses
        session_outputs = {sid: [] for sid in session_ids}
        active_sessions = set(session_ids)
        
        start_time = time.time()
        
        while active_sessions:
            try:
                response = await asyncio.wait_for(websocket.recv(), timeout=30)
                data = json.loads(response)
                op = data.get("op")
                
                if op == "token":
                    sid = data.get("session_id")
                    content = data.get("content", "")
                    session_outputs[sid].append(content)
                    print(content, end="", flush=True)
                
                elif op == "end":
                    sid = data.get("session_id")
                    active_sessions.discard(sid)
                    stats = data.get("stats", {})
                    idx = session_ids.index(sid) + 1
                    print(f"\n\n✅ Session {idx} complete:")
                    print(f"   Tokens: {stats.get('tokens', 0)}")
                    print(f"   TPS: {stats.get('tps', 0):.2f}")
                    print(f"   TTFT: {stats.get('ttft_ms', 0)}ms\n")
                
                elif op == "metrics":
                    pass  # Ignore periodic metrics
                
            except asyncio.TimeoutError:
                print("⚠️  Timeout waiting for response")
                break
        
        elapsed = time.time() - start_time
        print(f"\n⏱️  Total time: {elapsed:.2f}s")
        print(f"📊 All {len(session_ids)} sessions processed in parallel\n")
        
        # 4. Close sessions
        for i, session_id in enumerate(session_ids):
            close_req = {
                "op": "close_session",
                "session_id": session_id
            }
            await websocket.send(json.dumps(close_req))
            response = await websocket.recv()
            data = json.loads(response)
            
            if data.get("op") == "session_closed":
                print(f"✅ Closed session {i+1}")

asyncio.run(test_multi_session())
