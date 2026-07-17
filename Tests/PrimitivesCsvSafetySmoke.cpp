#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/PrimitivesCsvIO.h"
#include "RawIron/Scene/Scene.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "check failed at line " << __LINE__ << ": " #condition "\n"; \
            return EXIT_FAILURE; \
        } \
    } while (false)

int main() {
    using namespace ri::scene;
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "rawiron_primitives_csv_safety.csv";
    {
        std::ofstream csv(path, std::ios::binary | std::ios::trunc);
        csv << "  # name,type,x,y,z,sx,sy,sz,r,g,b,shading,texture,tx,ty,rx,ry,rz\n";
        csv << "\"Quoted, Node\", BOX, 12junk, 2, 3, 1, 1, 1, 2, -1, .5, UNLIT, 0.0, 1, 1, 0, 0, 0\n";
        csv << "\"Quoted, Node\",plane,4,5,6,1,1,1,.1,.2,.3,lit,-,1,1,0,0,0\n";
        csv << "Unknown,sphere,0,0,0,1,1,1,1,1,1,lit,-,1,1,0,0,0\n";
        csv << "\"Unclosed,cube,0,0,0,1,1,1,1,1,1,lit,-,1,1,0,0,0\n";
        csv << "\"Closed\"junk,cube,0,0,0,1,1,1,1,1,1,lit,-,1,1,0,0,0\n";
        csv << std::string(64U * 1024U + 1U, 'x') << '\n';
    }

    Scene scene{"csv-safety"};
    const int root = scene.CreateNode("Root");
    AssemblyPrimitivesImportResult result{.spawnedCount = 99, .renamedCount = 99, .skippedRows = 99};
    std::string error = "stale";
    CHECK(TryImportAssemblyPrimitivesCsv(scene, root, path, &result, &error));
    CHECK(error.empty());
    CHECK(result.spawnedCount == 2);
    CHECK(result.renamedCount == 1);
    CHECK(result.skippedRows == 4);
    CHECK(scene.NodeCount() == 3U);

    const Node& first = scene.GetNode(1);
    CHECK(first.name == "Quoted, Node");
    CHECK(first.localTransform.position.x == 0.0f); // strict parse rejects the `12junk` prefix
    CHECK(first.localTransform.position.y == 2.0f);
    CHECK(first.material != kInvalidHandle);
    const Material& material = scene.GetMaterial(first.material);
    CHECK(material.baseColor.x == 1.0f && material.baseColor.y == 0.0f && material.baseColor.z == 0.5f);
    CHECK(material.shadingModel == ShadingModel::Unlit);
    CHECK(material.baseColorTexture.empty());
    CHECK(scene.GetNode(2).name == "Quoted, Node_import1");

    result = {.spawnedCount = 7, .renamedCount = 7, .skippedRows = 7};
    error = "stale";
    CHECK(!TryImportAssemblyPrimitivesCsv(scene, -1, path, &result, &error));
    CHECK(result.spawnedCount == 0 && result.renamedCount == 0 && result.skippedRows == 0);
    CHECK(!error.empty());

    std::filesystem::remove(path);
    return EXIT_SUCCESS;
}
