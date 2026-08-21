#include "RawIron/Scene/GltfExporter.h"

#include "RawIron/Math/Mat4.h"
#include "RawIron/Scene/Helpers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace ri::scene {
namespace {

struct Geometry {
    std::vector<ri::math::Vec3> positions{};
    std::vector<ri::math::Vec3> normals{};
    std::vector<ri::math::Vec2> texCoords{};
    std::vector<std::uint32_t> indices{};
};

struct BufferViewRecord { std::size_t offset = 0; std::size_t length = 0; int target = 0; };
struct AccessorRecord {
    int bufferView = -1;
    int componentType = 5126;
    std::size_t count = 0;
    std::string type{};
    std::optional<ri::math::Vec3> min3{};
    std::optional<ri::math::Vec3> max3{};
};
struct MeshRecord { std::string name{}; int position = -1; int normal = -1; int uv = -1; int indices = -1; int material = -1; };
struct MaterialTextureRecord {
    int baseColor = -1;
    int normal = -1;
    int orm = -1;
    int emissive = -1;
    int occlusion = -1;
};
struct TextureRecord { std::string uri{}; };
struct NodeRecord {
    std::string name{};
    ri::math::Mat4 matrix = ri::math::IdentityMatrix();
    int mesh = -1;
    int camera = -1;
    int light = -1;
    std::vector<int> children{};
};

std::string JsonEscape(const std::string_view text) {
    std::ostringstream out;
    for (const unsigned char ch : text) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20U) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch)
                    << std::dec << std::setfill(' ');
            } else {
                out << static_cast<char>(ch);
            }
        }
    }
    return out.str();
}

void AppendAligned(std::vector<std::uint8_t>& bytes, const void* source, const std::size_t size) {
    while ((bytes.size() & 3U) != 0U) bytes.push_back(0U);
    const auto* begin = static_cast<const std::uint8_t*>(source);
    bytes.insert(bytes.end(), begin, begin + size);
}

Geometry MakeCubeGeometry() {
    Geometry out{};
    constexpr std::array<ri::math::Vec3, 8> corners{{
        {-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f},
        {-0.5f,-0.5f,0.5f},{0.5f,-0.5f,0.5f},{0.5f,0.5f,0.5f},{-0.5f,0.5f,0.5f}}};
    constexpr std::array<std::array<int,4>,6> faces{{
        {{4,5,6,7}},{{1,0,3,2}},{{0,4,7,3}},{{5,1,2,6}},{{3,7,6,2}},{{0,1,5,4}}}};
    constexpr std::array<ri::math::Vec3,6> normals{{
        {0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}}};
    constexpr std::array<ri::math::Vec2,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
    for (std::size_t face = 0; face < faces.size(); ++face) {
        const std::uint32_t base = static_cast<std::uint32_t>(out.positions.size());
        for (int corner = 0; corner < 4; ++corner) {
            out.positions.push_back(corners[faces[face][corner]]);
            out.normals.push_back(normals[face]);
            out.texCoords.push_back(uv[corner]);
        }
        out.indices.insert(out.indices.end(), {base,base+1,base+2,base,base+2,base+3});
    }
    return out;
}

Geometry MakePlaneGeometry() {
    Geometry out{};
    out.positions = {{-0.5f,0,-0.5f},{0.5f,0,-0.5f},{0.5f,0,0.5f},{-0.5f,0,0.5f}};
    out.normals.assign(4, {0,1,0});
    out.texCoords = {{0,0},{1,0},{1,1},{0,1}};
    out.indices = {0,1,2,0,2,3};
    return out;
}

void GenerateNormals(Geometry& geometry) {
    if (geometry.positions.empty() || geometry.indices.empty()) return;
    geometry.normals.assign(geometry.positions.size(), {});
    for (std::size_t index = 0; index + 2 < geometry.indices.size(); index += 3) {
        const std::uint32_t ia = geometry.indices[index];
        const std::uint32_t ib = geometry.indices[index + 1];
        const std::uint32_t ic = geometry.indices[index + 2];
        if (ia >= geometry.positions.size() || ib >= geometry.positions.size() || ic >= geometry.positions.size()) continue;
        const ri::math::Vec3 normal = ri::math::Cross(
            geometry.positions[ib] - geometry.positions[ia],
            geometry.positions[ic] - geometry.positions[ia]);
        geometry.normals[ia] = geometry.normals[ia] + normal;
        geometry.normals[ib] = geometry.normals[ib] + normal;
        geometry.normals[ic] = geometry.normals[ic] + normal;
    }
    for (ri::math::Vec3& normal : geometry.normals) {
        normal = ri::math::LengthSquared(normal) > 1.0e-12f ? ri::math::Normalize(normal) : ri::math::Vec3{0,1,0};
    }
}

Geometry ResolveGeometry(const Mesh& mesh) {
    if (mesh.positions.empty()) {
        if (mesh.primitive == PrimitiveType::Cube) return MakeCubeGeometry();
        if (mesh.primitive == PrimitiveType::Plane) return MakePlaneGeometry();
        if (mesh.primitive == PrimitiveType::Sphere) {
            const Mesh sphere = MakeUvSphereMesh(mesh.name);
            Geometry out{sphere.positions, sphere.normals, sphere.texCoords, {}};
            out.indices.reserve(sphere.indices.size());
            for (const int index : sphere.indices) if (index >= 0) out.indices.push_back(static_cast<std::uint32_t>(index));
            if (out.normals.size() != out.positions.size()) GenerateNormals(out);
            return out;
        }
        return {};
    }
    Geometry out{mesh.positions, mesh.normals, mesh.texCoords, {}};
    if (mesh.geometryMode == MeshGeometryMode::CameraFacingSpriteQuads
        && mesh.billboardOffsets.size() == out.positions.size()) {
        // glTF has no core billboard primitive. Export a deterministic +Z-facing snapshot while
        // retaining valid surface normals instead of leaking Raw Iron's runtime vertex contract.
        for (std::size_t index = 0; index < out.positions.size(); ++index) {
            out.positions[index].x += mesh.billboardOffsets[index].x;
            out.positions[index].y += mesh.billboardOffsets[index].y;
        }
        out.normals.assign(out.positions.size(), ri::math::Vec3{0.0f, 0.0f, 1.0f});
    }
    if (!mesh.indices.empty()) {
        out.indices.reserve(mesh.indices.size());
        for (const int index : mesh.indices) if (index >= 0) out.indices.push_back(static_cast<std::uint32_t>(index));
    } else {
        out.indices.resize(out.positions.size());
        for (std::size_t index = 0; index < out.indices.size(); ++index) out.indices[index] = static_cast<std::uint32_t>(index);
    }
    if (out.normals.size() != out.positions.size()) GenerateNormals(out);
    if (out.texCoords.size() != out.positions.size()) out.texCoords.clear();
    return out;
}

void WriteMatrix(std::ostream& out, const ri::math::Mat4& matrix) {
    out << '[';
    bool first = true;
    for (int column = 0; column < 4; ++column) for (int row = 0; row < 4; ++row) {
        if (!first) out << ',';
        first = false;
        out << matrix.m[row][column];
    }
    out << ']';
}

} // namespace

bool ExportSceneToGltf(const Scene& scene,
                       const std::filesystem::path& requestedOutputPath,
                       const GltfExportOptions& options,
                       GltfExportReport& report,
                       std::string& error) {
    try {
        report = {};
        std::filesystem::path outputPath = requestedOutputPath;
        if (outputPath.extension() != ".gltf") outputPath.replace_extension(".gltf");
        if (!outputPath.parent_path().empty()) std::filesystem::create_directories(outputPath.parent_path());
        const std::filesystem::path binaryPath = outputPath.parent_path() / (outputPath.stem().string() + ".bin");

        std::vector<std::uint8_t> binary{};
        std::vector<BufferViewRecord> views{};
        std::vector<AccessorRecord> accessors{};
        std::vector<MeshRecord> meshes{};
        std::map<std::pair<int,int>, int> meshByHandles{};
        std::vector<TextureRecord> textures{};
        std::map<std::filesystem::path, int> textureBySource{};
        std::vector<MaterialTextureRecord> materialTextures(scene.MaterialCount());

        const auto stablePathHash = [](const std::string_view value) {
            std::uint64_t hash = 1469598103934665603ULL;
            for (const unsigned char byte : value) hash = (hash ^ byte) * 1099511628211ULL;
            return hash;
        };
        const auto registerTexture = [&](const std::string& authoredPath) {
            if (authoredPath.empty() || authoredPath.starts_with("data:")) return -1;
            const std::filesystem::path source = std::filesystem::path(authoredPath).lexically_normal();
            if (const auto found = textureBySource.find(source); found != textureBySource.end()) return found->second;
            if (!std::filesystem::is_regular_file(source)) {
                report.warnings.push_back("Skipped missing texture: " + source.string());
                return -1;
            }
            std::filesystem::path uri = source;
            if (options.copyExternalTextures) {
                const std::string hashText = std::to_string(stablePathHash(source.generic_string()));
                const std::filesystem::path relative = std::filesystem::path("textures")
                    / (source.stem().string() + "-" + hashText.substr(0, 10) + source.extension().string());
                const std::filesystem::path destination = outputPath.parent_path() / relative;
                std::filesystem::create_directories(destination.parent_path());
                std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
                uri = relative;
            } else {
                std::error_code relativeError;
                const std::filesystem::path relative = std::filesystem::relative(source, outputPath.parent_path(), relativeError);
                if (!relativeError) uri = relative;
            }
            const int handle = static_cast<int>(textures.size());
            textures.push_back({uri.generic_string()});
            textureBySource.emplace(source, handle);
            return handle;
        };
        for (std::size_t index = 0; index < scene.MaterialCount(); ++index) {
            const Material& material = scene.GetMaterial(static_cast<int>(index));
            MaterialTextureRecord& slots = materialTextures[index];
            slots.baseColor = registerTexture(material.baseColorTexture);
            slots.normal = registerTexture(material.normalTexture);
            slots.orm = registerTexture(material.ormTexture);
            slots.emissive = registerTexture(material.emissiveTexture);
            slots.occlusion = registerTexture(material.occlusionTexture);
            if (!material.opacityTexture.empty() && material.opacityTexture != material.baseColorTexture) {
                report.warnings.push_back("glTF core cannot represent separate opacity texture for material " + material.name + ".");
            }
            if (!material.detailTexture.empty()) {
                report.warnings.push_back("glTF core cannot represent Raw Iron detail texture for material " + material.name + ".");
            }
            if (material.baseColorTextureFrames.size() > 1U) {
                report.warnings.push_back("Exported only the authored base texture for animated material " + material.name + ".");
            }
        }

        const auto appendView = [&](const void* data, const std::size_t size, const int target) {
            while ((binary.size() & 3U) != 0U) binary.push_back(0U);
            const std::size_t offset = binary.size();
            AppendAligned(binary, data, size);
            views.push_back({offset, size, target});
            return static_cast<int>(views.size() - 1U);
        };
        const auto addMesh = [&](const int meshHandle, const int materialHandle) {
            const auto key = std::pair{meshHandle, materialHandle};
            if (const auto found = meshByHandles.find(key); found != meshByHandles.end()) return found->second;
            const Mesh& source = scene.GetMesh(meshHandle);
            Geometry geometry = ResolveGeometry(source);
            if (geometry.positions.empty() || geometry.indices.empty()) return -1;

            ri::math::Vec3 minPoint = geometry.positions.front();
            ri::math::Vec3 maxPoint = geometry.positions.front();
            for (const auto& point : geometry.positions) {
                minPoint = {std::min(minPoint.x,point.x),std::min(minPoint.y,point.y),std::min(minPoint.z,point.z)};
                maxPoint = {std::max(maxPoint.x,point.x),std::max(maxPoint.y,point.y),std::max(maxPoint.z,point.z)};
            }
            const int posView = appendView(geometry.positions.data(), geometry.positions.size()*sizeof(ri::math::Vec3), 34962);
            accessors.push_back({posView,5126,geometry.positions.size(),"VEC3",minPoint,maxPoint});
            const int posAccessor = static_cast<int>(accessors.size()-1U);
            int normalAccessor = -1;
            if (!geometry.normals.empty()) {
                const int view = appendView(geometry.normals.data(), geometry.normals.size()*sizeof(ri::math::Vec3), 34962);
                accessors.push_back({view,5126,geometry.normals.size(),"VEC3"});
                normalAccessor = static_cast<int>(accessors.size()-1U);
            }
            int uvAccessor = -1;
            if (!geometry.texCoords.empty()) {
                const int view = appendView(geometry.texCoords.data(), geometry.texCoords.size()*sizeof(ri::math::Vec2), 34962);
                accessors.push_back({view,5126,geometry.texCoords.size(),"VEC2"});
                uvAccessor = static_cast<int>(accessors.size()-1U);
            }
            const int indexView = appendView(geometry.indices.data(), geometry.indices.size()*sizeof(std::uint32_t), 34963);
            accessors.push_back({indexView,5125,geometry.indices.size(),"SCALAR"});
            const int indexAccessor = static_cast<int>(accessors.size()-1U);
            meshes.push_back({source.name,posAccessor,normalAccessor,uvAccessor,indexAccessor,materialHandle});
            const int result = static_cast<int>(meshes.size()-1U);
            meshByHandles.emplace(key,result);
            return result;
        };

        std::vector<NodeRecord> nodes(scene.NodeCount());
        std::vector<int> roots{};
        for (std::size_t index = 0; index < scene.NodeCount(); ++index) {
            const Node& source = scene.GetNode(static_cast<int>(index));
            NodeRecord& target = nodes[index];
            target.name = source.name;
            target.matrix = source.localTransform.LocalMatrix();
            target.children = source.children;
            if (source.mesh >= 0 && static_cast<std::size_t>(source.mesh) < scene.MeshCount()) {
                target.mesh = addMesh(source.mesh, source.material);
            }
            if (options.includeCameras && source.camera >= 0 && static_cast<std::size_t>(source.camera) < scene.CameraCount()) {
                target.camera = source.camera;
            }
            if (options.includeLights && source.light >= 0 && static_cast<std::size_t>(source.light) < scene.LightCount()) {
                target.light = source.light;
            }
            if (source.parent == kInvalidHandle) roots.push_back(static_cast<int>(index));
        }

        if (options.includeInstanceBatches) {
            for (std::size_t batchIndex = 0; batchIndex < scene.MeshInstanceBatchCount(); ++batchIndex) {
                const MeshInstanceBatch& batch = scene.GetMeshInstanceBatch(static_cast<int>(batchIndex));
                if (batch.mesh < 0 || static_cast<std::size_t>(batch.mesh) >= scene.MeshCount()) continue;
                const int gltfMesh = addMesh(batch.mesh, batch.material);
                if (gltfMesh < 0) continue;
                for (std::size_t instance = 0; instance < batch.transforms.size(); ++instance) {
                    const int nodeIndex = static_cast<int>(nodes.size());
                    nodes.push_back({batch.name + "_" + std::to_string(instance), batch.transforms[instance].LocalMatrix(), gltfMesh});
                    if (batch.parent >= 0 && static_cast<std::size_t>(batch.parent) < scene.NodeCount()) {
                        nodes[batch.parent].children.push_back(nodeIndex);
                    } else {
                        roots.push_back(nodeIndex);
                    }
                    ++report.instanceCount;
                }
            }
        }

        std::ofstream bin(binaryPath, std::ios::binary | std::ios::trunc);
        if (!bin || (!binary.empty() && !bin.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size())))) {
            throw std::runtime_error("Could not write glTF binary buffer: " + binaryPath.string());
        }

        std::ofstream json(outputPath, std::ios::trunc);
        if (!json) throw std::runtime_error("Could not write glTF JSON: " + outputPath.string());
        json << std::setprecision(9);
        json << "{\n\"asset\":{\"version\":\"2.0\",\"generator\":\"Raw Iron native C++ glTF exporter\"},\n";
        if (options.includeLights && scene.LightCount() > 0) json << "\"extensionsUsed\":[\"KHR_lights_punctual\"],\n";
        json << "\"scene\":0,\n\"scenes\":[{\"name\":\"" << JsonEscape(scene.GetName()) << "\",\"nodes\":[";
        for (std::size_t i=0;i<roots.size();++i){if(i)json<<',';json<<roots[i];} json << "]}],\n";

        json << "\"nodes\":[";
        for (std::size_t i=0;i<nodes.size();++i) {
            if(i)json<<','; const NodeRecord& node=nodes[i];
            json << "{\"name\":\""<<JsonEscape(node.name)<<"\",\"matrix\":"; WriteMatrix(json,node.matrix);
            if(node.mesh>=0)json<<",\"mesh\":"<<node.mesh;
            if(node.camera>=0)json<<",\"camera\":"<<node.camera;
            if(node.light>=0)json<<",\"extensions\":{\"KHR_lights_punctual\":{\"light\":"<<node.light<<"}}";
            if(!node.children.empty()){json<<",\"children\":[";for(std::size_t c=0;c<node.children.size();++c){if(c)json<<',';json<<node.children[c];}json<<']';}
            json << '}';
        }
        json << "],\n";

        json << "\"materials\":[";
        for(std::size_t i=0;i<scene.MaterialCount();++i){if(i)json<<',';const Material& m=scene.GetMaterial(static_cast<int>(i));const MaterialTextureRecord& t=materialTextures[i];
            json<<"{\"name\":\""<<JsonEscape(m.name)<<"\",\"pbrMetallicRoughness\":{\"baseColorFactor\":["<<m.baseColor.x<<','<<m.baseColor.y<<','<<m.baseColor.z<<','<<m.opacity<<"],\"metallicFactor\":"<<m.metallic<<",\"roughnessFactor\":"<<m.roughness;if(t.baseColor>=0)json<<",\"baseColorTexture\":{\"index\":"<<t.baseColor<<'}';if(t.orm>=0)json<<",\"metallicRoughnessTexture\":{\"index\":"<<t.orm<<'}';json<<'}';
            if(m.emissiveColor.x!=0||m.emissiveColor.y!=0||m.emissiveColor.z!=0)json<<",\"emissiveFactor\":["<<m.emissiveColor.x<<','<<m.emissiveColor.y<<','<<m.emissiveColor.z<<']';
            if(t.normal>=0)json<<",\"normalTexture\":{\"index\":"<<t.normal<<",\"scale\":"<<std::max(std::abs(m.normalScale.x),std::abs(m.normalScale.y))<<'}';
            if(t.emissive>=0)json<<",\"emissiveTexture\":{\"index\":"<<t.emissive<<'}';
            if(t.occlusion>=0)json<<",\"occlusionTexture\":{\"index\":"<<t.occlusion<<'}';else if(t.orm>=0)json<<",\"occlusionTexture\":{\"index\":"<<t.orm<<'}';
            if(m.transparent||m.opacity<0.999f)json<<",\"alphaMode\":\"BLEND\"";else if(m.alphaCutoff<0.999f)json<<",\"alphaMode\":\"MASK\",\"alphaCutoff\":"<<m.alphaCutoff;
            if(m.doubleSided)json<<",\"doubleSided\":true"; json<<'}';}
        json << "],\n";

        if(!textures.empty()){json<<"\"images\":[";for(std::size_t i=0;i<textures.size();++i){if(i)json<<',';json<<"{\"uri\":\""<<JsonEscape(textures[i].uri)<<"\"}";}json<<"],\n\"textures\":[";for(std::size_t i=0;i<textures.size();++i){if(i)json<<',';json<<"{\"source\":"<<i<<'}';}json<<"],\n";}

        json << "\"meshes\":[";
        for(std::size_t i=0;i<meshes.size();++i){if(i)json<<',';const MeshRecord& m=meshes[i];json<<"{\"name\":\""<<JsonEscape(m.name)<<"\",\"primitives\":[{\"attributes\":{\"POSITION\":"<<m.position;if(m.normal>=0)json<<",\"NORMAL\":"<<m.normal;if(m.uv>=0)json<<",\"TEXCOORD_0\":"<<m.uv;json<<"},\"indices\":"<<m.indices;if(m.material>=0)json<<",\"material\":"<<m.material;json<<"}]}";} json<<"],\n";

        if(options.includeCameras&&scene.CameraCount()>0){json<<"\"cameras\":[";for(std::size_t i=0;i<scene.CameraCount();++i){if(i)json<<',';const Camera& c=scene.GetCamera(static_cast<int>(i));json<<"{\"name\":\""<<JsonEscape(c.name)<<"\",\"type\":\"perspective\",\"perspective\":{\"yfov\":"<<ri::math::DegreesToRadians(c.fieldOfViewDegrees)<<",\"znear\":"<<c.nearClip<<",\"zfar\":"<<c.farClip<<"}}";}json<<"],\n";}
        if(options.includeLights&&scene.LightCount()>0){json<<"\"extensions\":{\"KHR_lights_punctual\":{\"lights\":[";for(std::size_t i=0;i<scene.LightCount();++i){if(i)json<<',';const Light& l=scene.GetLight(static_cast<int>(i));const char* type=l.type==LightType::Directional?"directional":l.type==LightType::Spot?"spot":"point";json<<"{\"name\":\""<<JsonEscape(l.name)<<"\",\"type\":\""<<type<<"\",\"color\":["<<l.color.x<<','<<l.color.y<<','<<l.color.z<<"],\"intensity\":"<<l.intensity;if(l.type!=LightType::Directional&&l.range>0)json<<",\"range\":"<<l.range;if(l.type==LightType::Spot)json<<",\"spot\":{\"outerConeAngle\":"<<ri::math::DegreesToRadians(l.spotAngleDegrees*0.5f)<<'}';json<<'}';}json<<"]}},\n";}

        json << "\"bufferViews\":[";for(std::size_t i=0;i<views.size();++i){if(i)json<<',';json<<"{\"buffer\":0,\"byteOffset\":"<<views[i].offset<<",\"byteLength\":"<<views[i].length<<",\"target\":"<<views[i].target<<'}';}json<<"],\n";
        json << "\"accessors\":[";for(std::size_t i=0;i<accessors.size();++i){if(i)json<<',';const auto& a=accessors[i];json<<"{\"bufferView\":"<<a.bufferView<<",\"componentType\":"<<a.componentType<<",\"count\":"<<a.count<<",\"type\":\""<<a.type<<'"';if(a.min3)json<<",\"min\":["<<a.min3->x<<','<<a.min3->y<<','<<a.min3->z<<']';if(a.max3)json<<",\"max\":["<<a.max3->x<<','<<a.max3->y<<','<<a.max3->z<<']';json<<'}';}json<<"],\n";
        json << "\"buffers\":[{\"uri\":\""<<JsonEscape(binaryPath.filename().generic_string())<<"\",\"byteLength\":"<<binary.size()<<"}]\n}\n";
        if(!json)throw std::runtime_error("Failed while writing glTF JSON: "+outputPath.string());

        report.nodeCount=nodes.size();report.meshCount=meshes.size();report.materialCount=scene.MaterialCount();
        report.cameraCount=options.includeCameras?scene.CameraCount():0;report.lightCount=options.includeLights?scene.LightCount():0;report.textureCount=textures.size();
        report.jsonPath=outputPath;report.binaryPath=binaryPath;error.clear();return true;
    } catch(const std::exception& ex){error=ex.what();return false;}
}

} // namespace ri::scene
