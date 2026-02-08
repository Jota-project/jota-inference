# InferenceCenter

High-performance inference engine with **parallel session support** and **API token authentication**, optimized for resource-constrained GPUs (GTX 1060 3GB). Built with `llama.cpp` directly integrated in C++ for maximum efficiency and minimal latency.

## ✨ Key Features

- 🔐 **API Token Authentication** - Secure client identification with constant-time comparison
- 🚀 **Parallel Sessions** - Multiple concurrent inference sessions with independent contexts
- 💾 **Memory Efficient** - Single shared model + multiple lightweight contexts
- 📊 **Real-time Metrics** - GPU stats, per-session metrics, and system monitoring
- ⚡ **Smart GPU Split** - Auto-detection of optimal GPU layer distribution
- 🔌 **WebSocket API** - Real-time streaming with JSON protocol

---

## 🚀 Quick Start

### 1. Build

```bash
# Install dependencies
sudo apt-get install -y build-essential cmake git zlib1g-dev

# Clone and build
git clone <repository-url>
cd InferenceCenter
mkdir build && cd build

# CPU-only build
cmake -DUSE_CUDA=OFF ..
make -j$(nproc)

# OR GPU-enabled build (recommended)
cmake -DUSE_CUDA=ON ..
make -j$(nproc)
```

### 2. Configure Clients

Edit `clients.json` to add your API clients:

```json
{
  "clients": [
    {
      "client_id": "my_app",
      "api_key": "sk_your_secure_api_key_here",
      "max_sessions": 2,
      "priority": "normal",
      "description": "My Application"
    }
  ]
}
```

### 3. Run Server

```bash
./InferenceCore --model /path/to/model.gguf --port 3000
```

### 4. Connect Client

```python
import asyncio
import websockets
import json

async def example():
    uri = "ws://localhost:3000"
    async with websockets.connect(uri) as ws:
        # 1. Authenticate
        await ws.send(json.dumps({
            "op": "auth",
            "client_id": "my_app",
            "api_key": "sk_your_secure_api_key_here"
        }))
        auth_response = await ws.recv()
        print(auth_response)
        
        # 2. Create Session
        await ws.send(json.dumps({"op": "create_session"}))
        session_response = await ws.recv()
        session_id = json.loads(session_response)["session_id"]
        
        # 3. Run Inference
        await ws.send(json.dumps({
            "op": "infer",
            "session_id": session_id,
            "prompt": "Hello, world!",
            "params": {"temp": 0.7}
        }))
        
        # 4. Receive tokens
        while True:
            msg = await ws.recv()
            data = json.loads(msg)
            if data["op"] == "token":
                print(data["content"], end="", flush=True)
            elif data["op"] == "end":
                print(f"\n\nStats: {data['stats']}")
                break

asyncio.run(example())
```

---

## 🏗️ Architecture

### Core Components

```
src/
├── core/
│   ├── Engine.cpp/.h           # Model loader (shared across sessions)
│   ├── Session.cpp/.h          # Individual inference session
│   └── SessionManager.cpp/.h   # Session lifecycle management
├── server/
│   ├── WsServer.cpp/.h         # WebSocket server with thread pool
│   ├── Protocol.h              # JSON protocol definitions
│   └── ClientAuth.cpp/.h       # API token authentication
├── hardware/
│   └── Monitor.cpp/.h          # NVML GPU monitoring
└── main.cpp                    # Entry point
```

### Key Design Decisions

**Memory Efficiency:**
- **Single `llama_model`** loaded once in VRAM (e.g., 2-4GB)
- **Multiple `llama_context`** instances (small, ~100MB each)
- **Total VRAM** = Model + (N × Context) instead of N × Model

**True Parallelism:**
- Thread pool (4 workers by default)
- Multiple sessions generate concurrently
- No blocking between sessions

**Security:**
- API token required for all operations
- Constant-time comparison prevents timing attacks
- Per-client session limits
- Session ownership verification

---

## 📡 WebSocket Protocol

### 1. Authentication (Required First)

**Client → Server:**
```json
{
  "op": "auth",
  "client_id": "my_app",
  "api_key": "sk_your_api_key"
}
```

**Server → Client:**
```json
{
  "op": "auth_success",
  "client_id": "my_app",
  "max_sessions": 2
}
```

Or on failure:
```json
{
  "op": "auth_failed",
  "reason": "Invalid credentials"
}
```

### 2. Session Management

**Create Session:**
```json
// Client → Server
{"op": "create_session"}

// Server → Client
{"op": "session_created", "session_id": "sess_abc123_def456"}
```

**Close Session:**
```json
// Client → Server
{"op": "close_session", "session_id": "sess_abc123_def456"}

// Server → Client
{"op": "session_closed", "session_id": "sess_abc123_def456"}
```

### 3. Inference

**Client → Server:**
```json
{
  "op": "infer",
  "session_id": "sess_abc123_def456",
  "prompt": "Explain quantum physics",
  "params": {
    "temp": 0.7,
    "max_tokens": 500
  }
}
```

**Server → Client (Streaming):**
```json
{"op": "token", "session_id": "sess_...", "content": " Quantum"}
{"op": "token", "session_id": "sess_...", "content": " physics"}
...
{
  "op": "end",
  "session_id": "sess_...",
  "stats": {
    "ttft_ms": 104,
    "total_ms": 327,
    "tokens": 35,
    "tps": 107.03
  }
}
```

**Abort Generation:**
```json
{"op": "abort", "session_id": "sess_abc123_def456"}
```

### 4. Real-time Metrics (Opt-in)

**Subscribe to Metrics:**
```json
// Client → Server
{"op": "subscribe_metrics"}

// Server → Client
{"op": "metrics_subscribed", "message": "Subscribed to metrics updates"}
```

**Unsubscribe from Metrics:**
```json
// Client → Server
{"op": "unsubscribe_metrics"}

// Server → Client
{"op": "metrics_unsubscribed", "message": "Unsubscribed from metrics updates"}
```

**Metrics Broadcast** (sent every 1 second to subscribed clients):

```json
{
  "op": "metrics",
  "timestamp": 1706735234,
  "gpu": {
    "temp": 65,
    "vram_total_mb": 3072,
    "vram_used_mb": 1850,
    "vram_free_mb": 1222,
    "power_watts": 85,
    "fan_percent": 60,
    "throttling": false
  },
  "inference": {
    "active_generations": 2,
    "total_sessions": 5,
    "last_tps": 107.03,
    "last_ttft_ms": 104,
    "total_tokens_generated": 1523
  }
}
```

---

## 🔐 Client Configuration

Edit `clients.json` to manage API clients:

```json
{
  "clients": [
    {
      "client_id": "laptop_principal",
      "api_key": "sk_laptop_abc123...",
      "max_sessions": 2,
      "priority": "high",
      "description": "Main development laptop"
    },
    {
      "client_id": "desktop_oficina",
      "api_key": "sk_desktop_xyz789...",
      "max_sessions": 4,
      "priority": "normal",
      "description": "Office desktop"
    },
    {
      "client_id": "test_client",
      "api_key": "sk_test_qwe456...",
      "max_sessions": 1,
      "priority": "low",
      "description": "Testing and development"
    }
  ]
}
```

**Fields:**
- `client_id`: Unique identifier for the client
- `api_key`: Secure API token (use long random strings)
- `max_sessions`: Maximum concurrent sessions allowed
- `priority`: Client priority (currently informational)
- `description`: Human-readable description

---

## 🧪 Testing

The project includes a comprehensive test suite (Unit + Integration).

For detailed documentation, see **[TESTING.md](TESTING.md)**.

### Quick Verification
-   **Unit Tests**: Validate core C++ logic (Protocol, Auth).
-   **Integration Tests**: Validate server cycles (Auth -> Session -> Inference).

---

## 🎯 Recommended Models (GTX 1060 3GB)

| Model | Quantization | Size | Performance |
|-------|--------------|------|-------------|
| Llama-3.2-1B | q4_k_m | ~800MB | ✅ Excellent |
| Llama-3.2-3B | q4_k_m | ~2.1GB | ✅ Very good |
| Mistral-7B | q4_k_m | ~4.8GB | ⚠️ Requires CPU/GPU split |
| Llama-3-8B | q4_k_m | ~5.2GB | ⚠️ Requires CPU/GPU split |

Download models from: https://huggingface.co/models?library=gguf

---

## 🔧 Command-line Options

```bash
./InferenceCore [OPTIONS]

Options:
  --model <path>        Path to .gguf model file (required)
  --port <port>         WebSocket server port (default: 3000)
  --gpu-layers <N>      Number of layers to offload to GPU
                        -1 = auto-detect (default)
                        0 = CPU only
                        >0 = specific layer count
  --ctx-size <N>        Context size in tokens (default: 512, configurable)
```

**Examples:**

```bash
# Auto-detect GPU layers
./InferenceCore --model model.gguf --port 3000

# Force CPU-only
./InferenceCore --model model.gguf --gpu-layers 0

# Custom context size
./InferenceCore --model model.gguf --ctx-size 2048

# All options
./InferenceCore --model model.gguf --port 8080 --gpu-layers 20 --ctx-size 1024
```

---

## 📊 Performance Metrics

### Example: 3 Concurrent Sessions

**Test Setup:**
- Model: Llama-3.2-3B q4_k_m
- Hardware: GTX 1060 3GB
- Sessions: 3 concurrent

**Results:**
- **Total time**: 9.55s for 3 concurrent inferences
- **Tokens generated**: 1,112 total (505 + 99 + 508)
- **TPS per session**: ~53 tokens/second
- **TTFT**: 475ms (time to first token)
- **Memory**: Model (2.1GB) + 3 contexts (~300MB) = ~2.4GB VRAM

---

## 🐛 Troubleshooting

### "Failed to load client configuration"

Ensure `clients.json` exists in the same directory as the executable:

```bash
cp clients.json build/
# OR
./InferenceCore --model model.gguf  # Run from project root
```

### "Authentication failed"

Check that `client_id` and `api_key` match exactly in `clients.json`.

### "Session not found or access denied"

- Ensure you created a session first with `create_session`
- Verify the `session_id` is correct
- Check that the session belongs to your authenticated client

### "Failed to create session (limit reached)"

Your client has reached `max_sessions` limit. Close existing sessions first:

```json
{"op": "close_session", "session_id": "sess_..."}
```

### "CUDA OOM"

- Use smaller models or more aggressive quantization (q4_k_m)
- Reduce `--ctx-size`
- Limit concurrent sessions per client in `clients.json`

### "NVML initialization failed"

```bash
# Verify drivers
nvidia-smi

# Reinstall if needed
sudo apt-get install --reinstall nvidia-driver-535
```

---

## 📚 Documentation

- **[walkthrough.md](brain/.../walkthrough.md)**: Implementation walkthrough with architecture details
- **[task.md](brain/.../task.md)**: Development task breakdown
- **[implementation_plan.md](brain/.../implementation_plan.md)**: Original implementation plan

---

## 🔒 Security Best Practices

1. **API Keys**: Use long, random strings (min 64 characters)
2. **HTTPS**: Use a reverse proxy (nginx, caddy) with TLS in production
3. **Firewall**: Restrict access to trusted IPs
4. **Rotation**: Rotate API keys periodically
5. **Monitoring**: Monitor for unusual session creation patterns

---

## 🚀 Production Deployment

### Using systemd

Create `/etc/systemd/system/inference-center.service`:

```ini
[Unit]
Description=InferenceCenter Server
After=network.target

[Service]
Type=simple
User=inference
WorkingDirectory=/opt/inference-center
ExecStart=/opt/inference-center/InferenceCore --model /models/llama-3.2-3b.gguf --port 3000
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
sudo systemctl enable inference-center
sudo systemctl start inference-center
sudo systemctl status inference-center
```

### Using Docker

```bash
# 1. Build and Run with Docker Compose
docker-compose up -d --build

# 2. View Logs
docker-compose logs -f

# 3. Stop
docker-compose down
```

**Note:** Requires NVIDIA Container Toolkit for GPU pass-through.


---

## 📄 License

This project uses `llama.cpp` (MIT License). See the original repository for details.

---

## 🤝 Contributing

This is a personal project optimized for specific hardware (GTX 1060 3GB). Suggestions and improvements are welcome via issues or pull requests.

---

## 🙏 Acknowledgments

- [llama.cpp](https://github.com/ggerganov/llama.cpp) - Core inference engine
- [uWebSockets](https://github.com/uNetworking/uWebSockets) - WebSocket server
- [nlohmann/json](https://github.com/nlohmann/json) - JSON library
