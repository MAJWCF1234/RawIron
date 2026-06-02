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
        {"config/game.cfg", "# RawIron game config\n"
                            "game_id=" + manifest.id + "\n"
                            "game_name=" + gameName + "\n"
                            "default_spawn=0 1.5 0\n"},
        {"config/network.cfg", "# RawIron network config\nnet_mode=listen\nmax_peers=16\nserver_tick=60\n"},
        {"config/build.profile", "# RawIron build profile\ncontent_profile=dev\nrender_profile=balanced\n"},
        {"config/plugins.policy", "# RawIron plugin policy\nallow_mod_plugins=1\nallow_project_plugins=1\n"},
        {"config/security.policy", "# RawIron security policy\nallow_local_scripts=1\nallow_network_bootstrap=1\n"},
        {"levels/assembly.primitives.csv",
         "name,primitive,parent,px,py,pz,rx,ry,rz,sx,sy,sz,material,texture,tx,ty,r,g,b,a\n"
         "floor,plane,,0,0,0,0,0,0,12,1,12,starter_floor,smooth_stone.png,4,4,0.72,0.74,0.78,1\n"
         "spawn_block,cube,,0,1,0,0,0,0,1,1,1,starter_block,iron_block.png,1,1,0.55,0.58,0.62,1\n"},
        {"levels/assembly.colliders.csv", "name,type,px,py,pz,sx,sy,sz\nfloor_collider,box,0,0,0,12,1,12\n"},
        {"levels/assembly.lighting.csv",
         "name,type,px,py,pz,dx,dy,dz,r,g,b,intensity,range\nsun,directional,0,0,0,-0.4,-1.0,0.2,0.92,0.94,1.0,1.35,0\n"},
        {"levels/assembly.triggers.csv", "name,type,px,py,pz,sx,sy,sz,payload\nspawn_zone,box,0,1,0,2,2,2,player_spawn\n"},
        {"levels/assembly.zones.csv", "name,type,px,py,pz,sx,sy,sz\nplay_space,box,0,2,0,24,8,24\n"},
        {"scripts/init.riscript", "# RawIron init script\ngame.id=\"" + manifest.id + "\"\ngame.primary_level=\"" + primaryLevel + "\"\n"},
        {"scripts/gameplay.riscript", "# RawIron gameplay script\nplayer.spawn=\"spawn_zone\"\nplayer.move_speed=6.0\nplayer.jump_speed=7.5\n"},
        {"scripts/rendering.riscript", "# RawIron rendering defaults\nnative_exposure=1.0\nnative_contrast=1.0\nnative_saturation=1.0\nnative_fog_density=0.003\n"},
        {"scripts/postprocess.riscript", "# RawIron postprocess defaults\nbloom=0.10\ngrain=0.00\nvignette=0.00\n"},
        {"scripts/ui.riscript", "# RawIron UI script\nhud.layout=\"ui/layout.xml\"\nhud.style=\"ui/styling.css\"\nmenu.main=\"menus/main.menu\"\n"},
        {"scripts/audio.riscript", "# RawIron audio script\nmusic.enabled=0\nsfx.enabled=1\n"},
        {"scripts/streaming.riscript", "# RawIron streaming script\nstreaming.enabled=1\nstreaming.budget_mb=256\n"},
        {"scripts/localization.riscript", "# RawIron localization script\nlanguage.default=\"en-US\"\n"},
        {"scripts/physics.riscript", "# RawIron physics script\ngravity=9.81\nstep_hz=60\n"},
        {"scripts/network.riscript", "# RawIron network script\nreplication.enabled=1\nprediction.enabled=1\n"},
        {"scripts/persistence.riscript", "# RawIron persistence script\nsave.slot=\"autosave\"\ncheckpoint.enabled=1\n"},
        {"scripts/ai.riscript", "# RawIron AI script\nai.enabled=1\nai.behavior_tree=\"ai/behavior.tree\"\n"},
        {"scripts/plugins.riscript", "# RawIron plugins script\nplugins.manifest=\"plugins/manifest.plugins\"\n"},
        {"scripts/animation.riscript", "# RawIron animation script\nanimation.graph=\"assets/animation.graph\"\n"},
        {"scripts/vfx.riscript", "# RawIron VFX script\nvfx.manifest=\"assets/vfx.manifest\"\n"},
        {"ui/layout.xml", "<layout>\n  <screen id=\"root\">\n    <hud id=\"main_hud\" anchor=\"top-left\" />\n    <panel id=\"status_bar\" anchor=\"bottom-left\" />\n  </screen>\n</layout>\n"},
        {"ui/styling.css", "screen{color:#d7dde6;}\nhud{color:#d7dde6;}\npanel{background:#1f2630;color:#e8edf5;}\n"},
        {"menus/main.menu", "menu \"main\" {\n  title \"" + gameName + "\"\n  item \"Play\" action=\"play\"\n  item \"Options\" action=\"options\"\n  item \"Quit\" action=\"quit\"\n}\n"},
        {"ai/behavior.tree", "root\n  sequence\n    condition player_visible\n    action move_to_player\n"},
        {"ai/blackboard.json", "{\n  \"target\": null,\n  \"alert\": false\n}\n"},
        {"plugins/manifest.plugins", "# RawIron plugin manifest\n"},
        {"plugins/load_order.cfg", "# RawIron plugin load order\n"},
        {"plugins/registry.json", "{\n  \"plugins\": []\n}\n"},
        {"plugins/hooks.riplugin", "# RawIron plugin hooks\n"},
        {"data/save.schema", "# RawIron save schema\nversion=1\n"},
        {"data/entity.registry", "# RawIron entity registry\nplayer\n"},
        {"data/achievements.registry", "# RawIron achievements registry\n"},
        {"data/lookup.index", "# RawIron lookup index\n"},
        {"tests/gameplay.test.riscript", "# gameplay smoke test\nassert.player_spawn=spawn_zone\n"},
        {"tests/rendering.test.riscript", "# rendering smoke test\nassert.renderer=native\n"},
        {"tests/network.test.riscript", "# network smoke test\nassert.net_mode=listen\n"},
        {"tests/ui.test.riscript", "# ui smoke test\nassert.layout=ui/layout.xml\n"},
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
