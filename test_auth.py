import asyncio
import websockets
import json

async def test_auth():
    """Test authentication with valid and invalid credentials"""
    uri = "ws://localhost:3000"
    
    print("=== Testing Authentication ===\n")
    
    # Test 1: Valid credentials
    print("Test 1: Valid credentials")
    async with websockets.connect(uri) as websocket:
        # Wait for hello
        greeting = await websocket.recv()
        print(f"< {greeting}")
        
        # Send valid auth
        auth = {
            "op": "auth",
            "client_id": "laptop_principal",
            "api_key": "sk_laptop_abc123def456ghi789jkl012mno345pqr678stu901vwx234yz"
        }
        await websocket.send(json.dumps(auth))
        print(f"> {json.dumps(auth, indent=2)}")
        
        response = await websocket.recv()
        print(f"< {response}")
        data = json.loads(response)
        
        if data.get("op") == "auth_success":
            print("✅ Valid credentials accepted\n")
        else:
            print("❌ Valid credentials rejected\n")
    
    # Test 2: Invalid credentials
    print("Test 2: Invalid credentials")
    async with websockets.connect(uri) as websocket:
        await websocket.recv()  # Skip hello
        
        auth = {
            "op": "auth",
            "client_id": "laptop_principal",
            "api_key": "wrong_key"
        }
        await websocket.send(json.dumps(auth))
        print(f"> Invalid credentials sent")
        
        response = await websocket.recv()
        data = json.loads(response)
        
        if data.get("op") == "auth_failed":
            print("✅ Invalid credentials rejected\n")
        else:
            print("❌ Invalid credentials accepted (SECURITY ISSUE!)\n")
    
    # Test 3: Operations without auth
    print("Test 3: Operations without authentication")
    async with websockets.connect(uri) as websocket:
        await websocket.recv()  # Skip hello
        
        # Try to create session without auth
        req = {"op": "create_session"}
        await websocket.send(json.dumps(req))
        print(f"> Attempted CREATE_SESSION without auth")
        
        response = await websocket.recv()
        data = json.loads(response)
        
        if data.get("op") == "error":
            print("✅ Unauthenticated operation blocked\n")
        else:
            print("❌ Unauthenticated operation allowed (SECURITY ISSUE!)\n")

asyncio.run(test_auth())
