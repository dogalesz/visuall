# ── visuall-lsp: Visuall Language Server Protocol implementation ─────────────

set(LSP_SOURCES
    src/main.cpp
    src/json_rpc.cpp
    src/document_store.cpp
    src/lsp_server.cpp
    src/formatter.cpp
    src/handlers/initialize.cpp
    src/handlers/text_document/did_open.cpp
    src/handlers/text_document/did_change.cpp
    src/handlers/text_document/did_close.cpp
    src/handlers/text_document/publish_diagnostics.cpp
    src/handlers/text_document/completion.cpp
    src/handlers/text_document/hover.cpp
    src/handlers/text_document/definition.cpp
    src/handlers/text_document/references.cpp
    src/handlers/text_document/document_symbols.cpp
    src/handlers/text_document/formatting.cpp
    src/handlers/text_document/inlay_hints.cpp
    src/handlers/workspace/symbol.cpp
)

add_executable(visuall-lsp ${LSP_SOURCES})
target_include_directories(visuall-lsp PRIVATE include)
target_link_libraries(visuall-lsp PRIVATE visuall_frontend nlohmann_json::nlohmann_json)
target_compile_features(visuall-lsp PRIVATE cxx_std_17)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()

add_executable(lsp_test tests/lsp_test.cpp)
target_include_directories(lsp_test PRIVATE include)
target_link_libraries(lsp_test PRIVATE visuall_frontend nlohmann_json::nlohmann_json)
target_compile_features(lsp_test PRIVATE cxx_std_17)

add_test(NAME lsp_test COMMAND lsp_test)
