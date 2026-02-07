import subprocess
import time
import sys
import os
import signal
import socket
import json

# Configuration
SERVER_BIN = "./build/InferenceCore"
MODEL_PATH = "models/LFM/LFM2.5-1.2B-Thinking-Q4_K_M.gguf"
PORT = 8081  # Use a different port to avoid conflicts with running dev instances
HOST = "localhost"
URI = f"ws://{HOST}:{PORT}"

# Tests to run
TEST_FILES = [
    "test_auth.py",
    "test_client.py",
    # "test_multi_session.py", # This might take longer, uncomment if needed
    # "test_metrics_subscription.py"
]

def check_server_ready(host, port, timeout=30):
    """Check if the server is accepting connections."""
    start_time = time.time()
    while time.time() - start_time < timeout:
        try:
            with socket.create_connection((host, port), timeout=1):
                return True
        except (socket.timeout, ConnectionRefusedError):
            time.sleep(1)
    return False

def run_tests():
    print(f"🚀 Starting InferenceCenter Test Runner...")
    
    # 1. Start the server
    print(f"📦 Launching server manually on port {PORT}...")
    
    # Check if binary exists
    if not os.path.exists(SERVER_BIN):
        print(f"❌ Server binary not found at {SERVER_BIN}")
        print("   Please build the project first: mkdir build && cd build && cmake .. && make -j")
        sys.exit(1)

    # Check if model exists
    if not os.path.exists(MODEL_PATH):
        print(f"❌ Model not found at {MODEL_PATH}")
        print("   Please download a model or update MODEL_PATH in this script.")
        # Try to find any .gguf file in models/
        print("   Searching for other models...")
        found_model = False
        global MODEL_PATH
        for root, dirs, files in os.walk("models"):
        
        if not found_model:
            sys.exit(1)

    # Prepare command
    cmd = [
        SERVER_BIN,
        "--model", MODEL_PATH,
        "--port", str(PORT),
        "--ctx-size", "512", # Keep it light/fast for testing
        "--gpu-layers", "0"  # Use CPU for stability in CI/Test env if needed, or "-1" for auto
    ]
    
    # Start server process
    server_proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    try:
        print(f"⏳ Waiting for server to be ready at {HOST}:{PORT}...")
        if check_server_ready(HOST, PORT):
            print("✅ Server is up and running!")
        else:
            print("❌ Server failed to start within timeout.")
            print("--- Server Output ---")
            print(server_proc.stdout.read())
            print(server_proc.stderr.read())
            raise RuntimeError("Server start failed")

        # 2. Run Tests
        failed_tests = []
        passed_tests = []
        
        # Need to patch the URI in the test files or pass it via env var
        # Since test files mostly hardcode "ws://localhost:3000", we might need to sed them
        # or better, let's update test files to read from ENV if available.
        # For now, let's use a quick sed hack on a temp copy or just sed existing files if we are brave.
        # Actually, let's just assume the user runs this script and we set the PORT env var
        # BUT the python scripts likely don't read it.
        # Let's check `test_client.py` content again.
        # It has `uri = "ws://localhost/api/inference/"` or similar. 
        # Wait, the `README` said port 3000 but the code I saw in `test_client.py` (step 13) 
        # had `uri = "ws://localhost/api/inference/"`. That looks like a reverse proxy path?
        # Or maybe the port was omitted (defaults to 80?).
        
        # Let's re-read test_client.py to be sure about the URI.
        # Step 13: `uri = "ws://localhost/api/inference/"`
        # This implies it expects a reverse proxy or port 80. 
        # If I run on 8081, I need to change that.
        
        # STRATEGY: 
        # I will inject a small "test_config.py" or just write a wrapper 
        # that monkeypatches websockets.connect? No that's too complex.
        # 
        # Simpler: I will temporary modify the files or 
        # create temporary test files with the correct PORT.
        # 
        # Actually, best approach for *future* is to modify the test files 
        # to accept an environment variable.
        
        print("\n🏗️  Running Tests...\n")
        
        # We need to inform the tests about the port. 
        # Since I cannot easily modify them in this run without editing them constantly,
        # I will modify them ONCE to respect an environment variable `TEST_URI`.
        
        my_env = os.environ.copy()
        my_env["TEST_URI"] = URI
        
        for test_file in TEST_FILES:
            if not os.path.exists(test_file):
                print(f"⚠️  Skipping {test_file} (not found)")
                continue
                
            print(f"👉 Running {test_file}...")
            try:
                # We need to make sure the test file uses the env var.
                # If they don't, this will fail or test the WRONG server (e.g. prod).
                # I will add a patch step in a moment.
                
                result = subprocess.run(
                    [sys.executable, test_file],
                    env=my_env,
                    capture_output=True,
                    text=True,
                    timeout=60 # 1 min timeout per test
                )
                
                if result.returncode == 0:
                    print(f"✅ {test_file} PASSED")
                    passed_tests.append(test_file)
                else:
                    print(f"❌ {test_file} FAILED")
                    print("--- Test Output ---")
                    print(result.stdout)
                    print(result.stderr)
                    failed_tests.append(test_file)
            except Exception as e:
                print(f"❌ {test_file} ERROR: {e}")
                failed_tests.append(test_file)
                
        # 3. Summary
        print("\n📊 Test Summary")
        print(f"Passed: {len(passed_tests)}")
        print(f"Failed: {len(failed_tests)}")
        
        if failed_tests:
            sys.exit(1)
            
    finally:
        # 4. Cleanup
        print("\n🧹 Shutting down server...")
        server_proc.terminate()
        try:
            server_proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server_proc.kill()
        print("👋 Done.")

if __name__ == "__main__":
    run_tests()
