import asyncio
import websockets
import json
import sys

async def test_connection():
    uri = "ws://localhost/api/inference/"
    print(f"Connecting to {uri} ...")
    
    try:
        async with websockets.connect(uri) as websocket:
            print("Connected successfully!")
            
            # 1. Wait for Hello
            try:
                greeting = await asyncio.wait_for(websocket.recv(), timeout=5.0)
                print(f"< {greeting}")
            except asyncio.TimeoutError:
                print("Timeout waiting for greeting")
                return

            print("Test PASSED: Connection established and greeting received.")
            
    except Exception as e:
        print(f"Test FAILED: {e}")
        sys.exit(1)

if __name__ == "__main__":
    asyncio.run(test_connection())
