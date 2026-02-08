#!/usr/bin/env python3
"""
Test script to verify HELLO operation works without authentication
"""
import asyncio
import websockets
import json

async def test_hello():
    uri = "ws://localhost/api/inference"
    
    try:
        async with websockets.connect(uri) as websocket:
            # Wait for initial HELLO from server
            initial_message = await websocket.recv()
            print("📨 Received initial message from server:")
            print(json.dumps(json.loads(initial_message), indent=2))
            print()
            
            # Send HELLO request (without authentication)
            hello_request = {"op": "hello"}
            print("📤 Sending HELLO request (no credentials):")
            print(json.dumps(hello_request, indent=2))
            await websocket.send(json.dumps(hello_request))
            print()
            
            # Receive response
            response = await websocket.recv()
            print("📨 Received HELLO response:")
            response_data = json.loads(response)
            print(json.dumps(response_data, indent=2))
            print()
            
            # Verify response
            if response_data.get("op") == "hello":
                print("✅ SUCCESS: HELLO operation works without authentication!")
                print(f"   - Server status: {response_data.get('status')}")
                print(f"   - Uptime: {response_data.get('uptime_seconds')} seconds")
                print(f"   - Message: {response_data.get('message')}")
            else:
                print("❌ FAILED: Unexpected response")
                
    except Exception as e:
        print(f"❌ Error: {e}")
        print("\n⚠️  Make sure the InferenceCore server is running on port 3000")

if __name__ == "__main__":
    asyncio.run(test_hello())
