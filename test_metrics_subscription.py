import asyncio
import websockets
import json
import sys

async def test_metrics_subscription():
    """Test that metrics are only sent when subscribed."""
    uri = "ws://localhost:8001/"
    print(f"Connecting to {uri} ...")
    
    try:
        async with websockets.connect(uri) as websocket:
            print("✅ Connected")
            
            # 1. Wait for Hello
            greeting = await asyncio.wait_for(websocket.recv(), timeout=2.0)
            print(f"< {greeting}")

            # 2. Authenticate
            auth = {
                "op": "auth",
                "client_id": "laptop_principal",
                "api_key": "sk_laptop_abc123def456ghi789jkl012mno345pqr678stu901vwx234yz"
            }
            await websocket.send(json.dumps(auth))
            auth_response = await websocket.recv()
            print(f"< {auth_response}")
            
            # 3. Wait 3 seconds WITHOUT subscribing - should NOT receive metrics
            print("\n🔍 TEST 1: Waiting 3 seconds WITHOUT subscription...")
            try:
                msg = await asyncio.wait_for(websocket.recv(), timeout=3.0)
                data = json.loads(msg)
                if data.get("op") == "metrics":
                    print("❌ FAIL: Received metrics WITHOUT subscription!")
                    return False
            except asyncio.TimeoutError:
                print("✅ PASS: No metrics received (as expected)")
            
            # 4. Subscribe to metrics
            print("\n🔍 TEST 2: Subscribing to metrics...")
            subscribe = {"op": "subscribe_metrics"}
            await websocket.send(json.dumps(subscribe))
            
            # Should receive confirmation
            response = await asyncio.wait_for(websocket.recv(), timeout=2.0)
            print(f"< {response}")
            data = json.loads(response)
            if data.get("op") != "metrics_subscribed":
                print(f"❌ FAIL: Expected metrics_subscribed, got {data.get('op')}")
                return False
            print("✅ PASS: Subscription confirmed")
            
            # 5. Wait for metrics (should receive within 2 seconds)
            print("\n🔍 TEST 3: Waiting for metrics after subscription...")
            msg = await asyncio.wait_for(websocket.recv(), timeout=3.0)
            data = json.loads(msg)
            if data.get("op") != "metrics":
                print(f"❌ FAIL: Expected metrics, got {data.get('op')}")
                return False
            print(f"✅ PASS: Received metrics: GPU temp={data['gpu']['temp']}°C")
            
            # 6. Unsubscribe
            print("\n🔍 TEST 4: Unsubscribing from metrics...")
            unsubscribe = {"op": "unsubscribe_metrics"}
            await websocket.send(json.dumps(unsubscribe))
            
            response = await asyncio.wait_for(websocket.recv(), timeout=2.0)
            print(f"< {response}")
            data = json.loads(response)
            if data.get("op") != "metrics_unsubscribed":
                print(f"❌ FAIL: Expected metrics_unsubscribed, got {data.get('op')}")
                return False
            print("✅ PASS: Unsubscription confirmed")
            
            # 7. Wait 3 seconds - should NOT receive metrics anymore
            print("\n🔍 TEST 5: Waiting 3 seconds after unsubscribe...")
            try:
                msg = await asyncio.wait_for(websocket.recv(), timeout=3.0)
                data = json.loads(msg)
                if data.get("op") == "metrics":
                    print("❌ FAIL: Still receiving metrics after unsubscribe!")
                    return False
            except asyncio.TimeoutError:
                print("✅ PASS: No metrics received after unsubscribe")
            
            print("\n" + "="*50)
            print("🎉 ALL TESTS PASSED!")
            print("="*50)
            return True
            
    except Exception as e:
        print(f"❌ Test FAILED: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    result = asyncio.run(test_metrics_subscription())
    sys.exit(0 if result else 1)
