import subprocess
import time
import sys
import os
import signal
import socket
import json

try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass # python-dotenv is not strictly required if running with inline envs

# Configuration
SERVER_BIN = "./build_fast/InferenceCore"
PORT = 8001  # Use a different port to avoid conflicts with running dev instances
HOST = "localhost"
URI = f"ws://{HOST}:{PORT}"

# Tests will be auto-discovered by pytest in tests/integration/

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
    global MODEL_PATH
    print(f"🚀 Starting InferenceCenter Test Runner...")
    
    # 1. Start the server
    print(f"📦 Launching server manually on port {PORT}...")
    
    # Check if binary exists, optionally compile it automatically or warn
    if not os.path.exists(SERVER_BIN):
        print(f"❌ Server binary not found at {SERVER_BIN}")
        print("   Attempting to build using build_fast...")
        
        # Build logic
        os.makedirs("build_fast", exist_ok=True)
        build_cmd = "./build_fast.sh"
        build_result = subprocess.run(build_cmd, shell=True)
        
        if build_result.returncode != 0:
            print(f"❌ Build failed.")
            sys.exit(1)

    # Prepare command
    cmd = [
        SERVER_BIN,
        "--port", str(PORT),
        "--ctx-size", "2048", # Increased to avoid KV cache full during concurrent test
        "--gpu-layers", "0"  # Use CPU for stability in CI/Test env if needed, or "-1" for auto
    ]
    
    # Start server process (write output to file to avoid pipe buffer deadlocks)
    server_log_file = open("server_test.log", "w+")
    server_proc = subprocess.Popen(
        cmd,
        stdout=server_log_file,
        stderr=subprocess.STDOUT,  # Redirect stderr to stdout stream
        text=True
    )
    
    try:
        print(f"⏳ Waiting for server to be ready at {HOST}:{PORT}...")
        if check_server_ready(HOST, PORT):
            print("✅ Server is up and running!")
        else:
            print("❌ Server failed to start within timeout.")
            print("--- Server Output ---")
            server_log_file.seek(0)
            print(server_log_file.read())
            raise RuntimeError("Server start failed")

        # 2. Run Tests
        print("\n🏗️  Running Tests with Pytest...\n")
        
        my_env = os.environ.copy()
        my_env["TEST_URI"] = URI
        if "IC_TEST_CLIENT_ID" not in my_env:
             my_env["IC_TEST_CLIENT_ID"] = "inference_center"
        if "IC_TEST_API_KEY" not in my_env:
             my_env["IC_TEST_API_KEY"] = "inference_center_8f29c1a0e63847b592d8e428f7a6c9d0b51e39a02f374c18a5927d"
        
        python_bin = sys.executable
        if os.path.exists("venv/bin/python"):
            python_bin = "venv/bin/python"
        elif os.path.exists(".venv/bin/python"):
             python_bin = ".venv/bin/python"

        result = subprocess.run(
            [python_bin, "-m", "pytest", "tests/integration/", "-v", "-s"],
            env=my_env,
        )
        
        if result.returncode == 0:
            print("\n✅ All tests PASSED")
        else:
            print("\n❌ Tests FAILED")
            print("--- Server Logic Output ---")
            server_log_file.seek(0)
            print(server_log_file.read())
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
