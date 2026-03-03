# Testing Guide

This project maintains a robust testing strategy combining C++ Unit Tests (Catch2) and Python Integration Tests.

## 🚀 Quick Run

To run **ALL** integration tests with automatic server management:

```bash
python3 run_all_tests.py
```
*Note: `run_all_tests.py` uses `pytest` under the hood to auto-discover all tests inside `tests/integration/`, handles the temporary start/stop of the `InferenceCore` server (using `build_test/`), and outputs server `stderr` logs if any test fails.*

---

## 🧪 Test Inventory

### 1. Unit Tests (C++)
Located in `tests/`, built with `cmake`, and executed via `./build/run_tests`.
Uses **Catch2 v3.4.0**.

| File | Description |
| :--- | :--- |
| `tests/test_protocol.cpp` | **Protocol Verification**: Validates JSON serialization/deserialization of WebSocket messages and ensures OpCode constants match the spec. |
| `tests/test_auth.cpp` | **Authentication Logic**: Tests `ClientAuth` class methods (`loadConfig`, `authenticate`, `getClientConfig`) in isolation using temporary config files. |

### 2. Integration Tests (Python - Pytest)
Located in `tests/integration/`. These tests validate the full end-to-end flow of the `InferenceCore` server over WebSockets, interacting with the real local `JotaDB` SQLite database for authentication and model data.

All tests are written as `pytest` asynchronous classes (`@pytest.mark.asyncio`). They follow a strict setup/cleanup cycle:
- **`connect()`**: Establishes the WebSocket connection and injects authentication headers (`x-client-id`, `x-api-key`).
- **`cleanup()`**: Executed in a `finally` block to guarantee the socket is gracefully closed, preventing resource leaks.

| File | Description | Key Scenarios / Inner Workings |
| :--- | :--- | :--- |
| `test_auth.py` | **Security flows** | Valid login, Invalid logic, Unauthenticated access blocked. Verifies that the server correctly drops connections that lack valid HTTP headers. |
| `test_hello.py` | **Handshake** | Ensures the server correctly identifies the client and the protocol version using the `hello` opcode after connection. |
| `test_client.py` | **Full Lifecycle** | Auth via headers → Ask for Models list (`COMMAND_LIST_MODELS`) → Load Model (`COMMAND_LOAD_MODEL`) → Create Session → Inference → Receive Tokens → Close Session. |
| `test_safety.py` | **Safety & Edge Cases** | Verifies inference rejection when no model is loaded and model switch rejection during active inference. |
| `test_multi_session.py` | **Concurrency limits** | Verifies the `max_sessions: 1` constraint from the database database. Ensures a single connection can create one session, but strictly rejects overlapping simultaneous sessions with a `session_error`. |
| `test_metrics_subscription.py` | **Real-time Data** | Verifies metrics are only received when subscribed and stop after unsubscribing. |

---

## ⚙️ Environment Variables (Python Tests)

The integration tests no longer use hardcoded credentials. You can override testing targets by exporting the following variables:

| Variable | Default Value (fallback) | Description |
| :--- | :--- | :--- |
| `TEST_URI` | `ws://localhost:8001` | The WebSocket URI of the InferenceCore server. |
| `IC_TEST_CLIENT_ID` | `inference_center` | The ID of the API client registered in the database. |
| `IC_TEST_API_KEY` | `inference_center_8f...` | The secure API key mapping to the test client. |

---

## 🛠️ How to Run Manually

### Unit Tests (C++)

```bash
cd build
make run_tests
./run_tests
```

### Integration Tests (Python)

If you want to run a specific test file explicitly instead of `run_all_tests.py`, you must launch the server yourself first:

1.  **Start Server** (using `build_test` for faster execution):
    ```bash
    ./build_test/InferenceCore --port 8001 --ctx-size 2048 --gpu-layers 0
    ```
    *Note: The `--ctx-size 2048` is important for concurrent inference testing to avoid KV Cache memory limits.*

2.  **Run specific Pytest script** in another terminal:
    ```bash
    source venv/bin/activate
    pytest tests/integration/test_client.py -v -s
    ```

---

## 🧠 Test Internal Logic & Architecture Updates

- **Header Authentication**: As of the recent refactor, clients NO LONGER send an `{"op": "auth"}` payload. Authentication mapping is executed instantly over the HTTP Upgrade connection using `x-client-id` and `x-api-key`.
- **Dynamic Model Loading**: Tests no longer assume a default model is hardcoded via CLI parameters. Tests dynamically send `COMMAND_LIST_MODELS`, extract the top available GGUF model from JotaDB, and instruct the server to load it into VRAM using `COMMAND_LOAD_MODEL` before conducting inference.
- **Graceful Async Awaits**: Replaced legacy `teardown_method` with custom explicit `cleanup()` coroutines to ensure Python 3.12 warnings about unawaited tasks are squashed safely.
