#include "RawIron/Scene/Scene.h"

#include <cstdlib>

int main() {
    using namespace ri::scene;

    Scene source{"SnapshotSource"};
    Mesh mesh{};
    mesh.name = "LargeImportedMesh";
    mesh.positions.resize(10000U, ri::math::Vec3{1.0f, 2.0f, 3.0f});
    const int meshHandle = source.AddMesh(std::move(mesh));

    MeshInstanceBatch batch{};
    batch.name = "ForestInstances";
    batch.mesh = meshHandle;
    batch.transforms.resize(5000U);
    const int batchHandle = source.AddMeshInstanceBatch(std::move(batch));

    Scene snapshot = source;
    const Scene& constSource = source;
    const Scene& constSnapshot = snapshot;

    // Copies should share the expensive immutable backing storage.
    if (&constSource.GetMesh(meshHandle) != &constSnapshot.GetMesh(meshHandle)
        || &constSource.GetMeshInstanceBatch(batchHandle) != &constSnapshot.GetMeshInstanceBatch(batchHandle)) {
        return EXIT_FAILURE;
    }

    // Mutable access must detach so editor and renderer snapshots remain isolated.
    snapshot.GetMesh(meshHandle).positions[0].x = 99.0f;
    snapshot.GetMeshInstanceBatch(batchHandle).transforms[0].position.y = 42.0f;
    if (constSource.GetMesh(meshHandle).positions[0].x == 99.0f
        || constSource.GetMeshInstanceBatch(batchHandle).transforms[0].position.y == 42.0f
        || &constSource.GetMesh(meshHandle) == &static_cast<const Scene&>(snapshot).GetMesh(meshHandle)
        || &constSource.GetMeshInstanceBatch(batchHandle)
            == &static_cast<const Scene&>(snapshot).GetMeshInstanceBatch(batchHandle)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
