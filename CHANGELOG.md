## [1.0.1](https://github.com/Jota-project/jota-inference/compare/v1.0.0...v1.0.1) (2026-03-16)

# 1.0.0 (2026-03-16)


### Bug Fixes

* Complete client/server fixes for production readiness ([e04707d](https://github.com/Jota-project/jota-inference/commit/e04707d598a8f2b2ce5cdc225c511d31778d2917))
* correct branch name from main to master in release workflow ([af42c45](https://github.com/Jota-project/jota-inference/commit/af42c459972fe0e820aa5c457acc22194723124d))
* **core): EnvLoader fallback & refactor(auth:** Headers for JotaDB ([ba64d59](https://github.com/Jota-project/jota-inference/commit/ba64d599dabf36d154b4ab378623278b6a5ee6f7))
* remove npm cache from setup-node (no package-lock.json in repo) ([64b19e2](https://github.com/Jota-project/jota-inference/commit/64b19e27ca7a29027b9d02f55edb3dfa543e29f0))
* remove npm cache from setup-node (no package-lock.json in repo) ([#15](https://github.com/Jota-project/jota-inference/issues/15)) ([3ce1e71](https://github.com/Jota-project/jota-inference/commit/3ce1e7129020052f42126226eceef58affe9e4c4))
* update docker-compose models path to jota-db ([9149b35](https://github.com/Jota-project/jota-inference/commit/9149b35867d4ae509cae9b3b911522245f3d1964))
* update docker-compose models path to jota-db ([#13](https://github.com/Jota-project/jota-inference/issues/13)) ([7de0ffb](https://github.com/Jota-project/jota-inference/commit/7de0ffb4bac55cf5f79b4f1277fc0c9240b03530))


### Code Refactoring

* WsServer to Services/Handlers architecture (v0.4.0) ([c2d2312](https://github.com/Jota-project/jota-inference/commit/c2d23127f4435b70b482648e8603e86f1f9e467d))


### Features

* Add `build_fast.sh` script for rapid, CPU-only local development builds utilizing ccache and Ninja. ([c295149](https://github.com/Jota-project/jota-inference/commit/c295149fd24d31001077450d0c750b4954b10ee4))
* Add colored console and json file logs, update auth and docker mount ([84b9cb3](https://github.com/Jota-project/jota-inference/commit/84b9cb3d9cbdc5fc52e30ec6897e3de678e04025))
* Add GitHub Actions CI workflow for C++ and Python tests, including new C++ unit tests for authentication and protocol. ([66f3961](https://github.com/Jota-project/jota-inference/commit/66f3961e2c19e3ebcfc14fbf4fada04526249446))
* **logger:** Add IC_LOG macros and environment-based level filtering ([d6fa56f](https://github.com/Jota-project/jota-inference/commit/d6fa56fa589260656e5a90bc981d322363d4f6e6))
* Add named argument parser and hardware monitoring ([24cfb8c](https://github.com/Jota-project/jota-inference/commit/24cfb8c3c0e9d8090c201dde8bacaa99aacbae20))
* add set_context operation for session context (chat history) ([f809bfc](https://github.com/Jota-project/jota-inference/commit/f809bfc44212295538344bc198383b8680af0d61))
* Centralized AppConfig, fixed context crash & added JSON grammar and tool call support ([b255940](https://github.com/Jota-project/jota-inference/commit/b255940444d1e46d60dac6ac94d604774578e2fc))
* **resilience:** Exception hierarchy and RAII resource guards ([d521c11](https://github.com/Jota-project/jota-inference/commit/d521c113a243fe67505b332fe4b45e47cd4407ab))
* **auth:** Heartbeat verification & Hybrid Headers (X-Auth) ([bec0081](https://github.com/Jota-project/jota-inference/commit/bec00812fbe2bcf96c776b56e80696de041d2467))
* **logger:** Implement centralized JSON logging to stdout for Docker observabilities ([d5338ec](https://github.com/Jota-project/jota-inference/commit/d5338ec6591e107ffb6f4167352a8945c077beb2))
* Implement client authentication and multi-session management for the WebSocket server. ([f5a6355](https://github.com/Jota-project/jota-inference/commit/f5a63556edc9eb258d80178dfdb2a0e34c025c24))
* **Core:** Implement Dynamic Model Loading and Auto-Fallback ([791302d](https://github.com/Jota-project/jota-inference/commit/791302da9bb8a47f2adecd6a668d85a6ea46ee87))
* Implement header-based client authentication ([9663090](https://github.com/Jota-project/jota-inference/commit/96630902ec1aa13d53b13b7a6ff128d13551a420))
* implement inference safety and secure test credentials ([d78d648](https://github.com/Jota-project/jota-inference/commit/d78d648f890b1f1539c30cf6de617dfdc520d91b))
* implement watchdog to abort blocked inference sessions ([f27db52](https://github.com/Jota-project/jota-inference/commit/f27db526740c8236b2a9efac17417d000d0a00be))
* improve authentication and startup experience ([556b221](https://github.com/Jota-project/jota-inference/commit/556b22127f363a73f2ae2e8a5f166196a88db24f))
* **profiles:** Inference profiles with dynamic sampler and system prompts ([b9db8dc](https://github.com/Jota-project/jota-inference/commit/b9db8dccf74ed37eb5403aee13adb341690aefbc))
* integrate JotaDB via HTTPS for client authentication and configuration ([ad39510](https://github.com/Jota-project/jota-inference/commit/ad39510704fc33ecfb13006b20b85ec62b76012e))
* **logging:** Intercept llama.cpp logs via LlamaLogger callback ([46e3486](https://github.com/Jota-project/jota-inference/commit/46e3486e60ce05a6f12eb9fdb0ddb0465ff7c8fd))
* Migrate configuration from `clients.json` to `.env` files, update the default test URI, and adjust Docker build and compose settings accordingly. ([89f5f44](https://github.com/Jota-project/jota-inference/commit/89f5f4433c604002005497090ee9793edb6335b3))
* Release v0.4.0 - Refactor WsServer, Add Docker Support, CI/CD & Health Checks ([e0d5b6e](https://github.com/Jota-project/jota-inference/commit/e0d5b6eda366daa5b8ed708c00061ffa55b54597))
* Release v0.5.0 - JotaDB Integration ([df8d655](https://github.com/Jota-project/jota-inference/commit/df8d655a2d8c07e51667357093f0517ee02d9158))
* Release v0.5.1 - JotaDB Polish (HTTPS, TTL, Logging) ([ab40de1](https://github.com/Jota-project/jota-inference/commit/ab40de1a908398da3150ef0d027eed2a682a800a))
* **thought:** Streaming thought detection with ThoughtFilter ([79f1b35](https://github.com/Jota-project/jota-inference/commit/79f1b3510f66cb38d7c4b2968d872183273f4357))
* v0.3.0 Release - Opt-in metrics and llama_decode fix ([8637de3](https://github.com/Jota-project/jota-inference/commit/8637de349883b35a596f80f6f4ab32588e34260b))


### BREAKING CHANGES

* Internal architecture completely refactored

Core Changes:
- Extract InferenceService (thread pool + task queue)
- Extract MetricsService (periodic broadcasting)
- Create RequestContext (WebSocket abstraction)
- Create MessageDispatcher (operation router)
- Create 4 handlers: Auth, Session, Inference, Metrics (header-only)
- Reduce WsServer from 450 to 135 lines (70% reduction)
- Maintain backward compatibility (protocol unchanged)
- Zero performance impact (same threading model)

Benefits:
- Single Responsibility Principle
- Highly testable components
- Easy to extend/maintain
- Framework-agnostic services

New Files:
+ services/InferenceService.{h,cpp}
+ services/MetricsService.{h,cpp}
+ handlers/{Auth,Session,Inference,Metrics}Handler.h
+ RequestContext.h
+ MessageDispatcher.h

Modified:
~ WsServer.{h,cpp} (refactored)
~ CMakeLists.txt (updated build)

Architecture now production-ready with clean separation of concerns.
