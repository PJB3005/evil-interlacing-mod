#include "config.hpp"
#include "mods/svc/config.h"

#include "mod_helpers.hpp"
#include "mods/svc/ui.hpp"

namespace slugcat::interlace::config {

using namespace slugcat::mod_helpers;

namespace {

UiElementHandle statusText1 = 0;
UiElementHandle statusText2 = 0;

constexpr ConfigVarDesc cVarHalfsiesModeDesc{
    .struct_size = sizeof(cVarHalfsiesModeDesc),
    .name = "halfsies",
    .type = CONFIG_VAR_BOOL,
};

ConfigVarHandle cVarHalfsiesModeHandle;

ModResult build(ModContext *, UiElementHandle panel, void *, ModError *) {
    svc_ui->pane_add_section(mod_ctx, panel, "Settings");

    UiControlDesc control1 = UI_CONTROL_DESC_INIT;
    control1.kind = UI_CONTROL_TOGGLE;
    control1.label = "Half-FPS mode";
    control1.help_rml = "Real shit";
    control1.binding = UI_BINDING_CONFIG_VAR;
    control1.config_var = cVarHalfsiesModeHandle;
    svc_ui->pane_add_control(mod_ctx, panel, &control1, &statusText1);
    svc_ui->pane_add_rml(
        mod_ctx, panel,
        "Cap FPS to 60 and enable half-FPS mode for the <i>authentic</i> experience.",
        &statusText2);

    return MOD_OK;
}

} // namespace

bool GetHalfsiesMode() {
    bool result;
    CheckResult(svc_config->get_bool(mod_ctx, cVarHalfsiesModeHandle, &result), "get_bool");
    return result;
}

void Init() {
    mod_helpers::CheckResult(
        svc_config->register_var(mod_ctx, &cVarHalfsiesModeDesc, &cVarHalfsiesModeHandle),
        "register_var");

    UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
    panel.build = build;
    CheckResult(svc_ui->register_mods_panel(mod_ctx, &panel), "register_mods_panel");
}

} // namespace slugcat::interlace::config
