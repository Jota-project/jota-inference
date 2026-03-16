#pragma once

#include "../RequestContext.h"
#include "../services/ModelResolver.h"
#include "../services/InferenceService.h"
#include "../../core/Engine.h"
#include "Logger.h"
#include <nlohmann/json.hpp>
#include <memory>

using json = nlohmann::json;

namespace Server {

/**
 * ModelHandler - Handles loading and listing models
 * 
 * Processes Op::LOAD_MODEL and Op::LIST_MODELS requests.
 * Uses ModelResolver to call JotaDB and Engine to apply changes.
 */
class ModelHandler {
public:
    explicit ModelHandler(std::shared_ptr<ModelResolver> modelResolver, Core::Engine& engine, InferenceService* inferenceService)
        : modelResolver_(modelResolver)
        , engine_(engine)
        , inferenceService_(inferenceService)
    {
        if (!modelResolver_) {
            throw std::invalid_argument("ModelResolver cannot be null");
        }
    }
    
    /**
     * Handle LOAD_MODEL request
     * @param ctx Request context
     * @param payload JSON payload with model_id
     */
    void handleLoadModel(RequestContext& ctx, const json& payload) {
        auto* data = ctx.getData();
        
        if (!data->authenticated) {
            sendError(ctx, Err::NOT_AUTHENTICATED);
            return;
        }

        if (!payload.contains("model_id")) {
            sendError(ctx, Err::MISSING_FIELDS);
            return;
        }
        
        std::string model_id = payload["model_id"];
        IC_LOG_INFO("Client requested model load", {{"client_id", data->client_id}, {"model_id", model_id}});
        
        Core::EngineConfig config;
        bool success = modelResolver_->fetchModelConfig(model_id, config);
        
        if (!success) {
            sendError(ctx, Err::MODEL_NOT_FOUND);
            return;
        }

        // Safety check: do not unload or switch models while an inference is actively generating
        if (inferenceService_ && inferenceService_->getActiveGenerations() > 0) {
            sendError(ctx, Err::INFERENCE_IN_PROGRESS);
            IC_LOG_WARN("Model switch rejected: Inference in progress", {{"client_id", data->client_id}});
            return;
        }

        // Unload old, then try to load new
        engine_.unloadModel();
        
        bool loadSuccess = engine_.loadModel(config);
        
        if (loadSuccess) {
            json response = {
                {"op", Op::LOAD_MODEL_RESULT},
                {"status", "SUCCESS"}
            };
            ctx.send(response);
            IC_LOG_INFO("Model loaded successfully", {{"model_id", model_id}});
        } else {
            sendError(ctx, Err::MODEL_NOT_FOUND); // Config retrieved but engine failed to load file
            IC_LOG_ERROR("Engine failed to load model", {{"model_id", model_id}});
        }
    }

    /**
     * Handle LIST_MODELS request
     * @param ctx Request context
     * @param payload JSON payload
     */
    void handleListModels(RequestContext& ctx, const json& payload) {
        (void)payload;
        auto* data = ctx.getData();
        
        if (!data->authenticated) {
            sendError(ctx, Err::NOT_AUTHENTICATED);
            return;
        }
        
        IC_LOG_INFO("Client requested available models", {{"client_id", data->client_id}});
        
        json modelsList;
        bool success = modelResolver_->fetchAvailableModels(modelsList);
        
        if (success) {
            json response = {
                {"op", Op::LIST_MODELS_RESULT},
                {"status", "SUCCESS"},
                {"models", modelsList}
            };
            ctx.send(response);
        } else {
            sendError(ctx, Err::MODEL_LIST_FAILED);
        }
    }

private:
    std::shared_ptr<ModelResolver> modelResolver_;
    Core::Engine& engine_;
    InferenceService* inferenceService_;

    void sendError(RequestContext& ctx, const std::string& errorMsg) {
        json response = {
            {"op", Op::ERROR},
            {"error", errorMsg}
        };
        ctx.send(response);
    }
};

} // namespace Server
