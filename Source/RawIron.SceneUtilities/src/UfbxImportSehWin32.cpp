#include "RawIron/Scene/UfbxMeshImport.h"

#if defined(_WIN32)

#include <windows.h>

namespace ri::scene {
namespace {

struct UfbxImportSehContext {
    Scene* scene = nullptr;
    const std::filesystem::path* path = nullptr;
    const UfbxSceneImportOptions* options = nullptr;
    std::string* error = nullptr;
    int result = kInvalidHandle;
};

void RunUfbxImport(UfbxImportSehContext* context) {
    context->result =
        ImportUfbxSceneFile(*context->scene, *context->path, *context->options, *context->error);
}

int RunUfbxImportWithSeh(UfbxImportSehContext* context) {
    __try {
        RunUfbxImport(context);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}

} // namespace

int ImportUfbxSceneFileSeh(Scene& targetScene,
                          const std::filesystem::path& path,
                          const UfbxSceneImportOptions& options,
                          std::string& error) {
    UfbxImportSehContext context{
        .scene = &targetScene,
        .path = &path,
        .options = &options,
        .error = &error,
        .result = kInvalidHandle,
    };

    if (RunUfbxImportWithSeh(&context) != 0) {
        error = "Model import crashed while loading: " + path.string();
        return kInvalidHandle;
    }

    return context.result;
}

} // namespace ri::scene

#endif
