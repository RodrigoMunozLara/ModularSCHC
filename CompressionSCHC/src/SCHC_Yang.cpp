#include <libyang/libyang.h>
#include <stdexcept>
#include <string>
#include "spdlog/spdlog.h"

void validate_json_schc(const std::string& yang_dir,
                               const std::string& json_path) {
    ly_ctx* ctx = nullptr;
    const char *features[] ={"*",NULL};
    if (ly_ctx_new(yang_dir.c_str(), 0, &ctx) != LY_SUCCESS) {
        spdlog::error("No se pudo crear el contexto libyang");
        throw std::runtime_error("No se pudo crear el contexto libyang");
    }
    spdlog::info("Se pudo crear el contexto libyang");
    // Carga el módulo principal (ietf-schc) y sus imports
    

    const struct lys_module *mod = ly_ctx_load_module(ctx, "ietf-schc", nullptr, features);
    if (!mod) {
        ly_ctx_destroy(ctx);
        throw std::runtime_error("No se pudo cargar ietf-schc");
    }
    else{
        spdlog::info("Se pudo cargar el modulo YANG ietf-schc con features Comp-Frag-NoComp");
    }
    lyd_node* data = nullptr;

    // Parse + validate (STRICT) el JSON
    LY_ERR rc = lyd_parse_data_path(
        ctx,
        json_path.c_str(),
        LYD_JSON,
        LYD_PARSE_STRICT,        // exige que cumpla el modelo
        LYD_VALIDATE_PRESENT,    // valida must/when/mandatory/range/etc.
        &data
    );

    if (rc != LY_SUCCESS) {
        const ly_err_item* err = ly_err_first(ctx);
        std::string msg = err ? err->msg : "error desconocido";
        ly_ctx_destroy(ctx);
        spdlog::error("Validacion YANG fallida: {}", static_cast<uint8_t>(rc));
        spdlog::error("Validacion YANG fallida: {}", msg);
        throw std::runtime_error("Validacion YANG fallida: " + msg);
        
    }
    spdlog::info("Yang model validado");
    lyd_free_all(data);
    ly_ctx_destroy(ctx);
}
