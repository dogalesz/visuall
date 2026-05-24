#include "json_rpc.h"
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace lsp {

JsonRpc::JsonRpc() {
#ifdef _WIN32
    // Set stdin/stdout to binary mode on Windows to prevent \r\n translation.
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

void JsonRpc::run() {
    running_ = true;
    while (running_) {
        std::string content = readMessage();
        if (content.empty()) {
            break; // EOF
        }

        try {
            json msg = json::parse(content);
            dispatch(msg);
        } catch (const json::parse_error& e) {
            // Malformed JSON — send parse error if we can extract an id.
            sendError(nullptr, ErrorCodes::ParseError,
                      std::string("Parse error: ") + e.what());
        } catch (const std::exception& e) {
            // Internal error — log but don't crash.
            std::cerr << "[visuall-lsp] Internal error: " << e.what() << std::endl;
        }
    }
}

void JsonRpc::send(const json& message) {
    std::string content = message.dump();
    writeMessage(content);
}

void JsonRpc::sendNotification(const std::string& method, const json& params) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"method",  method},
        {"params",  params}
    };
    send(msg);
}

void JsonRpc::sendError(const json& id, int code, const std::string& message) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id",      id},
        {"error", {
            {"code",    code},
            {"message", message}
        }}
    };
    send(msg);
}

void JsonRpc::onRequest(const std::string& method, RequestHandler handler) {
    requestHandlers_[method] = std::move(handler);
}

void JsonRpc::onNotification(const std::string& method, NotificationHandler handler) {
    notificationHandlers_[method] = std::move(handler);
}

void JsonRpc::stop() {
    running_ = false;
}

// ── Private implementation ─────────────────────────────────────────────────

std::string JsonRpc::readMessage() {
    // Read headers until empty line (\r\n\r\n).
    // The only required header is Content-Length.
    int contentLength = -1;

    while (true) {
        std::string line;
        int ch;
        while ((ch = std::fgetc(stdin)) != EOF) {
            if (ch == '\n') break;
            line += static_cast<char>(ch);
        }

        if (ch == EOF) {
            return ""; // Connection closed.
        }

        // Strip trailing \r if present.
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Empty line signals end of headers.
        if (line.empty()) {
            break;
        }

        // Parse Content-Length header.
        const std::string prefix = "Content-Length: ";
        if (line.compare(0, prefix.size(), prefix) == 0) {
            try {
                contentLength = std::stoi(line.substr(prefix.size()));
            } catch (...) {
                contentLength = -1;
            }
        }
        // Other headers (Content-Type, etc.) are ignored per LSP spec.
    }

    if (contentLength <= 0) {
        return "";
    }

    // Read exactly contentLength bytes of the body.
    std::string body(contentLength, '\0');
    size_t bytesRead = std::fread(&body[0], 1, contentLength, stdin);
    if (bytesRead < static_cast<size_t>(contentLength)) {
        return ""; // Truncated message — treat as EOF.
    }

    return body;
}

void JsonRpc::writeMessage(const std::string& content) {
    std::lock_guard<std::mutex> lock(writeMutex_);

    std::string header = "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n";

    std::fwrite(header.data(), 1, header.size(), stdout);
    std::fwrite(content.data(), 1, content.size(), stdout);
    std::fflush(stdout);
}

void JsonRpc::dispatch(const json& msg) {
    // Determine if this is a request (has "id") or notification (no "id").
    bool hasId = msg.contains("id");
    std::string method = msg.value("method", "");
    json params = msg.value("params", json::object());
    json id = hasId ? msg["id"] : json(nullptr);

    if (method.empty()) {
        if (hasId) {
            sendError(id, ErrorCodes::InvalidRequest, "Missing method");
        }
        return;
    }

    if (hasId) {
        // Request — find handler and send response.
        auto it = requestHandlers_.find(method);
        if (it != requestHandlers_.end()) {
            try {
                json result = it->second(params);
                json response = {
                    {"jsonrpc", "2.0"},
                    {"id",      id},
                    {"result",  result}
                };
                send(response);
            } catch (const std::exception& e) {
                sendError(id, ErrorCodes::InternalError, e.what());
            }
        } else {
            sendError(id, ErrorCodes::MethodNotFound,
                      "Method not found: " + method);
        }
    } else {
        // Notification — fire and forget.
        auto it = notificationHandlers_.find(method);
        if (it != notificationHandlers_.end()) {
            try {
                it->second(params);
            } catch (const std::exception& e) {
                std::cerr << "[visuall-lsp] Error handling notification '"
                          << method << "': " << e.what() << std::endl;
            }
        }
        // Unknown notifications are silently ignored per LSP spec.
    }
}

} // namespace lsp
