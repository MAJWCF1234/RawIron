#include "EditorProjectScaffolding.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ri::editor {

namespace fs = std::filesystem;

namespace {

struct ScaffoldTemplateFile {
    fs::path relativePath;
    std::string body;
};

[[nodiscard]] std::vector<ScaffoldTemplateFile> BuildMountedGameScaffoldTemplates(const ri::content::GameManifest& manifest) {
    const std::string gameName = manifest.name.empty() ? manifest.id : manifest.name;
    const std::string primaryLevel = manifest.primaryLevel.empty() ? "levels/assembly.primitives.csv" : manifest.primaryLevel;
    return {
        {"README.md", "# " + gameName + "\n\nCreated with RawIron tooling.\n"},
        {"config/game.cfg", "# RawIron game config\n"
                            "game_id=" + manifest.id + "\n"
                            "game_name=" + gameName + "\n"
                            "default_spawn=0 1.5 0\n"},
        {"config/input.map", "# RawIron input map\nmove_forward=W\nmove_back=S\nmove_left=A\nmove_right=D\njump=Space\nsprint=Shift\n"},
        {"config/network.cfg", "# RawIron network config\nnet_mode=listen\nmax_peers=16\nserver_tick=60\n"},
        {"config/build.profile", "# RawIron build profile\ncontent_profile=dev\nrender_profile=balanced\n"},
        {"config/plugins.policy", "# RawIron plugin policy\nallow_mod_plugins=1\nallow_project_plugins=1\n"},
        {"config/security.policy", "# RawIron security policy\nallow_local_scripts=1\nallow_network_bootstrap=1\n"},
        {"levels/assembly.primitives.csv",
         "name,primitive,parent,px,py,pz,rx,ry,rz,sx,sy,sz,material,texture,tx,ty,r,g,b,a\n"
         "floor,plane,,0,0,0,0,0,0,12,1,12,starter_floor,smooth_stone.png,4,4,0.72,0.74,0.78,1\n"
         "spawn_block,cube,,0,1,0,0,0,0,1,1,1,starter_block,iron_block.png,1,1,0.55,0.58,0.62,1\n"},
        {"levels/assembly.colliders.csv", "name,type,px,py,pz,sx,sy,sz\nfloor_collider,box,0,0,0,12,1,12\n"},
        {"levels/assembly.navmesh", "# RawIron navmesh placeholder\n"},
        {"levels/assembly.ai.nodes", "name,px,py,pz\nspawn_anchor,0,1,0\n"},
        {"levels/assembly.lighting.csv",
         "name,type,px,py,pz,dx,dy,dz,r,g,b,intensity,range\nsun,directional,0,0,0,-0.4,-1.0,0.2,0.92,0.94,1.0,1.35,0\n"},
        {"levels/assembly.cinematics.csv", "name,type,px,py,pz,payload\nintro_marker,marker,0,1,0,opening\n"},
        {"levels/assembly.triggers.csv",
         "trigger_id,event_type,min_x,min_y,min_z,max_x,max_y,max_z,param\n"
         "Trigger_spawn_zone,generic_trigger_volume,-1,0,-1,1,2,1,player_spawn\n"},
        {"levels/assembly.occlusion.csv", "name,type,px,py,pz,sx,sy,sz\nocclusion_anchor,box,0,1,0,4,4,4\n"},
        {"levels/assembly.audio.zones", "name,type,px,py,pz,sx,sy,sz,preset\nambient_core,box,0,1,0,10,4,10,default\n"},
        {"levels/assembly.lods.csv", "name,group,near,mid,far\nstarter_block,default,8,16,32\n"},
        {"levels/assembly.zones.csv", "name,type,px,py,pz,sx,sy,sz\nplay_space,box,0,2,0,24,8,24\n"},
        {"scripts/init.riscript", "# RawIron init script\ngame.id=\"" + manifest.id + "\"\ngame.primary_level=\"" + primaryLevel + "\"\n"},
        {"scripts/gameplay.riscript", "# RawIron gameplay script\nplayer.spawn=\"spawn_zone\"\nplayer.move_speed=6.0\nplayer.jump_speed=7.5\n"},
        {"scripts/logic.riscript", "# RawIron logic script\nlogic.enabled=1\n"},
        {"scripts/rendering.riscript", "# RawIron rendering defaults\nnative_exposure=1.0\nnative_contrast=1.0\nnative_saturation=1.0\nnative_fog_density=0.003\n"},
        {"scripts/postprocess.riscript", "# RawIron postprocess defaults\nbloom=0.10\ngrain=0.00\nvignette=0.00\n"},
        {"scripts/ui.riscript",
         "# RawIron UI script\n"
         "# Scalar-only runtime flags for HUD and diagnostics.\n"
         "# Primary flow authoring lives in `ui/main.ui.json` and `ui/vn_intro.ui.json`.\n"
         "# `ui/layout.xml` and `ui/styling.css` remain optional support assets for HUD/chrome.\n"
         "# runtime_ui_boot_flow: 0=gameplay, 1=menu, 2=vn.\n"
         "# runtime_ui_hotkeys_enabled: allow F1/F2 manifest switching in standalone runtime.\n"
         "show_runtime_diagnostics=0\n"
         "show_objective_panel=1\n"
         "crosshair_mode=1\n"
         "crosshair_scale=1.0\n"
         "hud_style_variant=1\n"
         "runtime_ui_boot_flow=1\n"
         "runtime_ui_hotkeys_enabled=1\n"},
        {"scripts/audio.riscript", "# RawIron audio script\nmusic.enabled=0\nsfx.enabled=1\n"},
        {"scripts/streaming.riscript", "# RawIron streaming script\nstreaming.enabled=1\nstreaming.budget_mb=256\n"},
        {"scripts/localization.riscript", "# RawIron localization script\nlanguage.default=\"en-US\"\n"},
        {"scripts/physics.riscript", "# RawIron physics script\ngravity=9.81\nstep_hz=60\n"},
        {"scripts/network.riscript", "# RawIron network script\nreplication.enabled=1\nprediction.enabled=1\n"},
        {"scripts/persistence.riscript", "# RawIron persistence script\nsave.slot=\"autosave\"\ncheckpoint.enabled=1\n"},
        {"scripts/state.riscript", "# RawIron state script\nstate.bootstrap=\"default\"\n"},
        {"scripts/ai.riscript", "# RawIron AI script\nai.enabled=1\nai.behavior_tree=\"ai/behavior.tree\"\n"},
        {"scripts/plugins.riscript", "# RawIron plugins script\nplugins.manifest=\"plugins/manifest.plugins\"\n"},
        {"scripts/animation.riscript", "# RawIron animation script\nanimation.graph=\"assets/animation.graph\"\n"},
        {"scripts/vfx.riscript", "# RawIron VFX script\nvfx.manifest=\"assets/vfx.manifest\"\n"},
        {"ui/layout.xml", "<layout>\n  <screen id=\"root\">\n    <hud id=\"main_hud\" anchor=\"top-left\" />\n    <panel id=\"status_bar\" anchor=\"bottom-left\" />\n  </screen>\n</layout>\n"},
        {"ui/styling.css", "screen{color:#d7dde6;}\nhud{color:#d7dde6;}\npanel{background:#1f2630;color:#e8edf5;}\n"},
        {"ui/main.ui.json",
         "{\n"
         "  \"schemaVersion\": 1,\n"
         "  \"startScreen\": \"title\",\n"
         "  \"screens\": [\n"
         "    {\n"
         "      \"id\": \"title\",\n"
         "      \"title\": \"" + gameName + "\",\n"
         "      \"background\": {\n"
         "        \"tint\": [0.04, 0.05, 0.10, 0.98]\n"
         "      },\n"
         "      \"blocks\": [\n"
         "        { \"type\": \"heading\", \"text\": \"" + gameName + "\", \"align\": \"center\" },\n"
         "        { \"type\": \"spacer\", \"height\": 24 },\n"
         "        { \"type\": \"paragraph\", \"text\": \"Game-local UI flow. Author screens here inside the game folder.\", \"align\": \"center\" },\n"
         "        { \"type\": \"spacer\", \"height\": 32 },\n"
         "        { \"type\": \"button\", \"label\": \"Play\", \"action\": { \"type\": \"emit\", \"id\": \"game.start\" } },\n"
         "        { \"type\": \"button\", \"label\": \"Story\", \"action\": { \"type\": \"navigate\", \"target\": \"story_intro\" } },\n"
         "        { \"type\": \"button\", \"label\": \"Quit\", \"action\": { \"type\": \"emit\", \"id\": \"app.quit\" } }\n"
         "      ]\n"
         "    },\n"
         "    {\n"
         "      \"id\": \"story_intro\",\n"
         "      \"title\": \"Story intro\",\n"
         "      \"background\": {\n"
         "        \"tint\": [0.06, 0.04, 0.08, 0.96]\n"
         "      },\n"
         "      \"advance\": {\n"
         "        \"onSpace\": true,\n"
         "        \"onClick\": true,\n"
         "        \"onEnter\": true,\n"
         "        \"action\": { \"type\": \"navigate\", \"target\": \"title\" }\n"
         "      },\n"
         "      \"blocks\": [\n"
         "        { \"type\": \"historyNote\", \"text\": \"Act I\", \"backlogOnly\": true },\n"
         "        { \"type\": \"say\", \"speaker\": \"Guide\", \"text\": \"This screen lives in Games/" + gameName + "/ui/main.ui.json.\" },\n"
         "        { \"type\": \"spacer\", \"height\": 16 },\n"
         "        { \"type\": \"narration\", \"text\": \"Use the RawIron editor UI / VN workbench to preview and iterate on this flow.\", \"align\": \"left\" },\n"
         "        { \"type\": \"spacer\", \"height\": 20 },\n"
         "        { \"type\": \"label\", \"text\": \"Space or click to return\", \"align\": \"center\" }\n"
         "      ]\n"
         "    }\n"
         "  ]\n"
         "}\n"},
        {"ui/vn_intro.ui.json",
         "{\n"
         "  \"schemaVersion\": 1,\n"
         "  \"startScreen\": \"vn_opening\",\n"
         "  \"variables\": [\n"
         "    { \"id\": \"route\", \"value\": \"\" }\n"
         "  ],\n"
         "  \"screens\": [\n"
         "    {\n"
         "      \"id\": \"vn_opening\",\n"
         "      \"title\": \"Opening\",\n"
         "      \"background\": { \"tint\": [0.03, 0.03, 0.07, 0.97] },\n"
         "      \"blocks\": [\n"
         "        { \"type\": \"heading\", \"text\": \"" + gameName + " story flow\", \"align\": \"center\" },\n"
         "        { \"type\": \"spacer\", \"height\": 18 },\n"
         "        { \"type\": \"say\", \"speaker\": \"Lead\", \"text\": \"This VN sample belongs to the game folder, not workspace root.\" },\n"
         "        { \"type\": \"spacer\", \"height\": 16 },\n"
         "        {\n"
         "          \"type\": \"choices\",\n"
         "          \"choices\": [\n"
         "            { \"label\": \"Take route A\", \"action\": { \"type\": \"navigate\", \"target\": \"route_a\", \"setVar\": { \"id\": \"route\", \"value\": \"a\" } } },\n"
         "            { \"label\": \"Take route B\", \"action\": { \"type\": \"navigate\", \"target\": \"route_b\", \"setVar\": { \"id\": \"route\", \"value\": \"b\" } } }\n"
         "          ]\n"
         "        }\n"
         "      ]\n"
         "    },\n"
         "    {\n"
         "      \"id\": \"route_a\",\n"
         "      \"title\": \"Route A\",\n"
         "      \"background\": { \"tint\": [0.08, 0.04, 0.04, 0.96] },\n"
         "      \"advance\": { \"onSpace\": true, \"onEnter\": true, \"action\": { \"type\": \"navigate\", \"target\": \"vn_opening\" } },\n"
         "      \"blocks\": [\n"
         "        { \"type\": \"say\", \"speaker\": \"Lead\", \"text\": \"Route A selected.\" },\n"
         "        { \"type\": \"label\", \"text\": \"Space to return\", \"align\": \"center\" }\n"
         "      ]\n"
         "    },\n"
         "    {\n"
         "      \"id\": \"route_b\",\n"
         "      \"title\": \"Route B\",\n"
         "      \"background\": { \"tint\": [0.04, 0.06, 0.08, 0.96] },\n"
         "      \"advance\": { \"onSpace\": true, \"onEnter\": true, \"action\": { \"type\": \"navigate\", \"target\": \"vn_opening\" } },\n"
         "      \"blocks\": [\n"
         "        { \"type\": \"say\", \"speaker\": \"Lead\", \"text\": \"Route B selected.\" },\n"
         "        { \"type\": \"label\", \"text\": \"Space to return\", \"align\": \"center\" }\n"
         "      ]\n"
         "    }\n"
         "  ]\n"
         "}\n"},
        {"menus/main.menu", "menu \"main\" {\n  title \"" + gameName + "\"\n  item \"Play\" action=\"play\"\n  item \"Options\" action=\"options\"\n  item \"Quit\" action=\"quit\"\n}\n"},
        {"ai/behavior.tree", "root\n  sequence\n    condition player_visible\n    action move_to_player\n"},
        {"ai/blackboard.json", "{\n  \"target\": null,\n  \"alert\": false\n}\n"},
        {"ai/factions.cfg", "# RawIron factions\nplayer=allies\ndefault=neutral\n"},
        {"ai/perception.cfg", "# RawIron perception\nvision_range=24\nhearing_range=12\n"},
        {"ai/squad.tactics", "# RawIron squad tactics\nformation=loose\nfallback=hold\n"},
        {"plugins/manifest.plugins", "# RawIron plugin manifest\n"},
        {"plugins/load_order.cfg", "# RawIron plugin load order\n"},
        {"plugins/registry.json", "{\n  \"plugins\": []\n}\n"},
        {"plugins/hooks.riplugin", "# RawIron plugin hooks\n"},
        {"data/schema.db", "SQLite format 3"},
        {"data/save.schema", "# RawIron save schema\nversion=1\n"},
        {"data/entity.registry", "# RawIron entity registry\nplayer\n"},
        {"data/achievements.registry", "# RawIron achievements registry\n"},
        {"data/lookup.index", "# RawIron lookup index\n"},
        {"data/telemetry.db", "SQLite format 3"},
        {"tests/gameplay.test.riscript", "# gameplay smoke test\nassert.player_spawn=spawn_zone\n"},
        {"tests/rendering.test.riscript", "# rendering smoke test\nassert.renderer=native\n"},
        {"tests/network.test.riscript", "# network smoke test\nassert.net_mode=listen\n"},
        {"tests/ui.test.riscript",
         "# ui smoke test\n"
         "assert.flow=ui/main.ui.json\n"
         "assert.vn=ui/vn_intro.ui.json\n"},
        {"assets/palette.ripalette", "# RawIron palette\nentry 0 255 255 255 255\n"},
        {"assets/layers.config", "# RawIron layer config\ndefault=world\n"},
        {"assets/manifest.assets", "# RawIron asset manifest\n"},
        {"assets/metadata.json", "{\n  \"rawironMetadataVersion\": 1,\n  \"project\": \"" + manifest.id + "\"\n}\n"},
        {"assets/dependencies.json", "{\n  \"packages\": []\n}\n"},
        {"assets/materials.manifest", "# RawIron materials manifest\n"},
        {"assets/vfx.manifest", "# RawIron vfx manifest\n"},
        {"assets/shaders.manifest", "# RawIron shaders manifest\n"},
        {"assets/streaming.manifest", "# RawIron streaming manifest\n"},
        {"assets/audio.banks", "# RawIron audio banks\n"},
        {"assets/fonts.manifest", "# RawIron fonts manifest\n"},
        {"assets/animation.graph", "# RawIron animation graph\n"},
    };
}

} // namespace

bool EnsureMountedGameScaffold(const ri::content::GameManifest& manifest,
                               std::size_t& createdCount,
                               std::vector<std::string>& createdFiles,
                               std::string* error) {
    createdCount = 0;
    createdFiles.clear();
    std::error_code ec{};
    fs::create_directories(manifest.rootPath, ec);
    if (ec) {
        if (error != nullptr) {
            *error = "Unable to create game root directories.";
        }
        return false;
    }

    const std::vector<ScaffoldTemplateFile> templates = BuildMountedGameScaffoldTemplates(manifest);
    for (const ScaffoldTemplateFile& file : templates) {
        const fs::path absolutePath = manifest.rootPath / file.relativePath;
        fs::create_directories(absolutePath.parent_path(), ec);
        if (ec) {
            if (error != nullptr) {
                *error = "Unable to create scaffold directories for " + file.relativePath.generic_string() + ".";
            }
            return false;
        }
        if (fs::exists(absolutePath, ec)) {
            continue;
        }
        if (!ri::core::detail::WriteTextFile(absolutePath, file.body)) {
            if (error != nullptr) {
                *error = "Unable to write scaffold file " + file.relativePath.generic_string() + ".";
            }
            return false;
        }
        ++createdCount;
        createdFiles.push_back(file.relativePath.generic_string());
    }
    return true;
}

} // namespace ri::editor
