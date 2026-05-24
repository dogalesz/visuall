#pragma once

#include <nlohmann/json.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include <iostream>
#include <mutex>

namespace lsp {

using json = nlohmann::json;

// Handler signature: receives params, returns result (or null for notifications).
using RequestHandler      = std::function<json(const json& params)>;
using NotificationHandler = std::function<void(const json& params)>;

// ════════════════════════════════════════════════════════════════════════════
// JsonRpc — minimal JSON-RPC 2.0 transport over stdin/stdout.
//
// Single-threaded, synchronous. Reads one message at a time from stdin,
// dispatches to the registered handler, and writes the response to stdout.
// ════════════════════════════════════════════════════════════════════════════
class JsonRpc {
public:
    JsonRpc();

    // Main loop — reads stdin forever until EOF or "exit" notification.
    void run();

    // Send a JSON-RPC message (response or notification) to stdout.
    void send(const json& message);

    // Send a notification to the client (no id).
    void sendNotification(const std::string& method, const json& params);

    // Send an error response.
    void sendError(const json& id, int code, const std::string& message);

    // Register a handler for a request (expects a response).
    void onRequest(const std::string& method, RequestHandler handler);

    // Register a handler for a notification (no response).
    void onNotification(const std::string& method, NotificationHandler handler);

    // Signal the server to stop.
    void stop();

private:
    std::unordered_map<std::string, RequestHandler>      requestHandlers_;
    std::unordered_map<std::string, NotificationHandler>  notificationHandlers_;
    bool running_ = false;
    std::mutex writeMutex_;

    // Read a complete LSP message from stdin.
    // Returns empty string on EOF.
    std::string readMessage();

    // Write a complete LSP message to stdout.
    void writeMessage(const std::string& content);

    // Dispatch a parsed JSON-RPC message to the appropriate handler.
    void dispatch(const json& msg);
};

// LSP error codes
namespace ErrorCodes {
    constexpr int ParseError           = -32700;
    constexpr int InvalidRequest       = -32600;
    constexpr int MethodNotFound       = -32601;
    constexpr int InvalidParams        = -32602;
    constexpr int InternalError        = -32603;
    constexpr int ServerNotInitialized = -32002;
    constexpr int RequestCancelled     = -32800;
}

} // namespace lsp
