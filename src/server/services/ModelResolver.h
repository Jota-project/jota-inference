#pragma once

#include "../../core/Engine.h"
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Server {

/**
 * @brief Service to resolve model configurations from JotaDB.
 * 
 * JotaDB returns the absolute file_path, context size, and gpu layers config.
 * It also provides the full list of available models.
 */
class ModelResolver {
public:
    ModelResolver();
    ~ModelResolver() = default;

    /**
     * @brief Fetches the technical configuration for a specific model.
     * 
     * @param model_id The string ID of the model requested by the client.
     * @param out_config The output EngineConfig populated from JotaDB.
     * @return true if successfully fetched and parsed, false on error/not found.
     */
    bool fetchModelConfig(const std::string& model_id, Core::EngineConfig& out_config);

    /**
     * @brief Fetches the list of all available models from JotaDB.
     * 
     * @param out_models The output JSON array containing model details (sanitized).
     * @return true if successfully fetched and sanitized, false on error.
     */
    bool fetchAvailableModels(json& out_models);

private:
    std::string jota_db_url_;
    std::string inference_center_id_;
    std::string inference_center_sk_;

    // Helper to parse URL into scheme, domain, and path_prefix
    void parseUrl(const std::string& url, std::string& scheme, std::string& domain, std::string& path_prefix) const;
};

} // namespace Server
