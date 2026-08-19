#include "EditorScenePersistence.h"

#include "RawIron/Math/Vec2.h"
#include "RawIron/Math/Vec3.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ri::editor {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view kBundleMagic = "RAWIRON_EDITOR_SCENE_BUNDLE_V1";
constexpr std::string_view kGenerationPrefix = "gen-";
constexpr std::string_view kManifestFilename = "bundle.ri_manifest";
constexpr std::string_view kManifestTemporaryFilename = ".bundle.ri_manifest.tmp";
constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;
constexpr std::uintmax_t kMaxManifestBytes = 64U * 1024U;
constexpr std::uintmax_t kMaxAuthoredBytes = 512U * 1024U * 1024U;
constexpr std::uintmax_t kMaxOrbitBytes = 64U * 1024U;
constexpr std::uintmax_t kMaxLogicBytes = 64U * 1024U * 1024U;
constexpr std::size_t kGenerationReservationRetries = 128U;

struct EditorAuthoredNodeRecord {
    std::string name;
    std::string parentName;
    ri::scene::Transform localTransform{};
    bool hasMesh = false;
    ri::scene::Mesh mesh{};
    bool hasMaterial = false;
    ri::scene::Material material{};
};

struct ComponentFingerprint {
    std::uintmax_t size = 0U;
    std::uint64_t hash = kFnvOffset;
};

struct BundleManifest {
    std::uint64_t generation = 0U;
    ComponentFingerprint transform{};
    ComponentFingerprint authored{};
    ComponentFingerprint orbit{};
    ComponentFingerprint logic{};
};

struct NewFileWriteResult {
    bool succeeded = false;
    bool synchronized = false;
    std::error_code error{};
};

void WriteVec2(std::ostream& stream, const ri::math::Vec2& value) {
    stream << value.x << " " << value.y;
}

void WriteVec3(std::ostream& stream, const ri::math::Vec3& value) {
    stream << value.x << " " << value.y << " " << value.z;
}

bool ReadVec2(std::istream& stream, ri::math::Vec2& value) {
    return static_cast<bool>(stream >> value.x >> value.y);
}

bool ReadVec3(std::istream& stream, ri::math::Vec3& value) {
    return static_cast<bool>(stream >> value.x >> value.y >> value.z);
}

[[nodiscard]] bool SerializeAuthoredSceneState(const ri::scene::Scene& scene,
                                               const std::size_t authoredNodeStart,
                                               const int editorTrashHandle,
                                               std::string& bytes,
                                               std::string& error) {
    std::vector<EditorAuthoredNodeRecord> records;
    const std::size_t nodeCount = scene.NodeCount();
    std::unordered_map<std::string, int> existingNames;
    for (std::size_t index = 0; index < std::min(authoredNodeStart, nodeCount); ++index) {
        existingNames.emplace(scene.GetNode(static_cast<int>(index)).name, static_cast<int>(index));
    }
    for (std::size_t index = authoredNodeStart; index < nodeCount; ++index) {
        const ri::scene::Node& node = scene.GetNode(static_cast<int>(index));
        if (static_cast<int>(index) == editorTrashHandle || node.parent == editorTrashHandle) {
            continue;
        }
        if (node.name.empty()) {
            error = "authored node name is empty";
            return false;
        }
        if (existingNames.contains(node.name)) {
            error = "duplicate authored node name '" + node.name + "'";
            return false;
        }
        existingNames.emplace(node.name, static_cast<int>(index));

        EditorAuthoredNodeRecord record{};
        record.name = node.name;
        record.localTransform = node.localTransform;
        if (node.parent != ri::scene::kInvalidHandle
            && static_cast<std::size_t>(node.parent) < scene.NodeCount()) {
            record.parentName = scene.GetNode(node.parent).name;
        }
        if (node.mesh != ri::scene::kInvalidHandle && static_cast<std::size_t>(node.mesh) < scene.MeshCount()) {
            record.hasMesh = true;
            record.mesh = scene.GetMesh(node.mesh);
        }
        if (node.material != ri::scene::kInvalidHandle
            && static_cast<std::size_t>(node.material) < scene.MaterialCount()) {
            record.hasMaterial = true;
            record.material = scene.GetMaterial(node.material);
        }
        records.push_back(std::move(record));
    }

    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << "RAWIRON_EDITOR_AUTHORED_SCENE_V1\n";
    stream << "node_count " << records.size() << "\n";
    for (const EditorAuthoredNodeRecord& record : records) {
        stream << "node "
               << std::quoted(record.name) << " "
               << std::quoted(record.parentName) << " ";
        WriteVec3(stream, record.localTransform.position);
        stream << " ";
        WriteVec3(stream, record.localTransform.rotationDegrees);
        stream << " ";
        WriteVec3(stream, record.localTransform.scale);
        stream << " " << (record.hasMesh ? 1 : 0) << " " << (record.hasMaterial ? 1 : 0) << "\n";

        if (record.hasMesh) {
            stream << "mesh "
                   << std::quoted(record.mesh.name) << " "
                   << static_cast<int>(record.mesh.primitive) << " "
                   << record.mesh.vertexCount << " "
                   << record.mesh.indexCount << " "
                   << record.mesh.positions.size() << " ";
            for (const ri::math::Vec3& position : record.mesh.positions) {
                WriteVec3(stream, position);
                stream << " ";
            }
            stream << record.mesh.texCoords.size() << " ";
            for (const ri::math::Vec2& texCoord : record.mesh.texCoords) {
                WriteVec2(stream, texCoord);
                stream << " ";
            }
            stream << record.mesh.indices.size();
            for (const int index : record.mesh.indices) {
                stream << " " << index;
            }
            stream << "\n";
        }

        if (record.hasMaterial) {
            stream << "material "
                   << std::quoted(record.material.name) << " "
                   << static_cast<int>(record.material.shadingModel) << " ";
            WriteVec3(stream, record.material.baseColor);
            stream << " "
                   << std::quoted(record.material.baseColorTexture) << " "
                   << record.material.baseColorTextureFrames.size();
            for (const std::string& frame : record.material.baseColorTextureFrames) {
                stream << " " << std::quoted(frame);
            }
            stream << " " << record.material.baseColorTextureFramesPerSecond << " ";
            WriteVec2(stream, record.material.textureTiling);
            stream << " ";
            WriteVec3(stream, record.material.emissiveColor);
            stream << " "
                   << record.material.metallic << " "
                   << record.material.roughness << " "
                   << record.material.opacity << " "
                   << record.material.alphaCutoff << " "
                   << (record.material.doubleSided ? 1 : 0) << " "
                   << (record.material.transparent ? 1 : 0) << " "
                   << std::quoted(record.material.normalTexture) << " "
                   << std::quoted(record.material.ormTexture) << " "
                   << std::quoted(record.material.roughnessTexture) << " "
                   << std::quoted(record.material.metallicTexture) << " "
                   << std::quoted(record.material.emissiveTexture) << " "
                   << std::quoted(record.material.opacityTexture) << " "
                   << std::quoted(record.material.occlusionTexture) << "\n";
        }
        if (!stream.good() || stream.tellp() > static_cast<std::streamoff>(kMaxAuthoredBytes)) {
            error = "authored scene sidecar exceeds the 512 MiB safety limit";
            return false;
        }
    }

    bytes = stream.str();
    if (!stream.good() || bytes.size() > kMaxAuthoredBytes) {
        error = "could not serialize authored scene sidecar";
        return false;
    }
    return true;
}

[[nodiscard]] bool LoadAuthoredSceneState(ri::scene::Scene& scene,
                                          const fs::path& inputPath,
                                          std::string& errorMessage) {
    std::error_code sizeError;
    const std::uintmax_t fileSize = fs::file_size(inputPath, sizeError);
    if (sizeError || fileSize > kMaxAuthoredBytes) {
        errorMessage = sizeError ? "authored scene sidecar could not be inspected"
                                 : "authored scene sidecar exceeds safety limit";
        return false;
    }
    std::ifstream stream(inputPath, std::ios::binary);
    if (!stream.is_open()) {
        errorMessage = "authored scene sidecar missing";
        return false;
    }
    stream.imbue(std::locale::classic());

    std::string magic;
    std::getline(stream, magic);
    if (!magic.empty() && magic.back() == '\r') {
        magic.pop_back();
    }
    if (magic != "RAWIRON_EDITOR_AUTHORED_SCENE_V1") {
        errorMessage = "invalid authored scene sidecar header";
        return false;
    }

    std::string header;
    std::size_t nodeCount = 0;
    stream >> header >> nodeCount;
    if (!stream.good() || header != "node_count") {
        errorMessage = "invalid authored scene sidecar count";
        return false;
    }
    constexpr std::size_t kMaxAuthoredNodes = 100000U;
    constexpr std::size_t kMaxMeshElements = 10000000U;
    constexpr std::size_t kMaxTextureFrames = 10000U;
    if (nodeCount > kMaxAuthoredNodes) {
        errorMessage = "authored scene sidecar node count exceeds safety limit";
        return false;
    }

    // Parse into a copy so a truncated or corrupt sidecar cannot leave a
    // half-imported hierarchy in the candidate scene.
    ri::scene::Scene loadedScene = scene;
    std::unordered_map<std::string, int> nodeByName;
    for (std::size_t index = 0; index < loadedScene.NodeCount(); ++index) {
        nodeByName.emplace(loadedScene.GetNode(static_cast<int>(index)).name, static_cast<int>(index));
    }

    std::size_t loaded = 0;
    while (loaded < nodeCount && stream.good()) {
        std::string token;
        stream >> token;
        if (!stream.good() || token != "node") {
            break;
        }

        EditorAuthoredNodeRecord record{};
        int hasMesh = 0;
        int hasMaterial = 0;
        stream >> std::quoted(record.name) >> std::quoted(record.parentName);
        if (!ReadVec3(stream, record.localTransform.position)
            || !ReadVec3(stream, record.localTransform.rotationDegrees)
            || !ReadVec3(stream, record.localTransform.scale)
            || !(stream >> hasMesh >> hasMaterial)) {
            errorMessage = "malformed authored node record";
            return false;
        }
        record.hasMesh = hasMesh != 0;
        record.hasMaterial = hasMaterial != 0;

        if (record.hasMesh) {
            std::string meshToken;
            std::size_t positionCount = 0;
            std::size_t texCoordCount = 0;
            std::size_t indexCount = 0;
            int primitive = 0;
            stream >> meshToken
                   >> std::quoted(record.mesh.name)
                   >> primitive
                   >> record.mesh.vertexCount
                   >> record.mesh.indexCount
                   >> positionCount;
            if (!stream.good() || meshToken != "mesh") {
                errorMessage = "malformed mesh record";
                return false;
            }
            if (positionCount > kMaxMeshElements) {
                errorMessage = "mesh position count exceeds safety limit";
                return false;
            }
            record.mesh.primitive = static_cast<ri::scene::PrimitiveType>(primitive);
            record.mesh.positions.resize(positionCount);
            for (ri::math::Vec3& position : record.mesh.positions) {
                if (!ReadVec3(stream, position)) {
                    errorMessage = "malformed mesh positions";
                    return false;
                }
            }
            stream >> texCoordCount;
            if (!stream.good() || texCoordCount > kMaxMeshElements) {
                errorMessage = "mesh texcoord count exceeds safety limit";
                return false;
            }
            record.mesh.texCoords.resize(texCoordCount);
            for (ri::math::Vec2& texCoord : record.mesh.texCoords) {
                if (!ReadVec2(stream, texCoord)) {
                    errorMessage = "malformed mesh texcoords";
                    return false;
                }
            }
            stream >> indexCount;
            if (!stream.good() || indexCount > kMaxMeshElements) {
                errorMessage = "mesh index count exceeds safety limit";
                return false;
            }
            record.mesh.indices.resize(indexCount);
            for (int& indexValue : record.mesh.indices) {
                if (!(stream >> indexValue)) {
                    errorMessage = "malformed mesh indices";
                    return false;
                }
            }
        }

        if (record.hasMaterial) {
            std::string materialToken;
            std::size_t frameCount = 0;
            int shadingModel = 0;
            int doubleSided = 0;
            int transparent = 0;
            stream >> materialToken
                   >> std::quoted(record.material.name)
                   >> shadingModel;
            if (!stream.good() || materialToken != "material") {
                errorMessage = "malformed material record";
                return false;
            }
            record.material.shadingModel = static_cast<ri::scene::ShadingModel>(shadingModel);
            if (!ReadVec3(stream, record.material.baseColor)
                || !(stream >> std::quoted(record.material.baseColorTexture) >> frameCount)) {
                errorMessage = "malformed material header";
                return false;
            }
            if (frameCount > kMaxTextureFrames) {
                errorMessage = "material texture frame count exceeds safety limit";
                return false;
            }
            record.material.baseColorTextureFrames.resize(frameCount);
            for (std::string& frame : record.material.baseColorTextureFrames) {
                if (!(stream >> std::quoted(frame))) {
                    errorMessage = "malformed material frame list";
                    return false;
                }
            }
            if (!(stream >> record.material.baseColorTextureFramesPerSecond)
                || !ReadVec2(stream, record.material.textureTiling)
                || !ReadVec3(stream, record.material.emissiveColor)
                || !(stream >> record.material.metallic
                     >> record.material.roughness
                     >> record.material.opacity
                     >> record.material.alphaCutoff
                     >> doubleSided
                     >> transparent
                     >> std::quoted(record.material.normalTexture)
                     >> std::quoted(record.material.ormTexture)
                     >> std::quoted(record.material.roughnessTexture)
                     >> std::quoted(record.material.metallicTexture)
                     >> std::quoted(record.material.emissiveTexture)
                     >> std::quoted(record.material.opacityTexture)
                     >> std::quoted(record.material.occlusionTexture))) {
                errorMessage = "malformed material payload";
                return false;
            }
            record.material.doubleSided = doubleSided != 0;
            record.material.transparent = transparent != 0;
        }

        if (record.name.empty()) {
            errorMessage = "authored node name is empty";
            return false;
        }
        if (nodeByName.contains(record.name)) {
            errorMessage = "duplicate authored node name '" + record.name + "'";
            return false;
        }

        int parent = ri::scene::kInvalidHandle;
        if (!record.parentName.empty()) {
            const auto found = nodeByName.find(record.parentName);
            if (found == nodeByName.end()) {
                errorMessage = "missing parent node '" + record.parentName + "'";
                return false;
            }
            parent = found->second;
        }

        const int nodeHandle = loadedScene.CreateNode(record.name, parent);
        ri::scene::Node& node = loadedScene.GetNode(nodeHandle);
        node.localTransform = record.localTransform;
        if (record.hasMaterial) {
            const int materialHandle = loadedScene.AddMaterial(record.material);
            if (record.hasMesh) {
                const int meshHandle = loadedScene.AddMesh(record.mesh);
                loadedScene.AttachMesh(nodeHandle, meshHandle, materialHandle);
            }
        } else if (record.hasMesh) {
            const int meshHandle = loadedScene.AddMesh(record.mesh);
            loadedScene.AttachMesh(nodeHandle, meshHandle);
        }

        nodeByName[record.name] = nodeHandle;
        ++loaded;
    }

    if (loaded != nodeCount) {
        errorMessage = "authored scene sidecar truncated";
        return false;
    }
    stream >> std::ws;
    if (!stream.eof()) {
        errorMessage = "authored scene sidecar contains trailing data";
        return false;
    }

    scene = std::move(loadedScene);
    return true;
}

[[nodiscard]] std::string SerializeOrbitState(const ri::scene::OrbitCameraState& orbit) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << "RAWIRON_EDITOR_ORBIT_V1\n";
    stream << std::fixed << std::setprecision(8);
    stream << orbit.target.x << " " << orbit.target.y << " " << orbit.target.z << "\n";
    stream << orbit.distance << "\n";
    stream << orbit.yawDegrees << " " << orbit.pitchDegrees << "\n";
    return stream.str();
}

[[nodiscard]] bool LoadOrbitState(const fs::path& path,
                                  ri::scene::OrbitCameraState& orbit,
                                  std::string& error) {
    std::error_code sizeError;
    const std::uintmax_t fileSize = fs::file_size(path, sizeError);
    if (sizeError || fileSize > kMaxOrbitBytes) {
        error = sizeError ? "orbit sidecar could not be inspected" : "orbit sidecar exceeds safety limit";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    stream.imbue(std::locale::classic());
    if (!stream.is_open()) {
        error = "orbit sidecar missing";
        return false;
    }
    std::string magic;
    stream >> magic;
    if (magic != "RAWIRON_EDITOR_ORBIT_V1") {
        error = "invalid orbit sidecar header";
        return false;
    }
    if (!(stream >> orbit.target.x >> orbit.target.y >> orbit.target.z
          >> orbit.distance >> orbit.yawDegrees >> orbit.pitchDegrees)) {
        error = "malformed orbit sidecar";
        return false;
    }
    if (!std::isfinite(orbit.target.x) || !std::isfinite(orbit.target.y) || !std::isfinite(orbit.target.z)
        || !std::isfinite(orbit.distance) || !std::isfinite(orbit.yawDegrees) || !std::isfinite(orbit.pitchDegrees)
        || orbit.distance <= 0.0f) {
        error = "orbit sidecar contains invalid numeric state";
        return false;
    }
    stream >> std::ws;
    if (!stream.eof()) {
        error = "orbit sidecar contains trailing data";
        return false;
    }
    return true;
}

[[nodiscard]] std::uint64_t HashBytes(const std::string_view bytes) noexcept {
    std::uint64_t hash = kFnvOffset;
    for (const unsigned char value : bytes) {
        hash ^= static_cast<std::uint64_t>(value);
        hash *= kFnvPrime;
    }
    return hash;
}

[[nodiscard]] std::string FormatHash(const std::uint64_t hash) {
    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << hash;
    return stream.str();
}

[[nodiscard]] bool ParseHash(const std::string_view text, std::uint64_t& hash) noexcept {
    if (text.size() != 16U) {
        return false;
    }
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), hash, 16);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

[[nodiscard]] bool FingerprintFile(const fs::path& path,
                                   const std::uintmax_t maxBytes,
                                   ComponentFingerprint& fingerprint,
                                   std::string& error) {
    std::error_code statusError;
    const fs::file_status status = fs::symlink_status(path, statusError);
    if (statusError || !fs::is_regular_file(status) || fs::is_symlink(status)) {
        error = "component is missing, non-regular, or a symbolic link: " + path.filename().string();
        return false;
    }
    const std::uintmax_t fileSize = fs::file_size(path, statusError);
    if (statusError || fileSize > maxBytes) {
        error = statusError ? "could not inspect component size: " + path.filename().string()
                            : "component exceeds its safety limit: " + path.filename().string();
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        error = "could not read component: " + path.filename().string();
        return false;
    }
    std::array<char, 64U * 1024U> buffer{};
    fingerprint = {};
    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = stream.gcount();
        if (count < 0 || fingerprint.size + static_cast<std::uintmax_t>(count) > maxBytes) {
            error = "component changed size while being read: " + path.filename().string();
            return false;
        }
        for (std::streamsize index = 0; index < count; ++index) {
            fingerprint.hash ^= static_cast<std::uint8_t>(buffer[static_cast<std::size_t>(index)]);
            fingerprint.hash *= kFnvPrime;
        }
        fingerprint.size += static_cast<std::uintmax_t>(count);
    }
    if (stream.bad() || fingerprint.size != fileSize) {
        error = "component read was incomplete: " + path.filename().string();
        return false;
    }
    return true;
}

#if defined(_WIN32)
[[nodiscard]] std::error_code LastPlatformError() {
    return std::error_code(static_cast<int>(GetLastError()), std::system_category());
}

[[nodiscard]] NewFileWriteResult WriteNewFileDurably(const fs::path& path, const std::string_view bytes) {
    HANDLE handle = CreateFileW(path.c_str(),
                                GENERIC_WRITE,
                                FILE_SHARE_READ,
                                nullptr,
                                CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return {.error = LastPlatformError()};
    }
    NewFileWriteResult result{};
    std::size_t offset = 0U;
    while (offset < bytes.size()) {
        const std::size_t remaining = bytes.size() - offset;
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, std::numeric_limits<DWORD>::max()));
        DWORD written = 0U;
        if (WriteFile(handle, bytes.data() + offset, chunk, &written, nullptr) == FALSE || written != chunk) {
            result.error = LastPlatformError();
            (void)CloseHandle(handle);
            return result;
        }
        offset += written;
    }
    if (FlushFileBuffers(handle) == FALSE) {
        result.error = LastPlatformError();
        (void)CloseHandle(handle);
        return result;
    }
    result.synchronized = true;
    if (CloseHandle(handle) == FALSE) {
        result.error = LastPlatformError();
        return result;
    }
    result.succeeded = true;
    return result;
}

[[nodiscard]] bool PublishManifestNoReplace(const fs::path& temporary,
                                            const fs::path& destination,
                                            std::error_code& error) {
    if (MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH) == FALSE) {
        error = LastPlatformError();
        return false;
    }
    return true;
}

[[nodiscard]] bool SyncDirectory(const fs::path&, std::error_code&) {
    // Win32 has no generally supported directory-fsync equivalent. Component
    // files and the no-replace manifest move are individually write-through.
    return false;
}
#else
[[nodiscard]] std::error_code LastPlatformError() {
    return std::error_code(errno, std::generic_category());
}

[[nodiscard]] NewFileWriteResult WriteNewFileDurably(const fs::path& path, const std::string_view bytes) {
    const int descriptor = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (descriptor < 0) {
        return {.error = LastPlatformError()};
    }
    NewFileWriteResult result{};
    std::size_t offset = 0U;
    while (offset < bytes.size()) {
        const ssize_t written = write(descriptor, bytes.data() + offset, bytes.size() - offset);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            result.error = LastPlatformError();
            (void)close(descriptor);
            return result;
        }
        if (written == 0) {
            result.error = std::make_error_code(std::errc::io_error);
            (void)close(descriptor);
            return result;
        }
        offset += static_cast<std::size_t>(written);
    }
    if (fsync(descriptor) != 0) {
        result.error = LastPlatformError();
        (void)close(descriptor);
        return result;
    }
    result.synchronized = true;
    if (close(descriptor) != 0) {
        result.error = LastPlatformError();
        return result;
    }
    result.succeeded = true;
    return result;
}

[[nodiscard]] bool PublishManifestNoReplace(const fs::path& temporary,
                                            const fs::path& destination,
                                            std::error_code& error) {
    if (link(temporary.c_str(), destination.c_str()) != 0) {
        error = LastPlatformError();
        return false;
    }
    if (unlink(temporary.c_str()) != 0) {
        // Publication already happened. Keep the duplicate temporary link and
        // report a durability/cleanup warning through directory synchronization.
        error = LastPlatformError();
    }
    return true;
}

[[nodiscard]] bool SyncDirectory(const fs::path& path, std::error_code& error) {
    const int descriptor = open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (descriptor < 0) {
        error = LastPlatformError();
        return false;
    }
    const bool succeeded = fsync(descriptor) == 0;
    if (!succeeded) {
        error = LastPlatformError();
    }
    if (close(descriptor) != 0 && succeeded) {
        error = LastPlatformError();
        return false;
    }
    return succeeded;
}
#endif

[[nodiscard]] std::string GenerationDirectoryName(const std::uint64_t generation) {
    std::ostringstream stream;
    stream << kGenerationPrefix << std::setw(20) << std::setfill('0') << generation;
    return stream.str();
}

[[nodiscard]] bool ParseGenerationDirectoryName(const std::string_view name,
                                                std::uint64_t& generation) noexcept {
    if (!name.starts_with(kGenerationPrefix) || name.size() != kGenerationPrefix.size() + 20U) {
        return false;
    }
    const std::string_view digits = name.substr(kGenerationPrefix.size());
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), generation, 10);
    return parsed.ec == std::errc{} && parsed.ptr == digits.data() + digits.size() && generation != 0U;
}

[[nodiscard]] EditorSceneBundlePaths BuildGenerationPaths(const fs::path& generationDirectory,
                                                           const std::uint64_t generation) {
    return EditorSceneBundlePaths{
        .generation = generation,
        .generationDirectory = generationDirectory,
        .manifestPath = generationDirectory / kManifestFilename,
        .transformPath = generationDirectory / "scene_state.ri_state",
        .authoredPath = generationDirectory / "authored_scene.ri_editor",
        .orbitPath = generationDirectory / "editor_orbit.ri_cam",
        .logicPath = generationDirectory / "logic_authoring.ri_logic",
    };
}

/// Reject symlink/reparse components in `path` and ancestors before create_directories
/// can divert new trees outside the intended project root.
[[nodiscard]] bool PathPrefixHasIndirection(const fs::path& path, std::string& error) {
    if (path.empty()) {
        return false;
    }
    std::error_code absoluteError;
    fs::path current = fs::absolute(path, absoluteError);
    if (absoluteError) {
        error = "path could not be inspected: " + absoluteError.message();
        return true;
    }
    std::vector<fs::path> chain;
    for (;;) {
        chain.push_back(current);
        if (!current.has_relative_path()) {
            break;
        }
        const fs::path parent = current.parent_path();
        if (parent.empty() || parent == current) {
            break;
        }
        current = parent;
    }
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        std::error_code statusError;
        const fs::file_status status = fs::symlink_status(*it, statusError);
        if (statusError == std::errc::no_such_file_or_directory
            || status.type() == fs::file_type::not_found) {
            continue;
        }
        if (statusError) {
            error = "path could not be inspected: " + statusError.message();
            return true;
        }
        if (fs::is_symlink(status)) {
            error = "path must not contain a symlink or reparse component";
            return true;
        }
#if defined(_WIN32)
        const DWORD attributes = GetFileAttributesW(it->c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            const DWORD lastError = GetLastError();
            if (lastError == ERROR_FILE_NOT_FOUND || lastError == ERROR_PATH_NOT_FOUND) {
                continue;
            }
            error = "path could not be inspected";
            return true;
        }
        if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
            error = "path must not contain a symlink or reparse component";
            return true;
        }
#endif
    }
    return false;
}

[[nodiscard]] bool ReserveGeneration(const EditorSceneBundleSlotPaths& slot,
                                     EditorSceneBundlePaths& paths,
                                     std::string& error) {
    if (slot.slotRoot.empty()) {
        error = "bundle slot root is empty";
        return false;
    }
    if (PathPrefixHasIndirection(slot.slotRoot, error)) {
        return false;
    }
    std::error_code ec;
    fs::create_directories(slot.slotRoot, ec);
    if (ec) {
        error = "could not create bundle slot root: " + ec.message();
        return false;
    }
    const fs::file_status rootStatus = fs::symlink_status(slot.slotRoot, ec);
    if (ec || !fs::is_directory(rootStatus) || fs::is_symlink(rootStatus)) {
        error = "bundle slot root is not a regular directory";
        return false;
    }
#if defined(_WIN32)
    const DWORD rootAttributes = GetFileAttributesW(slot.slotRoot.c_str());
    if (rootAttributes == INVALID_FILE_ATTRIBUTES
        || (rootAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
        error = "bundle slot root is not a regular directory";
        return false;
    }
#endif

    std::uint64_t maximum = 0U;
    for (fs::directory_iterator it(slot.slotRoot, ec), end; !ec && it != end; it.increment(ec)) {
        const fs::directory_entry& entry = *it;
        std::uint64_t generation = 0U;
        if (entry.is_directory(ec)
            && !ec
            && ParseGenerationDirectoryName(entry.path().filename().string(), generation)) {
            maximum = std::max(maximum, generation);
        }
    }
    if (ec) {
        error = "could not enumerate bundle generations: " + ec.message();
        return false;
    }
    if (maximum == std::numeric_limits<std::uint64_t>::max()) {
        error = "bundle generation counter is exhausted";
        return false;
    }

    std::uint64_t candidate = maximum + 1U;
    for (std::size_t attempt = 0U; attempt < kGenerationReservationRetries; ++attempt) {
        const fs::path directory = slot.slotRoot / GenerationDirectoryName(candidate);
        ec.clear();
        if (fs::create_directory(directory, ec)) {
            paths = BuildGenerationPaths(directory, candidate);
            return true;
        }
        if (ec && ec != std::errc::file_exists) {
            error = "could not reserve bundle generation: " + ec.message();
            return false;
        }
        if (candidate == std::numeric_limits<std::uint64_t>::max()) {
            break;
        }
        ++candidate;
    }
    error = "could not reserve a unique bundle generation";
    return false;
}

[[nodiscard]] std::string SerializeManifest(const BundleManifest& manifest) {
    std::ostringstream stream;
    stream << kBundleMagic << "\n";
    stream << "generation " << manifest.generation << "\n";
    const auto writeComponent = [&stream](const std::string_view name, const ComponentFingerprint& fingerprint) {
        stream << name << " " << fingerprint.size << " " << FormatHash(fingerprint.hash) << "\n";
    };
    writeComponent("transform", manifest.transform);
    writeComponent("authored", manifest.authored);
    writeComponent("orbit", manifest.orbit);
    writeComponent("logic", manifest.logic);
    return stream.str();
}

[[nodiscard]] bool ParseManifest(const fs::path& path,
                                 const std::uint64_t expectedGeneration,
                                 BundleManifest& manifest,
                                 std::string& error) {
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(path, ec);
    if (ec || !fs::is_regular_file(status) || fs::is_symlink(status)) {
        error = "bundle manifest is missing, non-regular, or a symbolic link";
        return false;
    }
    const std::uintmax_t size = fs::file_size(path, ec);
    if (ec || size > kMaxManifestBytes) {
        error = ec ? "bundle manifest could not be inspected" : "bundle manifest exceeds safety limit";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        error = "bundle manifest could not be opened";
        return false;
    }
    std::ostringstream bytes;
    bytes << input.rdbuf();
    if (input.bad() || static_cast<std::uintmax_t>(bytes.tellp()) != size) {
        error = "bundle manifest read was incomplete";
        return false;
    }
    std::istringstream stream(bytes.str());
    stream.imbue(std::locale::classic());
    std::string magic;
    if (!std::getline(stream, magic)) {
        error = "bundle manifest is empty";
        return false;
    }
    if (!magic.empty() && magic.back() == '\r') {
        magic.pop_back();
    }
    if (magic != kBundleMagic) {
        error = "bundle manifest has an unsupported header";
        return false;
    }
    std::string generationKey;
    if (!(stream >> generationKey >> manifest.generation)
        || generationKey != "generation"
        || manifest.generation != expectedGeneration) {
        error = "bundle manifest generation does not match its directory";
        return false;
    }
    const auto readComponent = [&stream, &error](const std::string_view expected,
                                                 ComponentFingerprint& fingerprint) {
        std::string name;
        std::string hash;
        if (!(stream >> name >> fingerprint.size >> hash) || name != expected || !ParseHash(hash, fingerprint.hash)) {
            error = "bundle manifest has a malformed " + std::string(expected) + " fingerprint";
            return false;
        }
        return true;
    };
    if (!readComponent("transform", manifest.transform)
        || !readComponent("authored", manifest.authored)
        || !readComponent("orbit", manifest.orbit)
        || !readComponent("logic", manifest.logic)) {
        return false;
    }
    stream >> std::ws;
    if (!stream.eof()) {
        error = "bundle manifest contains trailing data";
        return false;
    }
    return true;
}

[[nodiscard]] bool SameFingerprint(const ComponentFingerprint& left,
                                   const ComponentFingerprint& right) noexcept {
    return left.size == right.size && left.hash == right.hash;
}

[[nodiscard]] bool ValidateGenerationFingerprints(const EditorSceneBundlePaths& paths,
                                                  const BundleManifest& manifest,
                                                  std::string& error) {
    ComponentFingerprint actual{};
    if (!FingerprintFile(paths.transformPath, ri::scene::kMaxSceneStateFileBytes, actual, error)
        || !SameFingerprint(actual, manifest.transform)) {
        if (error.empty()) {
            error = "transform component fingerprint mismatch";
        }
        return false;
    }
    if (!FingerprintFile(paths.authoredPath, kMaxAuthoredBytes, actual, error)
        || !SameFingerprint(actual, manifest.authored)) {
        if (error.empty()) {
            error = "authored component fingerprint mismatch";
        }
        return false;
    }
    if (!FingerprintFile(paths.orbitPath, kMaxOrbitBytes, actual, error)
        || !SameFingerprint(actual, manifest.orbit)) {
        if (error.empty()) {
            error = "orbit component fingerprint mismatch";
        }
        return false;
    }
    if (!FingerprintFile(paths.logicPath, kMaxLogicBytes, actual, error)
        || !SameFingerprint(actual, manifest.logic)) {
        if (error.empty()) {
            error = "logic component fingerprint mismatch";
        }
        return false;
    }
    return true;
}

struct PublishedGenerationResolution {
    bool found = false;
    bool sawPublishedButInvalid = false;
    bool recoveredPrevious = false;
    EditorSceneBundlePaths paths{};
    BundleManifest manifest{};
    std::string diagnostic;
};

[[nodiscard]] PublishedGenerationResolution ResolveLatestPublishedGeneration(
    const EditorSceneBundleSlotPaths& slot) {
    PublishedGenerationResolution result{};
    std::error_code ec;
    if (!fs::exists(slot.slotRoot, ec) || ec) {
        return result;
    }
    std::vector<std::pair<std::uint64_t, fs::path>> generations;
    for (fs::directory_iterator it(slot.slotRoot, ec), end; !ec && it != end; it.increment(ec)) {
        std::uint64_t generation = 0U;
        if (it->is_directory(ec)
            && !ec
            && ParseGenerationDirectoryName(it->path().filename().string(), generation)) {
            generations.emplace_back(generation, it->path());
        }
    }
    if (ec) {
        result.sawPublishedButInvalid = true;
        result.diagnostic = "could not enumerate bundle generations: " + ec.message();
        return result;
    }
    std::sort(generations.begin(), generations.end(), [](const auto& left, const auto& right) {
        return left.first > right.first;
    });

    bool skippedInvalidPublished = false;
    for (const auto& [generation, directory] : generations) {
        EditorSceneBundlePaths paths = BuildGenerationPaths(directory, generation);
        const bool manifestExists = fs::exists(paths.manifestPath, ec);
        if (ec) {
            skippedInvalidPublished = true;
            result.sawPublishedButInvalid = true;
            result.diagnostic = "could not inspect bundle manifest: " + ec.message();
            ec.clear();
            continue;
        }
        if (!manifestExists) {
            // A crash or injected failure before publication deliberately leaves
            // an uncommitted generation without a final manifest.
            continue;
        }
        result.sawPublishedButInvalid = true;
        BundleManifest manifest{};
        std::string validationError;
        if (!ParseManifest(paths.manifestPath, generation, manifest, validationError)
            || !ValidateGenerationFingerprints(paths, manifest, validationError)) {
            skippedInvalidPublished = true;
            result.diagnostic = "generation " + std::to_string(generation) + " rejected: " + validationError;
            continue;
        }
        result.found = true;
        result.paths = std::move(paths);
        result.manifest = manifest;
        result.recoveredPrevious = skippedInvalidPublished;
        if (result.recoveredPrevious) {
            result.diagnostic = "newer published generation was invalid; recovered generation "
                + std::to_string(generation);
        } else {
            result.diagnostic.clear();
        }
        return result;
    }
    return result;
}

[[nodiscard]] bool LoadCompleteGeneration(const EditorSceneBundlePaths& paths,
                                          const EditorSceneBundleLoadInput& input,
                                          EditorSceneBundleLoadedState& output,
                                          ri::scene::SceneStateIOResult& transformResult,
                                          std::string& error) {
    if (input.baselineScene == nullptr) {
        error = "baseline scene is missing";
        return false;
    }
    ri::scene::Scene candidateScene = *input.baselineScene;
    if (!LoadAuthoredSceneState(candidateScene, paths.authoredPath, error)) {
        return false;
    }
    transformResult = ri::scene::LoadSceneNodeTransformsDetailed(candidateScene, paths.transformPath);
    if (!transformResult.committed) {
        error = "transform component failed validation (error "
            + std::to_string(static_cast<int>(transformResult.error)) + ")";
        return false;
    }
    ri::scene::OrbitCameraState candidateOrbit = input.baselineOrbit;
    if (!LoadOrbitState(paths.orbitPath, candidateOrbit, error)) {
        return false;
    }
    EditorLogicLayer candidateLogic{};
    candidateLogic.EnsureKitLoaded(input.workspaceRoot);
    if (input.gameRoot.has_value()) {
        candidateLogic.EnsureGameColliderTrace(*input.gameRoot);
    }
    if (!candidateLogic.Load(paths.logicPath, candidateScene, input.worldRoot, &error)) {
        return false;
    }
    output.scene = std::move(candidateScene);
    output.orbit = candidateOrbit;
    output.logicLayer = std::move(candidateLogic);
    return true;
}

[[nodiscard]] bool LoadLegacyGeneration(const EditorSceneBundleSlotPaths& slot,
                                        const EditorSceneBundleLoadInput& input,
                                        EditorSceneBundleLoadedState& output,
                                        ri::scene::SceneStateIOResult& transformResult,
                                        std::string& error) {
    if (input.baselineScene == nullptr) {
        error = "baseline scene is missing";
        return false;
    }
    std::error_code ec;
    if (!fs::exists(slot.legacyTransformPath, ec) || ec) {
        error = "legacy transform state is missing";
        return false;
    }
    ri::scene::Scene candidateScene = *input.baselineScene;
    if (fs::exists(slot.legacyAuthoredPath, ec)) {
        if (ec || !LoadAuthoredSceneState(candidateScene, slot.legacyAuthoredPath, error)) {
            return false;
        }
    } else if (ec) {
        error = "legacy authored sidecar could not be inspected";
        return false;
    }
    transformResult = ri::scene::LoadSceneNodeTransformsDetailed(candidateScene, slot.legacyTransformPath);
    if (!transformResult.committed) {
        error = "legacy transform state failed validation (error "
            + std::to_string(static_cast<int>(transformResult.error)) + ")";
        return false;
    }

    ri::scene::OrbitCameraState candidateOrbit = input.baselineOrbit;
    if (fs::exists(slot.legacyOrbitPath, ec)) {
        if (ec || !LoadOrbitState(slot.legacyOrbitPath, candidateOrbit, error)) {
            return false;
        }
    } else if (ec) {
        error = "legacy orbit sidecar could not be inspected";
        return false;
    }

    EditorLogicLayer candidateLogic{};
    candidateLogic.EnsureKitLoaded(input.workspaceRoot);
    if (input.gameRoot.has_value()) {
        candidateLogic.EnsureGameColliderTrace(*input.gameRoot);
    }
    if (fs::exists(slot.legacyLogicPath, ec)) {
        if (ec || !candidateLogic.Load(slot.legacyLogicPath, candidateScene, input.worldRoot, &error)) {
            return false;
        }
    } else if (ec) {
        error = "legacy logic sidecar could not be inspected";
        return false;
    }

    output.scene = std::move(candidateScene);
    output.orbit = candidateOrbit;
    output.logicLayer = std::move(candidateLogic);
    return true;
}

[[nodiscard]] bool ShouldInjectFailure(const EditorSceneBundleSaveOptions& options,
                                       const EditorSceneBundleSaveStage stage) {
    return options.injectFailureAfterStage && options.injectFailureAfterStage(stage);
}

void AppendRecoveryPaths(const ri::scene::SceneStateIOResult& transform,
                         std::vector<fs::path>& paths) {
    if (!transform.retainedBackupPath.empty()) {
        paths.push_back(transform.retainedBackupPath);
    }
    if (!transform.retainedReplacementPath.empty()) {
        paths.push_back(transform.retainedReplacementPath);
    }
}

[[nodiscard]] bool WriteComponent(const fs::path& path,
                                  const std::string_view bytes,
                                  std::string& error) {
    const NewFileWriteResult written = WriteNewFileDurably(path, bytes);
    if (!written.succeeded) {
        error = "durable write failed for " + path.filename().string();
        if (written.error) {
            error += ": " + written.error.message();
        }
        return false;
    }
    return true;
}

[[nodiscard]] std::string SanitizeSnapshotToken(const std::string_view token) {
    std::string sanitized;
    sanitized.reserve(std::min<std::size_t>(token.size(), 64U));
    for (const unsigned char value : token) {
        if (sanitized.size() >= 64U) {
            break;
        }
        if ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z')
            || (value >= '0' && value <= '9') || value == '-' || value == '_') {
            sanitized.push_back(static_cast<char>(value));
        }
    }
    return sanitized.empty() ? std::string("snapshot") : sanitized;
}

} // namespace

const char* ToString(const EditorSceneBundleError error) noexcept {
    switch (error) {
    case EditorSceneBundleError::None: return "none";
    case EditorSceneBundleError::InvalidInput: return "invalid-input";
    case EditorSceneBundleError::SlotDirectoryCreationFailed: return "slot-directory-creation-failed";
    case EditorSceneBundleError::GenerationReservationFailed: return "generation-reservation-failed";
    case EditorSceneBundleError::SerializationFailed: return "serialization-failed";
    case EditorSceneBundleError::ComponentWriteFailed: return "component-write-failed";
    case EditorSceneBundleError::ComponentValidationFailed: return "component-validation-failed";
    case EditorSceneBundleError::ManifestWriteFailed: return "manifest-write-failed";
    case EditorSceneBundleError::ManifestPublishFailed: return "manifest-publish-failed";
    case EditorSceneBundleError::PublishedGenerationNotFound: return "published-generation-not-found";
    case EditorSceneBundleError::PublishedGenerationCorrupt: return "published-generation-corrupt";
    case EditorSceneBundleError::LegacyStateNotFound: return "legacy-state-not-found";
    case EditorSceneBundleError::LegacyStateInvalid: return "legacy-state-invalid";
    case EditorSceneBundleError::InjectedFailure: return "injected-failure";
    }
    return "unknown";
}

EditorSceneBundleSlotPaths ResolveCanonicalEditorSceneBundleSlot(const fs::path& canonicalTransformPath) {
    const fs::path directory = canonicalTransformPath.parent_path();
    return EditorSceneBundleSlotPaths{
        .kind = EditorSceneBundleSlotKind::Canonical,
        .slotRoot = directory / ".scene_bundles" / "canonical",
        .legacyTransformPath = canonicalTransformPath,
        .legacyAuthoredPath = directory / "authored_scene.ri_editor",
        .legacyOrbitPath = directory / "editor_orbit.ri_cam",
        .legacyLogicPath = directory / "logic_authoring.ri_logic",
        .label = "scene",
    };
}

EditorSceneBundleSlotPaths ResolveAutosaveEditorSceneBundleSlot(const fs::path& canonicalTransformPath) {
    const fs::path directory = canonicalTransformPath.parent_path();
    return EditorSceneBundleSlotPaths{
        .kind = EditorSceneBundleSlotKind::Autosave,
        .slotRoot = directory / ".scene_bundles" / "autosave",
        .legacyTransformPath = directory / "autosave_scene_state.ri_state",
        // Old Raw Iron autosaves accidentally shared these canonical sidecars.
        // Retain them only as a load fallback; new autosaves never write them.
        .legacyAuthoredPath = directory / "authored_scene.ri_editor",
        .legacyOrbitPath = directory / "autosave_editor_orbit.ri_cam",
        .legacyLogicPath = directory / "logic_authoring.ri_logic",
        .label = "autosave",
    };
}

EditorSceneBundleSlotPaths ResolveTimestampedEditorSceneBundleSlot(const fs::path& canonicalTransformPath,
                                                                  const std::string_view timestampToken) {
    const fs::path directory = canonicalTransformPath.parent_path();
    const std::string token = SanitizeSnapshotToken(timestampToken);
    return EditorSceneBundleSlotPaths{
        .kind = EditorSceneBundleSlotKind::TimestampedSnapshot,
        .slotRoot = directory / ".scene_bundles" / "snapshots" / token,
        .legacyTransformPath = directory / ("scene_state_snapshot_" + token + ".ri_state"),
        .legacyAuthoredPath = directory / ("authored_scene_snapshot_" + token + ".ri_editor"),
        .legacyOrbitPath = directory / ("editor_orbit_snapshot_" + token + ".ri_cam"),
        .legacyLogicPath = directory / ("logic_authoring_snapshot_" + token + ".ri_logic"),
        .label = "snapshot " + token,
    };
}

EditorSceneBundleSaveResult SaveEditorSceneBundle(const EditorSceneBundleSlotPaths& slot,
                                                  const EditorSceneBundleSaveInput& input,
                                                  const EditorSceneBundleSaveOptions& options) {
    EditorSceneBundleSaveResult result{};
    if (input.scene == nullptr || input.baselineScene == nullptr || input.logicLayer == nullptr
        || slot.slotRoot.empty() || input.worldRoot == ri::scene::kInvalidHandle) {
        result.error = EditorSceneBundleError::InvalidInput;
        result.diagnostic = "editor scene bundle save input is incomplete";
        return result;
    }

    std::string authoredBytes;
    if (!SerializeAuthoredSceneState(*input.scene,
                                     input.authoredNodeStart,
                                     input.editorTrashHandle,
                                     authoredBytes,
                                     result.diagnostic)) {
        result.error = EditorSceneBundleError::SerializationFailed;
        return result;
    }
    const std::string orbitBytes = SerializeOrbitState(input.orbit);
    const std::string logicBytes = input.logicLayer->Serialize(*input.scene);
    if (orbitBytes.size() > kMaxOrbitBytes || logicBytes.empty() || logicBytes.size() > kMaxLogicBytes) {
        result.error = EditorSceneBundleError::SerializationFailed;
        result.diagnostic = logicBytes.empty() ? "logic authoring serialization was empty"
                                               : "serialized editor sidecar exceeds its safety limit";
        return result;
    }

    if (!ReserveGeneration(slot, result.paths, result.diagnostic)) {
        result.error = EditorSceneBundleError::GenerationReservationFailed;
        return result;
    }

    result.transformResult = ri::scene::SaveSceneNodeTransformsDetailed(*input.scene, result.paths.transformPath);
    AppendRecoveryPaths(result.transformResult, result.retainedRecoveryPaths);
    if (!result.transformResult.committed) {
        result.error = EditorSceneBundleError::ComponentWriteFailed;
        result.diagnostic = "transform component was not committed (error "
            + std::to_string(static_cast<int>(result.transformResult.error)) + ")";
        return result;
    }
    if (!result.transformResult.Succeeded()) {
        result.durabilityWarning = true;
        result.diagnostic = "transform component committed with warning "
            + std::to_string(static_cast<int>(result.transformResult.error));
    }
    if (ShouldInjectFailure(options, EditorSceneBundleSaveStage::TransformCommitted)) {
        result.error = EditorSceneBundleError::InjectedFailure;
        result.diagnostic = "injected failure after transform commit";
        return result;
    }

    std::string componentError;
    if (!WriteComponent(result.paths.authoredPath, authoredBytes, componentError)) {
        result.error = EditorSceneBundleError::ComponentWriteFailed;
        result.diagnostic = std::move(componentError);
        return result;
    }
    if (ShouldInjectFailure(options, EditorSceneBundleSaveStage::AuthoredCommitted)) {
        result.error = EditorSceneBundleError::InjectedFailure;
        result.diagnostic = "injected failure after authored component commit";
        return result;
    }
    if (!WriteComponent(result.paths.orbitPath, orbitBytes, componentError)) {
        result.error = EditorSceneBundleError::ComponentWriteFailed;
        result.diagnostic = std::move(componentError);
        return result;
    }
    if (ShouldInjectFailure(options, EditorSceneBundleSaveStage::OrbitCommitted)) {
        result.error = EditorSceneBundleError::InjectedFailure;
        result.diagnostic = "injected failure after orbit component commit";
        return result;
    }
    if (!WriteComponent(result.paths.logicPath, logicBytes, componentError)) {
        result.error = EditorSceneBundleError::ComponentWriteFailed;
        result.diagnostic = std::move(componentError);
        return result;
    }
    if (ShouldInjectFailure(options, EditorSceneBundleSaveStage::LogicCommitted)) {
        result.error = EditorSceneBundleError::InjectedFailure;
        result.diagnostic = "injected failure after logic component commit";
        return result;
    }

    BundleManifest manifest{.generation = result.paths.generation};
    if (!FingerprintFile(result.paths.transformPath,
                         ri::scene::kMaxSceneStateFileBytes,
                         manifest.transform,
                         componentError)
        || !FingerprintFile(result.paths.authoredPath, kMaxAuthoredBytes, manifest.authored, componentError)
        || !FingerprintFile(result.paths.orbitPath, kMaxOrbitBytes, manifest.orbit, componentError)
        || !FingerprintFile(result.paths.logicPath, kMaxLogicBytes, manifest.logic, componentError)
        || manifest.authored.size != authoredBytes.size()
        || manifest.authored.hash != HashBytes(authoredBytes)
        || manifest.orbit.size != orbitBytes.size()
        || manifest.orbit.hash != HashBytes(orbitBytes)
        || manifest.logic.size != logicBytes.size()
        || manifest.logic.hash != HashBytes(logicBytes)) {
        result.error = EditorSceneBundleError::ComponentValidationFailed;
        result.diagnostic = componentError.empty() ? "component readback fingerprint mismatch" : componentError;
        return result;
    }

    EditorSceneBundleLoadedState validationState{};
    ri::scene::SceneStateIOResult validationTransform{};
    std::string validationError;
    if (!LoadCompleteGeneration(
            result.paths,
            EditorSceneBundleLoadInput{
                .baselineScene = input.baselineScene,
                .baselineOrbit = input.orbit,
                .workspaceRoot = input.workspaceRoot,
                .gameRoot = input.gameRoot,
                .worldRoot = input.worldRoot,
            },
            validationState,
            validationTransform,
            validationError)) {
        result.error = EditorSceneBundleError::ComponentValidationFailed;
        result.diagnostic = "bundle readback validation failed: " + validationError;
        return result;
    }
    if (ShouldInjectFailure(options, EditorSceneBundleSaveStage::ComponentsValidated)) {
        result.error = EditorSceneBundleError::InjectedFailure;
        result.diagnostic = "injected failure after component validation";
        return result;
    }

    const std::string manifestBytes = SerializeManifest(manifest);
    const fs::path temporaryManifest = result.paths.generationDirectory / kManifestTemporaryFilename;
    if (!WriteComponent(temporaryManifest, manifestBytes, componentError)) {
        result.error = EditorSceneBundleError::ManifestWriteFailed;
        result.diagnostic = std::move(componentError);
        return result;
    }
    if (ShouldInjectFailure(options, EditorSceneBundleSaveStage::ManifestPrepared)) {
        result.error = EditorSceneBundleError::InjectedFailure;
        result.diagnostic = "injected failure after durable manifest preparation";
        return result;
    }

    std::error_code publishError;
    if (!PublishManifestNoReplace(temporaryManifest, result.paths.manifestPath, publishError)) {
        result.error = EditorSceneBundleError::ManifestPublishFailed;
        result.diagnostic = "could not atomically publish bundle manifest";
        if (publishError) {
            result.diagnostic += ": " + publishError.message();
        }
        return result;
    }
    result.committed = true;
    if (publishError) {
        result.durabilityWarning = true;
        result.diagnostic = "bundle committed, but manifest temporary cleanup failed: " + publishError.message();
    }

    std::error_code generationSyncError;
    const bool generationSynced = SyncDirectory(result.paths.generationDirectory, generationSyncError);
    std::error_code slotSyncError;
    const bool slotSynced = SyncDirectory(slot.slotRoot, slotSyncError);
#if !defined(_WIN32)
    if (!generationSynced || !slotSynced) {
        result.durabilityWarning = true;
        const std::error_code syncError = generationSyncError ? generationSyncError : slotSyncError;
        result.diagnostic = "bundle committed, but parent-directory durability could not be confirmed";
        if (syncError) {
            result.diagnostic += ": " + syncError.message();
        }
    }
#else
    (void)generationSynced;
    (void)slotSynced;
#endif
    if (result.diagnostic.empty()) {
        result.diagnostic = "published " + slot.label + " generation " + std::to_string(result.paths.generation);
    }
    return result;
}

EditorSceneBundleLoadResult LoadEditorSceneBundle(const EditorSceneBundleSlotPaths& slot,
                                                  const EditorSceneBundleLoadInput& input,
                                                  EditorSceneBundleLoadedState& output) {
    EditorSceneBundleLoadResult result{};
    if (input.baselineScene == nullptr || input.worldRoot == ri::scene::kInvalidHandle) {
        result.error = EditorSceneBundleError::InvalidInput;
        result.diagnostic = "editor scene bundle load input is incomplete";
        return result;
    }

    const PublishedGenerationResolution published = ResolveLatestPublishedGeneration(slot);
    if (published.found) {
        EditorSceneBundleLoadedState candidate{};
        std::string loadError;
        if (!LoadCompleteGeneration(published.paths,
                                    input,
                                    candidate,
                                    result.transformResult,
                                    loadError)) {
            result.error = EditorSceneBundleError::PublishedGenerationCorrupt;
            result.paths = published.paths;
            result.diagnostic = "published generation failed semantic validation: " + loadError;
            return result;
        }
        output = std::move(candidate);
        result.loaded = true;
        result.paths = published.paths;
        result.recoveredPreviousGeneration = published.recoveredPrevious;
        result.diagnostic = published.diagnostic.empty()
            ? "loaded " + slot.label + " generation " + std::to_string(published.paths.generation)
            : published.diagnostic;
        return result;
    }
    if (published.sawPublishedButInvalid) {
        result.error = EditorSceneBundleError::PublishedGenerationCorrupt;
        result.diagnostic = published.diagnostic.empty()
            ? "no valid published generation remains"
            : published.diagnostic;
        return result;
    }

    std::error_code ec;
    if (!fs::exists(slot.legacyTransformPath, ec) || ec) {
        result.error = EditorSceneBundleError::LegacyStateNotFound;
        result.diagnostic = "no published or legacy " + slot.label + " state exists";
        return result;
    }
    EditorSceneBundleLoadedState legacyCandidate{};
    std::string legacyError;
    if (!LoadLegacyGeneration(slot, input, legacyCandidate, result.transformResult, legacyError)) {
        result.error = EditorSceneBundleError::LegacyStateInvalid;
        result.diagnostic = "legacy " + slot.label + " state rejected: " + legacyError;
        return result;
    }
    output = std::move(legacyCandidate);
    result.loaded = true;
    result.usedLegacyPaths = true;
    result.diagnostic = "loaded legacy fixed-name " + slot.label + " state";
    return result;
}

bool HasEditorSceneBundleState(const EditorSceneBundleSlotPaths& slot) {
    const PublishedGenerationResolution published = ResolveLatestPublishedGeneration(slot);
    if (published.found || published.sawPublishedButInvalid) {
        return true;
    }
    std::error_code ec;
    return fs::exists(slot.legacyTransformPath, ec)
        || fs::exists(slot.legacyAuthoredPath, ec)
        || fs::exists(slot.legacyOrbitPath, ec)
        || fs::exists(slot.legacyLogicPath, ec);
}

} // namespace ri::editor
