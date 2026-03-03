#include "ModelResolver.h"
#include "EnvLoader.h"
#include "Logger.h"
#include "AppConfig.h"
#include <httplib.h>

namespace Server {

ModelResolver::ModelResolver() {
    jota_db_url_ = Core::EnvLoader::get("JOTA_DB_URL", "https://green-house.local/api/db");
    jota_db_sk_ = Core::EnvLoader::get("JOTA_DB_SK", "");
    inference_center_id_ = Core::EnvLoader::get("INFERENCE_CENTER_ID", "");
    inference_center_sk_ = Core::EnvLoader::get("INFERENCE_CENTER_SK", "");

    if (inference_center_id_.empty() || inference_center_sk_.empty()) {
        IC_LOG_WARN("INFERENCE_CENTER_ID or INFERENCE_CENTER_SK is empty. ModelResolver requests to JotaDB may fail.");
    }
    if (jota_db_sk_.empty()) {
        IC_LOG_WARN("JOTA_DB_SK is empty. ModelResolver requests to JotaDB may fail to authenticate.");
    }
}

void ModelResolver::parseUrl(const std::string& url, std::string& scheme, std::string& domain, std::string& path_prefix) const {
    std::string host_port;
    if (url.find("http://") == 0) {
        scheme = "http";
        host_port = url.substr(7);
    } else if (url.find("https://") == 0) {
        scheme = "https";
        host_port = url.substr(8);
    } else {
        scheme = "http"; 
        host_port = url;
    }

    size_t path_pos = host_port.find('/');
    if (path_pos != std::string::npos) {
        domain = host_port.substr(0, path_pos);
        path_prefix = host_port.substr(path_pos);
    } else {
        domain = host_port;
        path_prefix = "";
    }
    
    if (!path_prefix.empty() && path_prefix.back() == '/') {
        path_prefix.pop_back();
    }
}

bool ModelResolver::fetchModelConfig(const std::string& model_id, Core::EngineConfig& out_config) {
    std::string scheme, domain, path_prefix;
    parseUrl(jota_db_url_, scheme, domain, path_prefix);
    
    std::string base_url = scheme + "://" + domain;
    httplib::Client cli(base_url.c_str());
    cli.set_connection_timeout(2);
    cli.set_read_timeout(3);
    
    #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    if (scheme == "https") {
        cli.enable_server_certificate_verification(false); 
    }
    #endif

    std::string request_path = path_prefix + "/chat/models";
    
    httplib::Headers headers;
    headers.emplace("X-Service-ID", inference_center_id_);
    headers.emplace("X-API-Key", inference_center_sk_);
    if (!jota_db_sk_.empty()) {
        headers.emplace("Authorization", "Bearer " + jota_db_sk_);
    }

    auto res = cli.Get(request_path.c_str(), headers);

    if (res && res->status == 200) {
        try {
            auto json_res = json::parse(res->body);

            if (json_res.contains("error")) {
                 IC_LOG_WARN("Failed to fetch models from DB", {
                     {"error", json_res["error"].dump()}
                 });
                 return false;
            }

            std::string db_prefix = "";

            if (!json_res.is_array()) {
                if (json_res.is_object()) {
                    // Try to guess the base path field or use what the DB specifically provides
                    if (json_res.contains("path") && json_res["path"].is_string()) db_prefix = json_res["path"].get<std::string>();
                    else if (json_res.contains("base_path") && json_res["base_path"].is_string()) db_prefix = json_res["base_path"].get<std::string>();
                    else if (json_res.contains("models_dir") && json_res["models_dir"].is_string()) db_prefix = json_res["models_dir"].get<std::string>();
                    else if (json_res.contains("route") && json_res["route"].is_string()) db_prefix = json_res["route"].get<std::string>();
                }

                if (json_res.contains("models") && json_res["models"].is_array()) {
                    json_res = json_res["models"];
                } else {
                    IC_LOG_ERROR("JotaDB models response is not an array");
                    return false;
                }
            }

            for (const auto& item : json_res) {
                if (item.value("id", "") == model_id) {
                    if (!item.contains("file_path")) {
                        IC_LOG_ERROR("JotaDB response structure invalid: missing file_path", {{"model_id", model_id}});
                        return false;
                    }

                    // Store IDs and basic config
                    out_config.modelId = model_id;
                    out_config.n_gpu_layers = item.value("gpu_layers", -1);
                    out_config.ctx_size = item.value("context_size", Core::AppConfig::get().ctx_size);

                    // Path Translation
                    std::string raw_path = item["file_path"].get<std::string>();
                    
                    if (!db_prefix.empty() && raw_path.find(db_prefix) == 0) {
                        std::string base_path = Core::EnvLoader::get("MODELS_BASE_PATH", "/models");
                        
                        // Ensure base_path doesn't end with a slash if we're appending
                        if (!base_path.empty() && base_path.back() == '/') {
                            base_path.pop_back();
                        }
                        
                        std::string relative_path = raw_path.substr(db_prefix.length());
                        if (!relative_path.empty() && relative_path.front() != '/') {
                            relative_path = "/" + relative_path;
                        }
                        
                        out_config.modelPath = base_path + relative_path;
                        
                        IC_LOG_INFO("Translated JotaDB model path", {
                            {"original", raw_path},
                            {"translated", out_config.modelPath}
                        });
                    } else {
                        // Keep original if it doesn't match the DB prefix
                        out_config.modelPath = raw_path;
                    }

                    return true;
                }
            }

            IC_LOG_WARN("Model not found in DB list", {{"model_id", model_id}});
            return false;

        } catch (const std::exception& e) {
            IC_LOG_ERROR("Error parsing JotaDB models response", {{"error", std::string(e.what())}});
            return false;
        }
    } else {
        auto err = res.error();
        int status = res ? res->status : 0;
        IC_LOG_ERROR("JotaDB models request failed", {
            {"status", status},
            {"error", std::to_string(static_cast<int>(err))}
        });
        return false;
    }
}

bool ModelResolver::fetchAvailableModels(json& out_models) {
    std::string scheme, domain, path_prefix;
    parseUrl(jota_db_url_, scheme, domain, path_prefix);
    
    std::string base_url = scheme + "://" + domain;
    httplib::Client cli(base_url.c_str());
    cli.set_connection_timeout(2);
    cli.set_read_timeout(3);
    
    #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    if (scheme == "https") {
        cli.enable_server_certificate_verification(false); 
    }
    #endif

    std::string request_path = path_prefix + "/chat/models";
    
    httplib::Headers headers;
    headers.emplace("X-Service-ID", inference_center_id_);
    headers.emplace("X-API-Key", inference_center_sk_);
    if (!jota_db_sk_.empty()) {
        headers.emplace("Authorization", "Bearer " + jota_db_sk_);
    }

    auto res = cli.Get(request_path.c_str(), headers);

    if (res && res->status == 200) {
        try {
            auto json_res = json::parse(res->body);

            if (json_res.contains("error")) {
                 IC_LOG_WARN("Failed to fetch models list from DB", {
                     {"error", json_res["error"].dump()}
                 });
                 return false;
            }

            if (!json_res.is_array()) {
                if (json_res.contains("models") && json_res["models"].is_array()) {
                    json_res = json_res["models"];
                } else {
                    IC_LOG_ERROR("JotaDB models response is not an array");
                    return false;
                }
            }

            // Sanitize: strip file_path from all models
            out_models = json::array();
            for (auto& item : json_res) {
                if (item.contains("file_path")) {
                    item.erase("file_path");
                }
                out_models.push_back(item);
            }

            return true;

        } catch (const std::exception& e) {
            IC_LOG_ERROR("Error parsing JotaDB models response", {{"error", std::string(e.what())}});
            return false;
        }
    } else {
        auto err = res.error();
        int status = res ? res->status : 0;
        IC_LOG_ERROR("JotaDB models request failed", {
            {"status", status},
            {"error", std::to_string(static_cast<int>(err))}
        });
        return false;
    }
}

} // namespace Server
