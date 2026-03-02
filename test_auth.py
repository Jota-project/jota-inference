import asyncio
import websockets
import json
import os

async def test_auth():
    """Test authentication with valid and invalid credentials"""
    uri = os.environ.get("TEST_URI", "ws://localhost/api/inference/")
    client_id = os.environ.get("IC_TEST_CLIENT_ID", "inference_center")
    api_key = os.environ.get("IC_TEST_API_KEY", "test_key_placeholder")

    print(f"Connecting to: {uri}")
    
    print("=== Testing Authentication ===\n")
    
    # Test 1: Valid credentials
    print("Test 1: Valid credentials")
    valid_headers = {
        "x-client-id": client_id,
        "x-api-key": api_key
    }
    
    try:
        async with websockets.connect(uri, additional_headers=valid_headers) as websocket:
            response = await websockets.recv()
            print(f"< {response}")
            data = json.loads(response)
            
            if data.get("op") == "auth_success":
                print("✅ Valid credentials accepted\\n")
            else:
                print("❌ Valid credentials rejected\\n")
    except Exception as e:
        print(f"❌ Valid credentials threw exception: {e}\\n")
    
    # Test 2: Invalid credentials
    print("Test 2: Invalid credentials")
    invalid_headers = {
        "x-client-id": client_id,
        "x-api-key": "wrong_key"
    }
    
    try:
        async with websockets.connect(uri, additional_headers=invalid_headers) as websocket:
            print("❌ Invalid credentials accepted (SECURITY ISSUE!)\\n")
    except websockets.exceptions.InvalidStatus as e:
        if e.response.status_code == 401:
            print("✅ Invalid credentials rejected with 401\\n")
        else:
            print(f"❌ Invalid credentials rejected with wrong error: {e}\\n")
    except Exception as e:
        print(f"❌ Invalid credentials threw unexpected exception: {e}\\n")
    
    # Test 3: Operations without auth
    print("Test 3: Operations without authentication")
    try:
        async with websockets.connect(uri) as websocket:
            print("❌ Unauthenticated connection allowed (SECURITY ISSUE!)\\n")
    except websockets.exceptions.InvalidStatus as e:
        if e.response.status_code == 401:
            print("✅ Unauthenticated connection rejected with 401\\n")
        else:
            print(f"❌ Unauthenticated connection rejected with wrong error: {e}\\n")

asyncio.run(test_auth())
