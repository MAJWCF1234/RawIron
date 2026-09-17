#include "RawIron/Scene/NativeSculpt.h"

#include "RawIron/Math/Mat4.h"
#include "RawIron/Scene/Helpers.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ri::scene {
namespace {

constexpr float kPi = 3.14159265358979323846f;

[[nodiscard]] std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

[[nodiscard]] std::string Slugify(std::string value, const std::string_view fallback) {
    std::string slug{};
    bool pending = false;
    for (const unsigned char raw : value) {
        if ((raw >= 'a' && raw <= 'z') || (raw >= '0' && raw <= '9')) {
            if (pending && !slug.empty()) {
                slug.push_back('_');
            }
            pending = false;
            slug.push_back(static_cast<char>(raw));
        } else if (raw >= 'A' && raw <= 'Z') {
            if (pending && !slug.empty()) {
                slug.push_back('_');
            }
            pending = false;
            slug.push_back(static_cast<char>(raw - 'A' + 'a'));
        } else {
            pending = true;
        }
    }
    return slug.empty() ? std::string(fallback) : slug;
}

Mesh MakeSphereCage(const int segmentsAround, const int segmentsDown, std::string name) {
    const int around = std::clamp(segmentsAround, 8, 96);
    const int down = std::clamp(segmentsDown, 4, 48);
    Mesh mesh{};
    mesh.name = std::move(name);
    mesh.primitive = PrimitiveType::Custom;
    const int stride = around + 1;
    mesh.positions.reserve(static_cast<std::size_t>(stride * (down + 1)));
    mesh.texCoords.reserve(mesh.positions.capacity());
    mesh.indices.reserve(static_cast<std::size_t>(around * down * 6));
    for (int y = 0; y <= down; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(down);
        const float theta = v * kPi;
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);
        for (int x = 0; x <= around; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(around);
            const float phi = u * (kPi * 2.0f);
            const ri::math::Vec3 position{
                sinTheta * std::cos(phi) * 0.5f,
                cosTheta * 0.5f,
                sinTheta * std::sin(phi) * 0.5f,
            };
            mesh.positions.push_back(position);
            mesh.texCoords.push_back(ri::math::Vec2{u, v});
        }
    }
    for (int y = 0; y < down; ++y) {
        for (int x = 0; x < around; ++x) {
            const int a = y * stride + x;
            const int b = a + 1;
            const int c = a + stride;
            const int d = c + 1;
            mesh.indices.insert(mesh.indices.end(), {a, c, b, b, c, d});
        }
    }
    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    RecalculateSculptNormals(mesh);
    return mesh;
}

Mesh MakeCubeCage(const int segmentsAround, std::string name) {
    const int segments = std::clamp(segmentsAround, 4, 32);
    Mesh mesh{};
    mesh.name = std::move(name);
    mesh.primitive = PrimitiveType::Custom;
    const auto addFace = [&](const ri::math::Vec3 origin,
                             const ri::math::Vec3 axisU,
                             const ri::math::Vec3 axisV,
                             const ri::math::Vec2 uvOrigin,
                             const ri::math::Vec2 uvSize) {
        const int base = static_cast<int>(mesh.positions.size());
        for (int y = 0; y <= segments; ++y) {
            const float v = static_cast<float>(y) / static_cast<float>(segments);
            for (int x = 0; x <= segments; ++x) {
                const float u = static_cast<float>(x) / static_cast<float>(segments);
                mesh.positions.push_back(origin + axisU * (u - 0.5f) + axisV * (v - 0.5f));
                mesh.texCoords.push_back(ri::math::Vec2{
                    uvOrigin.x + uvSize.x * u,
                    uvOrigin.y + uvSize.y * v,
                });
            }
        }
        const int stride = segments + 1;
        for (int y = 0; y < segments; ++y) {
            for (int x = 0; x < segments; ++x) {
                const int a = base + y * stride + x;
                const int b = a + 1;
                const int c = a + stride;
                const int d = c + 1;
                mesh.indices.insert(mesh.indices.end(), {a, c, b, b, c, d});
            }
        }
    };
    addFace({0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f});
    addFace({0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f});
    addFace({0.0f, 0.0f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f});
    addFace({0.0f, 0.0f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f});
    addFace({0.5f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f});
    addFace({-0.5f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f});
    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    RecalculateSculptNormals(mesh);
    return mesh;
}

[[nodiscard]] float BrushFalloff(const float distance, const float radius) {
    if (radius <= 0.0001f || distance >= radius) {
        return 0.0f;
    }
    const float t = 1.0f - (distance / radius);
    return t * t * (3.0f - 2.0f * t);
}

bool ApplyNativeSculptStrokeOnce(Mesh& mesh, const NativeSculptStroke& stroke) {
    if (mesh.positions.empty() || stroke.radius <= 0.0001f || !std::isfinite(stroke.strength)) {
        return false;
    }
    if (mesh.normals.size() != mesh.positions.size()) {
        RecalculateSculptNormals(mesh);
    }
    const float signedStrength = stroke.invert ? -stroke.strength : stroke.strength;
    const ri::math::Vec3 clayDirection = ri::math::LengthSquared(stroke.worldNormal) > 0.000001f
        ? ri::math::Normalize(stroke.worldNormal)
        : ri::math::Vec3{0.0f, 1.0f, 0.0f};
    std::vector<ri::math::Vec3> smoothed;
    if (stroke.brush == NativeSculptBrush::Smooth) {
        smoothed = mesh.positions;
        std::vector<ri::math::Vec3> accum(mesh.positions.size(), ri::math::Vec3{});
        std::vector<int> counts(mesh.positions.size(), 0);
        const std::size_t triangleCount = mesh.indices.size() / 3U;
        for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
            const int ia = mesh.indices[triangle * 3U];
            const int ib = mesh.indices[triangle * 3U + 1U];
            const int ic = mesh.indices[triangle * 3U + 2U];
            if (ia < 0 || ib < 0 || ic < 0
                || ia >= static_cast<int>(mesh.positions.size())
                || ib >= static_cast<int>(mesh.positions.size())
                || ic >= static_cast<int>(mesh.positions.size())) {
                continue;
            }
            const auto add = [&](const int from, const int to) {
                accum[static_cast<std::size_t>(from)] =
                    accum[static_cast<std::size_t>(from)] + mesh.positions[static_cast<std::size_t>(to)];
                counts[static_cast<std::size_t>(from)] += 1;
            };
            add(ia, ib);
            add(ia, ic);
            add(ib, ia);
            add(ib, ic);
            add(ic, ia);
            add(ic, ib);
        }
        for (std::size_t index = 0; index < mesh.positions.size(); ++index) {
            if (counts[index] > 0) {
                smoothed[index] = accum[index] * (1.0f / static_cast<float>(counts[index]));
            }
        }
    }

    bool changed = false;
    for (std::size_t index = 0; index < mesh.positions.size(); ++index) {
        const float distance = ri::math::Distance(mesh.positions[index], stroke.worldPosition);
        const float weight = BrushFalloff(distance, stroke.radius);
        if (weight <= 0.0f) {
            continue;
        }
        changed = true;
        if (stroke.brush == NativeSculptBrush::Smooth) {
            const ri::math::Vec3 delta = smoothed[index] - mesh.positions[index];
            mesh.positions[index] = mesh.positions[index] + delta * std::clamp(std::abs(signedStrength) * 4.0f * weight, 0.0f, 1.0f);
        } else if (stroke.brush == NativeSculptBrush::Flatten) {
            const float planeDistance = ri::math::Dot(mesh.positions[index] - stroke.worldPosition, clayDirection);
            mesh.positions[index] = mesh.positions[index]
                - clayDirection * (planeDistance * std::clamp(std::abs(signedStrength) * 4.0f * weight, 0.0f, 1.0f));
        } else {
            const ri::math::Vec3 direction = stroke.brush == NativeSculptBrush::Inflate
                ? mesh.normals[index]
                : clayDirection;
            mesh.positions[index] = mesh.positions[index] + direction * (signedStrength * weight);
        }
    }
    if (changed) {
        RecalculateSculptNormals(mesh);
        mesh.vertexCount = static_cast<int>(mesh.positions.size());
        mesh.indexCount = static_cast<int>(mesh.indices.size());
    }
    return changed;
}

void AppendEdgeBox(Mesh& mesh, const ri::math::Vec3& a, const ri::math::Vec3& b, const float thickness) {
    const ri::math::Vec3 delta = b - a;
    const float length = ri::math::Length(delta);
    if (length <= 0.0001f || thickness <= 0.0001f) {
        return;
    }
    const ri::math::Vec3 along = delta * (1.0f / length);
    ri::math::Vec3 side = ri::math::Cross(along, ri::math::Vec3{0.0f, 1.0f, 0.0f});
    if (ri::math::LengthSquared(side) < 0.0001f) {
        side = ri::math::Cross(along, ri::math::Vec3{1.0f, 0.0f, 0.0f});
    }
    const float half = thickness * 0.5f;
    side = ri::math::Normalize(side) * half;
    const ri::math::Vec3 up = ri::math::Normalize(ri::math::Cross(along, ri::math::Normalize(side))) * half;
    const int base = static_cast<int>(mesh.positions.size());
    const ri::math::Vec3 corners[8] = {
        a - side - up,
        a + side - up,
        a + side + up,
        a - side + up,
        b - side - up,
        b + side - up,
        b + side + up,
        b - side + up,
    };
    for (const ri::math::Vec3& corner : corners) {
        mesh.positions.push_back(corner);
        mesh.texCoords.push_back(ri::math::Vec2{});
    }
    const int faces[6][4] = {
        {0, 1, 2, 3},
        {4, 7, 6, 5},
        {0, 4, 5, 1},
        {1, 5, 6, 2},
        {2, 6, 7, 3},
        {3, 7, 4, 0},
    };
    for (const auto& face : faces) {
        const int ia = base + face[0];
        const int ib = base + face[1];
        const int ic = base + face[2];
        const int id = base + face[3];
        mesh.indices.insert(mesh.indices.end(), {ia, ib, ic, ia, ic, id});
    }
}

[[nodiscard]] ri::math::Vec3 ClosestPointOnTriangle(
    const ri::math::Vec3& point,
    const ri::math::Vec3& a,
    const ri::math::Vec3& b,
    const ri::math::Vec3& c) {
    const ri::math::Vec3 ab = b - a;
    const ri::math::Vec3 ac = c - a;
    const ri::math::Vec3 ap = point - a;
    const float d1 = ri::math::Dot(ab, ap);
    const float d2 = ri::math::Dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        return a;
    }
    const ri::math::Vec3 bp = point - b;
    const float d3 = ri::math::Dot(ab, bp);
    const float d4 = ri::math::Dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) {
        return b;
    }
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        const float v = d1 / (d1 - d3);
        return a + ab * v;
    }
    const ri::math::Vec3 cp = point - c;
    const float d5 = ri::math::Dot(ab, cp);
    const float d6 = ri::math::Dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) {
        return c;
    }
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        const float w = d2 / (d2 - d6);
        return a + ac * w;
    }
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + (c - b) * w;
    }
    const float denom = va + vb + vc;
    if (std::abs(denom) <= 0.0000001f) {
        return a;
    }
    const float inv = 1.0f / denom;
    return a + ab * (vb * inv) + ac * (vc * inv);
}

[[nodiscard]] ri::math::Vec3 ClosestPointOnMesh(const Mesh& mesh, const ri::math::Vec3& point) {
    ri::math::Vec3 closest = mesh.positions.empty() ? point : mesh.positions.front();
    float best = ri::math::DistanceSquared(point, closest);
    const std::size_t triangleCount = mesh.indices.size() / 3U;
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
        const int ia = mesh.indices[triangle * 3U];
        const int ib = mesh.indices[triangle * 3U + 1U];
        const int ic = mesh.indices[triangle * 3U + 2U];
        if (ia < 0 || ib < 0 || ic < 0
            || ia >= static_cast<int>(mesh.positions.size())
            || ib >= static_cast<int>(mesh.positions.size())
            || ic >= static_cast<int>(mesh.positions.size())) {
            continue;
        }
        const ri::math::Vec3 candidate = ClosestPointOnTriangle(
            point,
            mesh.positions[static_cast<std::size_t>(ia)],
            mesh.positions[static_cast<std::size_t>(ib)],
            mesh.positions[static_cast<std::size_t>(ic)]);
        const float distance = ri::math::DistanceSquared(point, candidate);
        if (distance < best) {
            best = distance;
            closest = candidate;
        }
    }
    return closest;
}

template <typename Fn>
void ForEachMirroredStroke(const NativeSculptStroke& stroke, Fn&& fn) {
    NativeSculptStroke local = stroke;
    local.mirrorX = false;
    local.mirrorY = false;
    local.mirrorZ = false;
    const int xLimit = stroke.mirrorX ? 1 : 0;
    const int yLimit = stroke.mirrorY ? 1 : 0;
    const int zLimit = stroke.mirrorZ ? 1 : 0;
    for (int flipX = 0; flipX <= xLimit; ++flipX) {
        if (flipX == 1 && std::abs(stroke.worldPosition.x) <= 0.0001f) {
            continue;
        }
        for (int flipY = 0; flipY <= yLimit; ++flipY) {
            if (flipY == 1 && std::abs(stroke.worldPosition.y) <= 0.0001f) {
                continue;
            }
            for (int flipZ = 0; flipZ <= zLimit; ++flipZ) {
                if (flipZ == 1 && std::abs(stroke.worldPosition.z) <= 0.0001f) {
                    continue;
                }
                local.worldPosition = stroke.worldPosition;
                local.worldNormal = stroke.worldNormal;
                if (flipX == 1) {
                    local.worldPosition.x = -local.worldPosition.x;
                    local.worldNormal.x = -local.worldNormal.x;
                }
                if (flipY == 1) {
                    local.worldPosition.y = -local.worldPosition.y;
                    local.worldNormal.y = -local.worldNormal.y;
                }
                if (flipZ == 1) {
                    local.worldPosition.z = -local.worldPosition.z;
                    local.worldNormal.z = -local.worldNormal.z;
                }
                fn(local);
            }
        }
    }
}

int InstantiateUnlitOverlay(
    Scene& scene,
    const int parent,
    std::string nodeName,
    const ri::math::Vec3& color,
    Mesh mesh) {
    const int material = scene.AddMaterial(Material{
        .name = nodeName + "Material",
        .shadingModel = ShadingModel::Unlit,
        .baseColor = color,
        .metallic = 0.0f,
        .roughness = 1.0f,
    });
    const int meshHandle = scene.AddMesh(std::move(mesh));
    const int node = scene.CreateNode(std::move(nodeName), parent);
    scene.AttachMesh(node, meshHandle, material);
    return node;
}

void ReplaceOverlayMesh(Scene& scene, const int nodeHandle, Mesh mesh) {
    if (nodeHandle == kInvalidHandle) {
        return;
    }
    Node& node = scene.GetNode(nodeHandle);
    if (node.mesh == kInvalidHandle) {
        return;
    }
    scene.GetMesh(node.mesh) = std::move(mesh);
}

} // namespace

NativeSculptCage ParseNativeSculptCage(const std::string_view value) {
    return LowerAscii(std::string(value)) == "cube" ? NativeSculptCage::Cube : NativeSculptCage::Sphere;
}

std::string_view NativeSculptCageName(const NativeSculptCage cage) noexcept {
    return cage == NativeSculptCage::Cube ? "cube" : "sphere";
}

std::string_view NativeSculptBrushName(const NativeSculptBrush brush) noexcept {
    switch (brush) {
        case NativeSculptBrush::Smooth:
            return "smooth";
        case NativeSculptBrush::Inflate:
            return "inflate";
        case NativeSculptBrush::Flatten:
            return "flatten";
        case NativeSculptBrush::Extrude:
            return "extrude";
        case NativeSculptBrush::Clay:
            return "clay";
    }
    return "clay";
}

Mesh MakeNativeSculptCageMesh(
    const NativeSculptCage cage,
    const int segmentsAround,
    const int segmentsDown,
    std::string name) {
    if (cage == NativeSculptCage::Cube) {
        return MakeCubeCage(segmentsAround, std::move(name));
    }
    return MakeSphereCage(segmentsAround, segmentsDown, std::move(name));
}

void RecalculateSculptNormals(Mesh& mesh) {
    mesh.normals.assign(mesh.positions.size(), ri::math::Vec3{});
    const std::size_t triangleCount = mesh.indices.size() / 3U;
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
        const int ia = mesh.indices[triangle * 3U];
        const int ib = mesh.indices[triangle * 3U + 1U];
        const int ic = mesh.indices[triangle * 3U + 2U];
        if (ia < 0 || ib < 0 || ic < 0
            || ia >= static_cast<int>(mesh.positions.size())
            || ib >= static_cast<int>(mesh.positions.size())
            || ic >= static_cast<int>(mesh.positions.size())) {
            continue;
        }
        const ri::math::Vec3& a = mesh.positions[static_cast<std::size_t>(ia)];
        const ri::math::Vec3& b = mesh.positions[static_cast<std::size_t>(ib)];
        const ri::math::Vec3& c = mesh.positions[static_cast<std::size_t>(ic)];
        const ri::math::Vec3 face = ri::math::Cross(b - a, c - a);
        mesh.normals[static_cast<std::size_t>(ia)] = mesh.normals[static_cast<std::size_t>(ia)] + face;
        mesh.normals[static_cast<std::size_t>(ib)] = mesh.normals[static_cast<std::size_t>(ib)] + face;
        mesh.normals[static_cast<std::size_t>(ic)] = mesh.normals[static_cast<std::size_t>(ic)] + face;
    }
    for (ri::math::Vec3& normal : mesh.normals) {
        const float length = ri::math::Length(normal);
        normal = length > 0.000001f ? normal * (1.0f / length) : ri::math::Vec3{0.0f, 1.0f, 0.0f};
    }
}

bool ApplyNativeSculptStroke(Mesh& mesh, const NativeSculptStroke& stroke) {
    bool changed = false;
    ForEachMirroredStroke(stroke, [&](const NativeSculptStroke& local) {
        changed = ApplyNativeSculptStrokeOnce(mesh, local) || changed;
    });
    return changed;
}

bool ApplyNativeSculptFaceExtrude(
    Mesh& mesh,
    const NativeSculptStroke& stroke,
    std::vector<std::string>* vertexBoneNames,
    std::vector<std::vector<ri::content::NativeSculptVertexInfluence>>* vertexInfluences) {
    if (stroke.mirrorX || stroke.mirrorY || stroke.mirrorZ) {
        bool changed = false;
        ForEachMirroredStroke(stroke, [&](const NativeSculptStroke& local) {
            changed = ApplyNativeSculptFaceExtrude(mesh, local, vertexBoneNames, vertexInfluences) || changed;
        });
        return changed;
    }
    if (mesh.positions.empty() || mesh.indices.size() < 3U || stroke.radius <= 0.0001f) {
        return false;
    }
    Mesh next = mesh;
    if (next.normals.size() != next.positions.size()) {
        RecalculateSculptNormals(next);
    }
    const std::size_t sourceCount = next.positions.size();
    const bool inheritWeights = vertexBoneNames != nullptr && vertexBoneNames->size() == sourceCount;
    std::vector<std::string> nextNames = inheritWeights ? *vertexBoneNames : std::vector<std::string>{};
    const bool inheritInfluences =
        vertexInfluences != nullptr && vertexInfluences->size() == sourceCount;
    std::vector<std::vector<ri::content::NativeSculptVertexInfluence>> nextInfluences =
        inheritInfluences ? *vertexInfluences
                          : std::vector<std::vector<ri::content::NativeSculptVertexInfluence>>{};
    const float distance = (stroke.invert ? -1.0f : 1.0f) * std::max(stroke.strength, 0.04f) * 1.35f;
    const ri::math::Vec3 hitNormal = ri::math::LengthSquared(stroke.worldNormal) > 0.000001f
        ? ri::math::Normalize(stroke.worldNormal)
        : ri::math::Vec3{0.0f, 1.0f, 0.0f};
    const std::size_t triangleCount = next.indices.size() / 3U;
    std::vector<char> selected(triangleCount, 0);
    std::size_t selectedCount = 0;
    const auto considerHit = [&](const ri::math::Vec3& position, const ri::math::Vec3& normal) {
        for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
            const int ia = next.indices[triangle * 3U];
            const int ib = next.indices[triangle * 3U + 1U];
            const int ic = next.indices[triangle * 3U + 2U];
            if (ia < 0 || ib < 0 || ic < 0
                || ia >= static_cast<int>(next.positions.size())
                || ib >= static_cast<int>(next.positions.size())
                || ic >= static_cast<int>(next.positions.size())) {
                continue;
            }
            const ri::math::Vec3& a = next.positions[static_cast<std::size_t>(ia)];
            const ri::math::Vec3& b = next.positions[static_cast<std::size_t>(ib)];
            const ri::math::Vec3& c = next.positions[static_cast<std::size_t>(ic)];
            const ri::math::Vec3 centroid = (a + b + c) * (1.0f / 3.0f);
            if (ri::math::Distance(centroid, position) > stroke.radius) {
                continue;
            }
            const ri::math::Vec3 face = ri::math::Cross(b - a, c - a);
            const float facing = ri::math::LengthSquared(face) <= 0.0000001f
                ? 0.0f
                : std::abs(ri::math::Dot(ri::math::Normalize(face), normal));
            if (facing < 0.18f) {
                continue;
            }
            if (selected[triangle] == 0) {
                selected[triangle] = 1;
                ++selectedCount;
            }
        }
    };
    considerHit(stroke.worldPosition, stroke.worldNormal);
    if (selectedCount == 0U) {
        return false;
    }
    if (selectedCount == triangleCount) {
        for (ri::math::Vec3& position : next.positions) {
            position = position + hitNormal * distance;
        }
        RecalculateSculptNormals(next);
        mesh = std::move(next);
        return true;
    }

    std::vector<int> vertMap(next.positions.size(), -1);
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
        if (selected[triangle] == 0) {
            continue;
        }
        for (int corner = 0; corner < 3; ++corner) {
            const int index = next.indices[triangle * 3U + static_cast<std::size_t>(corner)];
            if (index < 0 || index >= static_cast<int>(vertMap.size())
                || vertMap[static_cast<std::size_t>(index)] >= 0) {
                continue;
            }
            vertMap[static_cast<std::size_t>(index)] = static_cast<int>(next.positions.size());
            next.positions.push_back(next.positions[static_cast<std::size_t>(index)] + hitNormal * distance);
            if (!next.normals.empty() && static_cast<std::size_t>(index) < next.normals.size()) {
                next.normals.push_back(next.normals[static_cast<std::size_t>(index)]);
            }
            if (!next.texCoords.empty() && static_cast<std::size_t>(index) < next.texCoords.size()) {
                next.texCoords.push_back(next.texCoords[static_cast<std::size_t>(index)]);
            }
            if (inheritWeights) {
                nextNames.resize(next.positions.size());
                nextNames[next.positions.size() - 1U] = nextNames[static_cast<std::size_t>(index)];
            }
            if (inheritInfluences) {
                nextInfluences.resize(next.positions.size());
                nextInfluences[next.positions.size() - 1U] =
                    nextInfluences[static_cast<std::size_t>(index)];
            }
        }
    }
    if (next.positions.size() > static_cast<std::size_t>(ri::content::NativeSculptDocument::kMaxVertices)) {
        return false;
    }

    std::unordered_set<std::uint64_t> directed{};
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
        if (selected[triangle] == 0) {
            continue;
        }
        const int ia = next.indices[triangle * 3U];
        const int ib = next.indices[triangle * 3U + 1U];
        const int ic = next.indices[triangle * 3U + 2U];
        next.indices[triangle * 3U] = vertMap[static_cast<std::size_t>(ia)];
        next.indices[triangle * 3U + 1U] = vertMap[static_cast<std::size_t>(ib)];
        next.indices[triangle * 3U + 2U] = vertMap[static_cast<std::size_t>(ic)];
        directed.insert((static_cast<std::uint64_t>(static_cast<std::uint32_t>(ia)) << 32U)
            | static_cast<std::uint32_t>(ib));
        directed.insert((static_cast<std::uint64_t>(static_cast<std::uint32_t>(ib)) << 32U)
            | static_cast<std::uint32_t>(ic));
        directed.insert((static_cast<std::uint64_t>(static_cast<std::uint32_t>(ic)) << 32U)
            | static_cast<std::uint32_t>(ia));
    }
    for (const std::uint64_t edge : directed) {
        const int from = static_cast<int>(edge >> 32U);
        const int to = static_cast<int>(edge & 0xffffffffU);
        const std::uint64_t opposite = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(to)) << 32U)
            | static_cast<std::uint32_t>(from);
        if (directed.contains(opposite)) {
            continue;
        }
        const int nf = vertMap[static_cast<std::size_t>(from)];
        const int nt = vertMap[static_cast<std::size_t>(to)];
        next.indices.insert(next.indices.end(), {from, to, nt, from, nt, nf});
    }
    if (next.indices.size() > static_cast<std::size_t>(ri::content::NativeSculptDocument::kMaxIndices)) {
        return false;
    }
    RecalculateSculptNormals(next);
    next.vertexCount = static_cast<int>(next.positions.size());
    next.indexCount = static_cast<int>(next.indices.size());
    next.primitive = PrimitiveType::Custom;
    if (inheritWeights) {
        *vertexBoneNames = std::move(nextNames);
    }
    if (inheritInfluences) {
        *vertexInfluences = std::move(nextInfluences);
    }
    mesh = std::move(next);
    return true;
}

namespace {

[[nodiscard]] std::uint64_t UndirectedEdgeKey(const int a, const int b) {
    const auto lo = static_cast<std::uint32_t>(std::min(a, b));
    const auto hi = static_cast<std::uint32_t>(std::max(a, b));
    return (static_cast<std::uint64_t>(lo) << 32U) | static_cast<std::uint64_t>(hi);
}

[[nodiscard]] bool SculptTopologyEdited(const ri::content::NativeSculptDocument& document) {
    const NativeSculptCage cage = ParseNativeSculptCage(document.cage);
    const Mesh cageNow = MakeNativeSculptCageMesh(
        cage, document.segmentsAround, document.segmentsDown, document.displayName);
    return document.mesh.positions.size() > cageNow.positions.size() + 2U
        || document.mesh.indices.size() > cageNow.indices.size() + 6U;
}

} // namespace

bool SubdivideNativeSculptMesh(Mesh& mesh) {
    if (mesh.positions.empty() || mesh.indices.size() < 3U) {
        return false;
    }
    const std::size_t triangleCount = mesh.indices.size() / 3U;
    if (triangleCount == 0U) {
        return false;
    }
    Mesh next{};
    next.name = mesh.name;
    next.primitive = PrimitiveType::Custom;
    next.geometryMode = mesh.geometryMode;
    next.positions = mesh.positions;
    next.normals = mesh.normals;
    next.texCoords = mesh.texCoords;
    std::unordered_map<std::uint64_t, int> midpoints{};
    const auto midpointOf = [&](const int a, const int b) -> int {
        const std::uint64_t key = UndirectedEdgeKey(a, b);
        if (const auto found = midpoints.find(key); found != midpoints.end()) {
            return found->second;
        }
        const auto ia = static_cast<std::size_t>(a);
        const auto ib = static_cast<std::size_t>(b);
        const int index = static_cast<int>(next.positions.size());
        next.positions.push_back((mesh.positions[ia] + mesh.positions[ib]) * 0.5f);
        if (!mesh.normals.empty() && ia < mesh.normals.size() && ib < mesh.normals.size()) {
            const ri::math::Vec3 summed = mesh.normals[ia] + mesh.normals[ib];
            next.normals.push_back(
                ri::math::LengthSquared(summed) > 0.0000001f ? ri::math::Normalize(summed)
                                                             : mesh.normals[ia]);
        }
        if (!mesh.texCoords.empty() && ia < mesh.texCoords.size() && ib < mesh.texCoords.size()) {
            next.texCoords.push_back((mesh.texCoords[ia] + mesh.texCoords[ib]) * 0.5f);
        }
        midpoints.emplace(key, index);
        return index;
    };
    next.indices.reserve(triangleCount * 12U);
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
        const int ia = mesh.indices[triangle * 3U];
        const int ib = mesh.indices[triangle * 3U + 1U];
        const int ic = mesh.indices[triangle * 3U + 2U];
        if (ia < 0 || ib < 0 || ic < 0
            || ia >= static_cast<int>(mesh.positions.size())
            || ib >= static_cast<int>(mesh.positions.size())
            || ic >= static_cast<int>(mesh.positions.size())) {
            continue;
        }
        const int mab = midpointOf(ia, ib);
        const int mbc = midpointOf(ib, ic);
        const int mca = midpointOf(ic, ia);
        next.indices.insert(next.indices.end(), {ia, mab, mca, ib, mbc, mab, ic, mca, mbc, mab, mbc, mca});
    }
    if (next.indices.size() < 3U
        || next.positions.size() > static_cast<std::size_t>(ri::content::NativeSculptDocument::kMaxVertices)
        || next.indices.size() > static_cast<std::size_t>(ri::content::NativeSculptDocument::kMaxIndices)) {
        return false;
    }
    RecalculateSculptNormals(next);
    next.vertexCount = static_cast<int>(next.positions.size());
    next.indexCount = static_cast<int>(next.indices.size());
    mesh = std::move(next);
    return true;
}

bool RebuildNativeSculptDensity(
    ri::content::NativeSculptDocument& document,
    const int segmentsAround,
    const int segmentsDown) {
    if (!ri::content::ValidateNativeSculptDocument(document).valid) {
        return false;
    }
    const NativeSculptCage cage = ParseNativeSculptCage(document.cage);
    const int around = std::clamp(segmentsAround, 8, 96);
    const int down = cage == NativeSculptCage::Cube ? around : std::clamp(segmentsDown, 4, 48);
    if (around == document.segmentsAround && down == document.segmentsDown) {
        return false;
    }
    const bool edited = SculptTopologyEdited(document);
    const bool denser = around > document.segmentsAround || down > document.segmentsDown;
    const bool coarser = around < document.segmentsAround || down < document.segmentsDown;
    if (edited && coarser && !denser) {
        return false;
    }
    const int previousAround = document.segmentsAround;
    const int previousDown = document.segmentsDown;
    const Mesh source = document.mesh;
    Mesh next{};
    if (edited && denser) {
        next = source;
        if (!SubdivideNativeSculptMesh(next)) {
            return false;
        }
    } else {
        next = MakeNativeSculptCageMesh(cage, around, down, document.displayName);
        for (ri::math::Vec3& position : next.positions) {
            position = ClosestPointOnMesh(source, position);
        }
        RecalculateSculptNormals(next);
        next.vertexCount = static_cast<int>(next.positions.size());
        next.indexCount = static_cast<int>(next.indices.size());
    }
    document.segmentsAround = around;
    document.segmentsDown = down;
    document.mesh = std::move(next);
    if (document.vertexBoneNames.size() != document.mesh.positions.size()) {
        document.vertexBoneNames.clear();
    }
    if (document.vertexInfluences.size() != document.mesh.positions.size()) {
        document.vertexInfluences.clear();
    }
    if (!ri::content::ValidateNativeSculptDocument(document).valid) {
        document.segmentsAround = previousAround;
        document.segmentsDown = previousDown;
        document.mesh = source;
        return false;
    }
    return true;
}

ri::content::NativeSculptDocument CreateNativeSculptDocument(
    std::string id,
    std::string displayName,
    const NativeSculptCage cage,
    const int segmentsAround,
    const int segmentsDown) {
    ri::content::NativeSculptDocument document{};
    document.id = Slugify(std::move(id), "clay_sphere");
    document.displayName = displayName.empty() ? document.id : std::move(displayName);
    document.cage = std::string(NativeSculptCageName(cage));
    document.segmentsAround = std::clamp(segmentsAround, 8, 96);
    document.segmentsDown = std::clamp(segmentsDown, 4, 48);
    document.mesh = MakeNativeSculptCageMesh(
        cage, document.segmentsAround, document.segmentsDown, document.displayName);
    return document;
}

int InstantiateNativeSculpt(
    Scene& scene,
    const int parent,
    const ri::content::NativeSculptDocument& document) {
    if (!ri::content::ValidateNativeSculptDocument(document).valid) {
        return kInvalidHandle;
    }
    const int material = scene.AddMaterial(Material{
        .name = document.id + "ClayMaterial",
        .shadingModel = ShadingModel::Lit,
        .baseColor = {0.78f, 0.52f, 0.34f},
        .metallic = 0.0f,
        .roughness = 0.82f,
    });
    Mesh mesh = document.mesh;
    mesh.primitive = PrimitiveType::Custom;
    if (mesh.normals.size() != mesh.positions.size()) {
        RecalculateSculptNormals(mesh);
    }
    const int meshHandle = scene.AddMesh(std::move(mesh));
    const int node = scene.CreateNode(document.displayName.empty() ? document.id : document.displayName, parent);
    scene.AttachMesh(node, meshHandle, material);
    return node;
}

void NativeSculptUndoStack::Capture(const Mesh& mesh) {
    undo_.push_back(Frame{.mesh = mesh});
    if (undo_.size() > kMaxDepth) {
        undo_.erase(undo_.begin());
    }
    redo_.clear();
}

void NativeSculptUndoStack::Capture(const ri::content::NativeSculptDocument& document) {
    undo_.push_back(Frame{
        .mesh = document.mesh,
        .segmentsAround = document.segmentsAround,
        .segmentsDown = document.segmentsDown,
        .vertexBoneNames = document.vertexBoneNames,
        .vertexInfluences = document.vertexInfluences,
        .capturedBind = true,
    });
    if (undo_.size() > kMaxDepth) {
        undo_.erase(undo_.begin());
    }
    redo_.clear();
}

bool NativeSculptUndoStack::CanUndo() const noexcept {
    return !undo_.empty();
}

bool NativeSculptUndoStack::CanRedo() const noexcept {
    return !redo_.empty();
}

bool NativeSculptUndoStack::Undo(Mesh& mesh) {
    if (undo_.empty()) {
        return false;
    }
    redo_.push_back(Frame{.mesh = std::move(mesh)});
    mesh = std::move(undo_.back().mesh);
    undo_.pop_back();
    return true;
}

bool NativeSculptUndoStack::Undo(ri::content::NativeSculptDocument& document) {
    if (undo_.empty()) {
        return false;
    }
    redo_.push_back(Frame{
        .mesh = document.mesh,
        .segmentsAround = document.segmentsAround,
        .segmentsDown = document.segmentsDown,
        .vertexBoneNames = document.vertexBoneNames,
        .vertexInfluences = document.vertexInfluences,
        .capturedBind = true,
    });
    Frame frame = std::move(undo_.back());
    undo_.pop_back();
    document.mesh = std::move(frame.mesh);
    if (frame.segmentsAround >= 0) {
        document.segmentsAround = frame.segmentsAround;
        document.segmentsDown = frame.segmentsDown;
    }
    if (frame.capturedBind) {
        document.vertexBoneNames = std::move(frame.vertexBoneNames);
        document.vertexInfluences = std::move(frame.vertexInfluences);
    }
    return true;
}

bool NativeSculptUndoStack::Redo(Mesh& mesh) {
    if (redo_.empty()) {
        return false;
    }
    undo_.push_back(Frame{.mesh = std::move(mesh)});
    mesh = std::move(redo_.back().mesh);
    redo_.pop_back();
    return true;
}

bool NativeSculptUndoStack::Redo(ri::content::NativeSculptDocument& document) {
    if (redo_.empty()) {
        return false;
    }
    undo_.push_back(Frame{
        .mesh = document.mesh,
        .segmentsAround = document.segmentsAround,
        .segmentsDown = document.segmentsDown,
        .vertexBoneNames = document.vertexBoneNames,
        .vertexInfluences = document.vertexInfluences,
        .capturedBind = true,
    });
    Frame frame = std::move(redo_.back());
    redo_.pop_back();
    document.mesh = std::move(frame.mesh);
    if (frame.segmentsAround >= 0) {
        document.segmentsAround = frame.segmentsAround;
        document.segmentsDown = frame.segmentsDown;
    }
    if (frame.capturedBind) {
        document.vertexBoneNames = std::move(frame.vertexBoneNames);
        document.vertexInfluences = std::move(frame.vertexInfluences);
    }
    return true;
}

void NativeSculptUndoStack::Clear() {
    undo_.clear();
    redo_.clear();
}

void WriteNativeSculptMesh(Scene& scene, const int sculptNode, const Mesh& mesh) {
    if (sculptNode == kInvalidHandle) {
        return;
    }
    Node& node = scene.GetNode(sculptNode);
    if (node.mesh == kInvalidHandle) {
        return;
    }
    Mesh& dest = scene.GetMesh(node.mesh);
    dest = mesh;
    dest.primitive = PrimitiveType::Custom;
    dest.vertexCount = static_cast<int>(dest.positions.size());
    dest.indexCount = static_cast<int>(dest.indices.size());
}

Mesh MakeNativeSculptWireframeMesh(const Mesh& source, const float thickness) {
    Mesh mesh{};
    mesh.name = source.name.empty() ? "SculptWireframe" : (source.name + "Wireframe");
    mesh.primitive = PrimitiveType::Custom;
    std::unordered_set<std::uint64_t> seen{};
    const std::size_t triangleCount = source.indices.size() / 3U;
    constexpr std::size_t kMaxEdges = 24000;
    const auto consider = [&](const int ia, const int ib) {
        if (ia < 0 || ib < 0 || ia == ib
            || ia >= static_cast<int>(source.positions.size())
            || ib >= static_cast<int>(source.positions.size())
            || seen.size() >= kMaxEdges) {
            return;
        }
        const auto lo = static_cast<std::uint32_t>(std::min(ia, ib));
        const auto hi = static_cast<std::uint32_t>(std::max(ia, ib));
        const std::uint64_t key = (static_cast<std::uint64_t>(hi) << 32U) | lo;
        if (!seen.insert(key).second) {
            return;
        }
        AppendEdgeBox(
            mesh,
            source.positions[static_cast<std::size_t>(ia)],
            source.positions[static_cast<std::size_t>(ib)],
            thickness);
    };
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
        consider(source.indices[triangle * 3U], source.indices[triangle * 3U + 1U]);
        consider(source.indices[triangle * 3U + 1U], source.indices[triangle * 3U + 2U]);
        consider(source.indices[triangle * 3U + 2U], source.indices[triangle * 3U]);
    }
    RecalculateSculptNormals(mesh);
    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    return mesh;
}

int InstantiateNativeSculptWireframe(Scene& scene, const int parent, const Mesh& source) {
    Mesh mesh = MakeNativeSculptWireframeMesh(source);
    const int material = scene.AddMaterial(Material{
        .name = "NativeSculptWireframeMaterial",
        .shadingModel = ShadingModel::Unlit,
        .baseColor = {0.06f, 0.07f, 0.08f},
        .metallic = 0.0f,
        .roughness = 1.0f,
    });
    const int meshHandle = scene.AddMesh(std::move(mesh));
    const int node = scene.CreateNode("NativeSculptWireframe", parent);
    scene.AttachMesh(node, meshHandle, material);
    return node;
}

void UpdateNativeSculptWireframe(Scene& scene, const int wireframeNode, const Mesh& source) {
    if (wireframeNode == kInvalidHandle) {
        return;
    }
    Node& node = scene.GetNode(wireframeNode);
    if (node.mesh == kInvalidHandle) {
        return;
    }
    scene.GetMesh(node.mesh) = MakeNativeSculptWireframeMesh(source);
}

int InstantiateNativeSculptBrushCursor(Scene& scene, const int parent) {
    PrimitiveNodeOptions cursor{};
    cursor.nodeName = "NativeSculptBrushCursor";
    cursor.parent = parent;
    cursor.primitive = PrimitiveType::Sphere;
    cursor.shadingModel = ShadingModel::Unlit;
    cursor.materialName = "NativeSculptBrushCursorMaterial";
    cursor.baseColor = {0.22f, 0.86f, 1.0f};
    cursor.opacity = 0.42f;
    cursor.transparent = true;
    cursor.doubleSided = true;
    cursor.transform.scale = {0.0001f, 0.0001f, 0.0001f};
    return AddPrimitiveNode(scene, cursor);
}

void UpdateNativeSculptBrushCursor(
    Scene& scene,
    const int cursorNode,
    const ri::math::Vec3& worldPosition,
    const float radius,
    const bool visible) {
    if (cursorNode == kInvalidHandle) {
        return;
    }
    Node& node = scene.GetNode(cursorNode);
    node.localTransform.position = worldPosition;
    const float scale = visible ? std::max(radius * 2.0f, 0.02f) : 0.0001f;
    node.localTransform.scale = {scale, scale, scale};
}

Mesh MakeNativeSculptNormalsMesh(const Mesh& source, const float length) {
    Mesh mesh{};
    mesh.name = source.name.empty() ? "SculptNormals" : (source.name + "Normals");
    mesh.primitive = PrimitiveType::Custom;
    if (source.positions.empty()) {
        return mesh;
    }
    std::vector<ri::math::Vec3> normals = source.normals;
    if (normals.size() != source.positions.size()) {
        Mesh counted = source;
        RecalculateSculptNormals(counted);
        normals = counted.normals;
    }
    const float tick = std::max(length, 0.02f);
    const std::size_t stride = std::max<std::size_t>(1, source.positions.size() / 2048U);
    for (std::size_t index = 0; index < source.positions.size(); index += stride) {
        const ri::math::Vec3 direction = index < normals.size() && ri::math::LengthSquared(normals[index]) > 0.000001f
            ? ri::math::Normalize(normals[index])
            : ri::math::Vec3{0.0f, 1.0f, 0.0f};
        AppendEdgeBox(mesh, source.positions[index], source.positions[index] + direction * tick, tick * 0.12f);
    }
    RecalculateSculptNormals(mesh);
    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    return mesh;
}

int InstantiateNativeSculptNormals(Scene& scene, const int parent, const Mesh& source) {
    return InstantiateUnlitOverlay(
        scene, parent, "NativeSculptNormals", {0.35f, 0.82f, 1.0f}, MakeNativeSculptNormalsMesh(source));
}

void UpdateNativeSculptNormals(Scene& scene, const int normalsNode, const Mesh& source) {
    ReplaceOverlayMesh(scene, normalsNode, MakeNativeSculptNormalsMesh(source));
}

Mesh MakeNativeSculptCollisionBoundsMesh(const Mesh& source, const float thickness) {
    Mesh mesh{};
    mesh.name = source.name.empty() ? "SculptCollision" : (source.name + "Collision");
    mesh.primitive = PrimitiveType::Custom;
    if (source.positions.empty()) {
        return mesh;
    }
    ri::math::Vec3 min = source.positions.front();
    ri::math::Vec3 max = source.positions.front();
    for (const ri::math::Vec3& position : source.positions) {
        min.x = std::min(min.x, position.x);
        min.y = std::min(min.y, position.y);
        min.z = std::min(min.z, position.z);
        max.x = std::max(max.x, position.x);
        max.y = std::max(max.y, position.y);
        max.z = std::max(max.z, position.z);
    }
    const ri::math::Vec3 corners[8] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {max.x, max.y, min.z},
        {min.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {max.x, max.y, max.z},
        {min.x, max.y, max.z},
    };
    const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };
    for (const auto& edge : edges) {
        AppendEdgeBox(mesh, corners[edge[0]], corners[edge[1]], std::max(thickness, 0.008f));
    }
    RecalculateSculptNormals(mesh);
    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    return mesh;
}

int InstantiateNativeSculptCollisionBounds(Scene& scene, const int parent, const Mesh& source) {
    return InstantiateUnlitOverlay(
        scene,
        parent,
        "NativeSculptCollision",
        {0.95f, 0.42f, 0.18f},
        MakeNativeSculptCollisionBoundsMesh(source));
}

void UpdateNativeSculptCollisionBounds(Scene& scene, const int collisionNode, const Mesh& source) {
    ReplaceOverlayMesh(scene, collisionNode, MakeNativeSculptCollisionBoundsMesh(source));
}

NativeSculptBindResult BindNativeSculptToRig(
    ri::content::NativeSculptDocument& document,
    const RigDefinition& rig,
    const std::string_view rigPath,
    const bool replaceAssigned) {
    NativeSculptBindResult result{};
    if (document.mesh.positions.empty()) {
        result.summary = "Sculpt has no vertices to bind.";
        return result;
    }

    const std::unordered_map<std::string, ri::math::Mat4> restWorld = RestBoneWorldMatrices(rig);
    std::vector<std::string> deformNames{};
    deformNames.reserve(rig.bones.size());
    for (const RigBone& bone : rig.bones) {
        if (bone.deform && !bone.name.empty() && restWorld.contains(bone.name)) {
            deformNames.push_back(bone.name);
        }
    }
    if (deformNames.empty()) {
        result.summary = "Rig has no named deform bones.";
        return result;
    }

    const std::unordered_set<std::string> deformSet(deformNames.begin(), deformNames.end());
    bool anyAssigned = false;
    for (const std::string& boneName : document.vertexBoneNames) {
        if (deformSet.contains(boneName)) {
            anyAssigned = true;
            break;
        }
    }

    document.rigPath = std::string(rigPath);
    document.vertexBoneNames.resize(document.mesh.positions.size());
    document.vertexInfluences.resize(document.mesh.positions.size());
    const bool keepAssigned = !replaceAssigned && anyAssigned;
    std::unordered_set<std::string> usedBones{};
    const auto nearestBone = [&](const ri::math::Vec3& position) {
        std::string best = deformNames.front();
        float bestDistance = ri::math::DistanceSquared(
            position, ri::math::ExtractTranslation(restWorld.at(best)));
        for (std::size_t deform = 1; deform < deformNames.size(); ++deform) {
            const std::string& boneName = deformNames[deform];
            const float distance = ri::math::DistanceSquared(
                position, ri::math::ExtractTranslation(restWorld.at(boneName)));
            if (distance < bestDistance) {
                bestDistance = distance;
                best = boneName;
            }
        }
        return best;
    };
    const auto writeRigid = [&](const std::size_t vertex, const std::string& boneName) {
        document.vertexBoneNames[vertex] = boneName;
        if (boneName.empty()) {
            document.vertexInfluences[vertex].clear();
            return;
        }
        document.vertexInfluences[vertex] = {
            ri::content::NativeSculptVertexInfluence{.boneName = boneName, .weight = 1.0f},
        };
    };
    for (std::size_t vertex = 0; vertex < document.mesh.positions.size(); ++vertex) {
        std::string& boneName = document.vertexBoneNames[vertex];
        if (keepAssigned) {
            if (boneName.empty()) {
                document.vertexInfluences[vertex].clear();
                continue;
            }
            if (deformSet.contains(boneName)) {
                usedBones.insert(boneName);
                bool hasBone = false;
                for (const ri::content::NativeSculptVertexInfluence& influence :
                     document.vertexInfluences[vertex]) {
                    if (influence.boneName == boneName && influence.weight > 0.0f) {
                        hasBone = true;
                        break;
                    }
                }
                if (!hasBone) {
                    writeRigid(vertex, boneName);
                }
                continue;
            }
        }
        writeRigid(vertex, nearestBone(document.mesh.positions[vertex]));
        usedBones.insert(document.vertexBoneNames[vertex]);
    }

    result.valid = true;
    result.boundVertexCount = 0;
    for (const std::string& boneName : document.vertexBoneNames) {
        if (!boneName.empty()) {
            ++result.boundVertexCount;
        }
    }
    result.deformBoneCount = usedBones.size();
    result.summary = "Bound " + std::to_string(result.boundVertexCount) + " verts to "
        + std::to_string(result.deformBoneCount) + " bones";
    return result;
}

std::unordered_map<std::string, ri::math::Mat4> RestBoneWorldMatrices(const RigDefinition& rig) {
    std::unordered_map<std::string, ri::math::Mat4> worlds{};
    std::vector<ri::math::Mat4> ordered(rig.bones.size(), ri::math::IdentityMatrix());
    for (std::size_t index = 0; index < rig.bones.size(); ++index) {
        const RigBone& bone = rig.bones[index];
        const ri::math::Mat4 local = bone.restLocal.LocalMatrix();
        if (bone.parentIndex >= 0 && static_cast<std::size_t>(bone.parentIndex) < ordered.size()
            && static_cast<std::size_t>(bone.parentIndex) != index) {
            ordered[index] = ri::math::Multiply(
                ordered[static_cast<std::size_t>(bone.parentIndex)], local);
        } else {
            ordered[index] = local;
        }
        if (!bone.name.empty()) {
            worlds[bone.name] = ordered[index];
        }
    }
    return worlds;
}

std::size_t SkinRigidMesh(
    Mesh& mesh,
    const std::vector<std::string>& vertexBoneNames,
    const std::vector<ri::math::Vec3>& restPositions,
    const std::vector<ri::math::Vec3>& restNormals,
    const std::unordered_map<std::string, ri::math::Mat4>& restBoneWorld,
    const std::unordered_map<std::string, ri::math::Mat4>& posedBoneWorld,
    const std::vector<std::vector<ri::content::NativeSculptVertexInfluence>>& vertexInfluences) {
    if (restPositions.empty() || restBoneWorld.empty()) {
        return 0;
    }
    const bool blended = vertexInfluences.size() == restPositions.size();
    if (!blended && vertexBoneNames.size() != restPositions.size()) {
        return 0;
    }
    mesh.positions = restPositions;
    if (restNormals.size() == restPositions.size()) {
        mesh.normals = restNormals;
    }
    std::unordered_map<std::string, ri::math::Mat4> skinMats{};
    const auto skinOf = [&](const std::string& boneName) -> const ri::math::Mat4* {
        if (boneName.empty()) {
            return nullptr;
        }
        if (const auto cached = skinMats.find(boneName); cached != skinMats.end()) {
            return &cached->second;
        }
        const auto restIt = restBoneWorld.find(boneName);
        const auto posedIt = posedBoneWorld.find(boneName);
        if (restIt == restBoneWorld.end() || posedIt == posedBoneWorld.end()) {
            return nullptr;
        }
        ri::math::Mat4 inverseRest{};
        if (!ri::math::TryInvertMat4(restIt->second, inverseRest)) {
            return nullptr;
        }
        const auto inserted = skinMats.emplace(boneName, ri::math::Multiply(posedIt->second, inverseRest));
        return &inserted.first->second;
    };
    std::size_t skinned = 0;
    for (std::size_t vertex = 0; vertex < restPositions.size(); ++vertex) {
        std::vector<ri::content::NativeSculptVertexInfluence> influences{};
        if (blended && !vertexInfluences[vertex].empty()) {
            influences = vertexInfluences[vertex];
        } else if (vertex < vertexBoneNames.size() && !vertexBoneNames[vertex].empty()) {
            influences.push_back(ri::content::NativeSculptVertexInfluence{
                .boneName = vertexBoneNames[vertex],
                .weight = 1.0f,
            });
        }
        if (influences.empty()) {
            continue;
        }
        ri::math::Vec3 position{};
        ri::math::Vec3 normal{};
        float weightSum = 0.0f;
        bool any = false;
        for (const ri::content::NativeSculptVertexInfluence& influence : influences) {
            if (influence.weight <= 0.0f) {
                continue;
            }
            const ri::math::Mat4* skin = skinOf(influence.boneName);
            if (skin == nullptr) {
                continue;
            }
            position = position + (ri::math::TransformPoint(*skin, restPositions[vertex]) * influence.weight);
            if (vertex < restNormals.size()) {
                normal = normal + (ri::math::TransformVector(*skin, restNormals[vertex]) * influence.weight);
            }
            weightSum += influence.weight;
            any = true;
        }
        if (!any || weightSum <= 0.000001f) {
            continue;
        }
        mesh.positions[vertex] = position * (1.0f / weightSum);
        if (vertex < mesh.normals.size()) {
            mesh.normals[vertex] = ri::math::LengthSquared(normal) > 0.0000001f
                ? ri::math::Normalize(normal)
                : restNormals[vertex];
        }
        ++skinned;
    }
    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    mesh.primitive = PrimitiveType::Custom;
    return skinned;
}

Mesh MakeNativeSculptWeightMesh(
    const Mesh& source,
    const std::vector<std::string>& vertexBoneNames,
    const std::string_view selectedBone,
    const std::vector<std::vector<ri::content::NativeSculptVertexInfluence>>& vertexInfluences) {
    Mesh mesh{};
    mesh.name = source.name.empty() ? "SculptWeights" : (source.name + "Weights");
    mesh.primitive = PrimitiveType::Custom;
    if (selectedBone.empty()
        || source.indices.size() < 3U
        || (vertexBoneNames.size() != source.positions.size()
            && vertexInfluences.size() != source.positions.size())) {
        mesh.vertexCount = 0;
        mesh.indexCount = 0;
        return mesh;
    }
    const std::string selected{selectedBone};
    const auto selectedWeight = [&](const std::size_t index) {
        if (index < vertexInfluences.size()) {
            for (const ri::content::NativeSculptVertexInfluence& influence : vertexInfluences[index]) {
                if (influence.boneName == selected) {
                    return influence.weight;
                }
            }
            return 0.0f;
        }
        if (index < vertexBoneNames.size() && vertexBoneNames[index] == selected) {
            return 1.0f;
        }
        return 0.0f;
    };
    constexpr float kOffset = 0.012f;
    constexpr float kMinWeight = 0.04f;
    const std::size_t triangleCount = source.indices.size() / 3U;
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
        const int ia = source.indices[triangle * 3U];
        const int ib = source.indices[triangle * 3U + 1U];
        const int ic = source.indices[triangle * 3U + 2U];
        if (ia < 0 || ib < 0 || ic < 0
            || ia >= static_cast<int>(source.positions.size())
            || ib >= static_cast<int>(source.positions.size())
            || ic >= static_cast<int>(source.positions.size())) {
            continue;
        }
        const auto a = static_cast<std::size_t>(ia);
        const auto b = static_cast<std::size_t>(ib);
        const auto c = static_cast<std::size_t>(ic);
        if (selectedWeight(a) < kMinWeight && selectedWeight(b) < kMinWeight
            && selectedWeight(c) < kMinWeight) {
            continue;
        }
        const int base = static_cast<int>(mesh.positions.size());
        const auto emit = [&](const std::size_t index) {
            ri::math::Vec3 position = source.positions[index];
            if (index < source.normals.size()) {
                position = position + (source.normals[index] * kOffset);
            }
            mesh.positions.push_back(position);
            if (index < source.normals.size()) {
                mesh.normals.push_back(source.normals[index]);
            }
        };
        emit(a);
        emit(b);
        emit(c);
        mesh.indices.push_back(base);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
    }
    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    return mesh;
}

int InstantiateNativeSculptWeightOverlay(Scene& scene, const int parent) {
    return InstantiateUnlitOverlay(
        scene,
        parent,
        "NativeSculptWeights",
        {0.18f, 0.92f, 0.42f},
        MakeNativeSculptWeightMesh({}, {}, {}));
}

void UpdateNativeSculptWeightOverlay(
    Scene& scene,
    const int overlayNode,
    const Mesh& source,
    const std::vector<std::string>& vertexBoneNames,
    const std::string_view selectedBone,
    const std::vector<std::vector<ri::content::NativeSculptVertexInfluence>>& vertexInfluences) {
    ReplaceOverlayMesh(
        scene,
        overlayNode,
        MakeNativeSculptWeightMesh(source, vertexBoneNames, selectedBone, vertexInfluences));
}

namespace {

[[nodiscard]] std::vector<ri::math::Vec3> MirroredPaintCenters(
    const ri::math::Vec3& worldPosition,
    const bool mirrorX,
    const bool mirrorY,
    const bool mirrorZ) {
    std::vector<ri::math::Vec3> centers{};
    const int xLimit = mirrorX ? 1 : 0;
    const int yLimit = mirrorY ? 1 : 0;
    const int zLimit = mirrorZ ? 1 : 0;
    for (int flipX = 0; flipX <= xLimit; ++flipX) {
        if (flipX == 1 && std::abs(worldPosition.x) <= 0.0001f) {
            continue;
        }
        for (int flipY = 0; flipY <= yLimit; ++flipY) {
            if (flipY == 1 && std::abs(worldPosition.y) <= 0.0001f) {
                continue;
            }
            for (int flipZ = 0; flipZ <= zLimit; ++flipZ) {
                if (flipZ == 1 && std::abs(worldPosition.z) <= 0.0001f) {
                    continue;
                }
                ri::math::Vec3 local = worldPosition;
                if (flipX == 1) {
                    local.x = -local.x;
                }
                if (flipY == 1) {
                    local.y = -local.y;
                }
                if (flipZ == 1) {
                    local.z = -local.z;
                }
                centers.push_back(local);
            }
        }
    }
    return centers;
}

void EnsureVertexBoneNames(ri::content::NativeSculptDocument& document) {
    document.vertexBoneNames.resize(document.mesh.positions.size());
}

void EnsureVertexInfluences(ri::content::NativeSculptDocument& document) {
    EnsureVertexBoneNames(document);
    if (document.vertexInfluences.size() == document.mesh.positions.size()) {
        return;
    }
    document.vertexInfluences.assign(document.mesh.positions.size(), {});
    for (std::size_t vertex = 0; vertex < document.vertexBoneNames.size(); ++vertex) {
        if (!document.vertexBoneNames[vertex].empty()) {
            document.vertexInfluences[vertex] = {
                ri::content::NativeSculptVertexInfluence{
                    .boneName = document.vertexBoneNames[vertex],
                    .weight = 1.0f,
                },
            };
        }
    }
}

void NormalizeVertexInfluences(
    std::vector<ri::content::NativeSculptVertexInfluence>& influences,
    std::string& dominantName,
    const float minWeight = 0.02f) {
    auto firstRemoved = std::remove_if(
        influences.begin(),
        influences.end(),
        [&](const ri::content::NativeSculptVertexInfluence& influence) {
            return influence.boneName.empty() || influence.weight < minWeight || !std::isfinite(influence.weight);
        });
    influences.erase(firstRemoved, influences.end());
    std::sort(
        influences.begin(),
        influences.end(),
        [](const ri::content::NativeSculptVertexInfluence& lhs,
           const ri::content::NativeSculptVertexInfluence& rhs) {
            return lhs.weight > rhs.weight;
        });
    if (influences.size() > static_cast<std::size_t>(ri::content::NativeSculptDocument::kMaxInfluences)) {
        influences.resize(static_cast<std::size_t>(ri::content::NativeSculptDocument::kMaxInfluences));
    }
    float sum = 0.0f;
    for (const ri::content::NativeSculptVertexInfluence& influence : influences) {
        sum += influence.weight;
    }
    if (sum <= 0.000001f) {
        influences.clear();
        dominantName.clear();
        return;
    }
    for (ri::content::NativeSculptVertexInfluence& influence : influences) {
        influence.weight /= sum;
    }
    dominantName = influences.front().boneName;
}

void WriteRigidInfluence(
    ri::content::NativeSculptDocument& document,
    const std::size_t vertex,
    const std::string& boneName) {
    EnsureVertexInfluences(document);
    document.vertexBoneNames[vertex] = boneName;
    if (boneName.empty()) {
        document.vertexInfluences[vertex].clear();
        return;
    }
    document.vertexInfluences[vertex] = {
        ri::content::NativeSculptVertexInfluence{.boneName = boneName, .weight = 1.0f},
    };
}

void AddVertexInfluence(
    ri::content::NativeSculptDocument& document,
    const std::size_t vertex,
    const std::string& boneName,
    const float amount) {
    EnsureVertexInfluences(document);
    auto& influences = document.vertexInfluences[vertex];
    bool found = false;
    for (ri::content::NativeSculptVertexInfluence& influence : influences) {
        if (influence.boneName == boneName) {
            influence.weight += amount;
            found = true;
            break;
        }
    }
    if (!found) {
        if (influences.size() < static_cast<std::size_t>(ri::content::NativeSculptDocument::kMaxInfluences)) {
            influences.push_back(
                ri::content::NativeSculptVertexInfluence{.boneName = boneName, .weight = amount});
        } else {
            std::size_t weakest = 0;
            for (std::size_t index = 1; index < influences.size(); ++index) {
                if (influences[index].weight < influences[weakest].weight) {
                    weakest = index;
                }
            }
            if (amount > influences[weakest].weight) {
                influences[weakest] = {.boneName = boneName, .weight = amount};
            }
        }
    }
    NormalizeVertexInfluences(influences, document.vertexBoneNames[vertex]);
}

[[nodiscard]] float ClosestPaintDistanceSquared(
    const ri::math::Vec3& position,
    const std::vector<ri::math::Vec3>& centers) {
    float best = 1.0e30f;
    for (const ri::math::Vec3& center : centers) {
        best = (std::min)(best, ri::math::DistanceSquared(position, center));
    }
    return best;
}

[[nodiscard]] bool VertexInPaintRadius(
    const ri::math::Vec3& position,
    const std::vector<ri::math::Vec3>& centers,
    const float radiusSquared) {
    return ClosestPaintDistanceSquared(position, centers) <= radiusSquared;
}

[[nodiscard]] std::vector<std::vector<std::size_t>> BuildVertexAdjacency(const Mesh& mesh) {
    std::vector<std::vector<std::size_t>> adjacency(mesh.positions.size());
    const std::size_t triangleCount = mesh.indices.size() / 3U;
    const auto addEdge = [&](const int a, const int b) {
        if (a < 0 || b < 0 || a == b
            || a >= static_cast<int>(mesh.positions.size())
            || b >= static_cast<int>(mesh.positions.size())) {
            return;
        }
        adjacency[static_cast<std::size_t>(a)].push_back(static_cast<std::size_t>(b));
        adjacency[static_cast<std::size_t>(b)].push_back(static_cast<std::size_t>(a));
    };
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
        const int ia = mesh.indices[triangle * 3U];
        const int ib = mesh.indices[triangle * 3U + 1U];
        const int ic = mesh.indices[triangle * 3U + 2U];
        addEdge(ia, ib);
        addEdge(ib, ic);
        addEdge(ic, ia);
    }
    for (std::vector<std::size_t>& neighbors : adjacency) {
        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
    }
    return adjacency;
}

[[nodiscard]] std::string SwapMirrorBoneName(const std::string& name) {
    constexpr std::string_view kLeft = "left_";
    constexpr std::string_view kRight = "right_";
    if (name.size() >= kLeft.size() && name.compare(0, kLeft.size(), kLeft) == 0) {
        return std::string(kRight) + name.substr(kLeft.size());
    }
    if (name.size() >= kRight.size() && name.compare(0, kRight.size(), kRight) == 0) {
        return std::string(kLeft) + name.substr(kRight.size());
    }
    if (name.size() >= 2U && name.ends_with("_l")) {
        return name.substr(0, name.size() - 2U) + "_r";
    }
    if (name.size() >= 2U && name.ends_with("_r")) {
        return name.substr(0, name.size() - 2U) + "_l";
    }
    return name;
}

[[nodiscard]] std::vector<ri::content::NativeSculptVertexInfluence> MirrorInfluenceList(
    const std::vector<ri::content::NativeSculptVertexInfluence>& influences) {
    std::vector<ri::content::NativeSculptVertexInfluence> mirrored{};
    mirrored.reserve(influences.size());
    for (const ri::content::NativeSculptVertexInfluence& influence : influences) {
        mirrored.push_back(ri::content::NativeSculptVertexInfluence{
            .boneName = SwapMirrorBoneName(influence.boneName),
            .weight = influence.weight,
        });
    }
    return mirrored;
}

[[nodiscard]] float MirrorMatchTolerance(const Mesh& mesh) {
    float maxExtent = 0.0f;
    for (const ri::math::Vec3& position : mesh.positions) {
        maxExtent = (std::max)(maxExtent, std::abs(position.x));
        maxExtent = (std::max)(maxExtent, std::abs(position.y));
        maxExtent = (std::max)(maxExtent, std::abs(position.z));
    }
    return (std::max)(0.015f, maxExtent * 0.04f);
}

[[nodiscard]] std::optional<std::size_t> FindMirrorPartnerVertex(
    const Mesh& mesh,
    const std::size_t sourceVertex,
    const bool mirrorX,
    const bool mirrorY,
    const bool mirrorZ,
    const float toleranceSquared) {
    const ri::math::Vec3& source = mesh.positions[sourceVertex];
    ri::math::Vec3 target = source;
    if (mirrorX) {
        target.x = -target.x;
    }
    if (mirrorY) {
        target.y = -target.y;
    }
    if (mirrorZ) {
        target.z = -target.z;
    }
    std::optional<std::size_t> best{};
    float bestDistance = toleranceSquared;
    for (std::size_t vertex = 0; vertex < mesh.positions.size(); ++vertex) {
        if (vertex == sourceVertex) {
            continue;
        }
        const float distance = ri::math::DistanceSquared(mesh.positions[vertex], target);
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = vertex;
        }
    }
    return best;
}

[[nodiscard]] bool IsSourceMirrorHalf(
    const ri::math::Vec3& position,
    const bool mirrorX,
    const bool mirrorY,
    const bool mirrorZ) {
    constexpr float kCenter = 0.0001f;
    if (mirrorX && position.x > kCenter) {
        return true;
    }
    if (mirrorY && position.y > kCenter) {
        return true;
    }
    if (mirrorZ && position.z > kCenter) {
        return true;
    }
    return false;
}

[[nodiscard]] float InfluenceWeightSum(
    const std::vector<ri::content::NativeSculptVertexInfluence>& influences) {
    float sum = 0.0f;
    for (const ri::content::NativeSculptVertexInfluence& influence : influences) {
        if (!influence.boneName.empty() && influence.weight > 0.0f && std::isfinite(influence.weight)) {
            sum += influence.weight;
        }
    }
    return sum;
}

} // namespace

std::size_t PaintNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    const ri::math::Vec3& worldPosition,
    const float radius,
    const std::string_view boneName,
    const NativeSculptWeightPaint mode,
    const bool mirrorX,
    const bool mirrorY,
    const bool mirrorZ,
    const float strength) {
    if (document.mesh.positions.empty() || radius <= 0.0f) {
        return 0;
    }
    if ((mode == NativeSculptWeightPaint::Assign || mode == NativeSculptWeightPaint::Add)
        && boneName.empty()) {
        return 0;
    }
    EnsureVertexInfluences(document);
    const std::vector<ri::math::Vec3> centers =
        MirroredPaintCenters(worldPosition, mirrorX, mirrorY, mirrorZ);
    const float radiusSquared = radius * radius;
    std::size_t changed = 0;
    if (mode == NativeSculptWeightPaint::Smooth) {
        const auto previous = document.vertexInfluences;
        const std::vector<std::vector<std::size_t>> adjacency = BuildVertexAdjacency(document.mesh);
        for (std::size_t vertex = 0; vertex < document.mesh.positions.size(); ++vertex) {
            if (!VertexInPaintRadius(document.mesh.positions[vertex], centers, radiusSquared)) {
                continue;
            }
            std::unordered_map<std::string, float> sums{};
            std::size_t sampleCount = 1;
            const auto accumulate = [&](const std::size_t index) {
                if (index >= previous.size()) {
                    return;
                }
                for (const ri::content::NativeSculptVertexInfluence& influence : previous[index]) {
                    sums[influence.boneName] += influence.weight;
                }
            };
            accumulate(vertex);
            for (const std::size_t neighbor : adjacency[vertex]) {
                accumulate(neighbor);
                ++sampleCount;
            }
            std::vector<ri::content::NativeSculptVertexInfluence> mixed{};
            mixed.reserve(sums.size());
            const float inv = 1.0f / static_cast<float>(sampleCount);
            for (const auto& [name, weight] : sums) {
                mixed.push_back(ri::content::NativeSculptVertexInfluence{
                    .boneName = name,
                    .weight = weight * inv,
                });
            }
            std::string dominant = document.vertexBoneNames[vertex];
            NormalizeVertexInfluences(mixed, dominant);
            if (mixed == document.vertexInfluences[vertex]
                && dominant == document.vertexBoneNames[vertex]) {
                continue;
            }
            document.vertexInfluences[vertex] = std::move(mixed);
            document.vertexBoneNames[vertex] = std::move(dominant);
            ++changed;
        }
        return changed;
    }
    if (mode == NativeSculptWeightPaint::Add) {
        const std::string target{boneName};
        const float amountScale = std::clamp(strength, 0.01f, 1.0f);
        for (std::size_t vertex = 0; vertex < document.mesh.positions.size(); ++vertex) {
            const float distanceSquared =
                ClosestPaintDistanceSquared(document.mesh.positions[vertex], centers);
            if (distanceSquared > radiusSquared) {
                continue;
            }
            const float falloff = 1.0f - (std::sqrt(distanceSquared) / radius);
            const float amount = falloff * amountScale;
            if (amount <= 0.0001f) {
                continue;
            }
            const auto beforeInfluences = document.vertexInfluences[vertex];
            const std::string beforeName = document.vertexBoneNames[vertex];
            AddVertexInfluence(document, vertex, target, amount);
            if (document.vertexInfluences[vertex] != beforeInfluences
                || document.vertexBoneNames[vertex] != beforeName) {
                ++changed;
            }
        }
        return changed;
    }
    const std::string target =
        mode == NativeSculptWeightPaint::Clear ? std::string{} : std::string(boneName);
    for (std::size_t vertex = 0; vertex < document.mesh.positions.size(); ++vertex) {
        if (!VertexInPaintRadius(document.mesh.positions[vertex], centers, radiusSquared)) {
            continue;
        }
        if (document.vertexBoneNames[vertex] == target
            && (target.empty()
                ? document.vertexInfluences[vertex].empty()
                : document.vertexInfluences[vertex].size() == 1U
                    && document.vertexInfluences[vertex].front().boneName == target
                    && document.vertexInfluences[vertex].front().weight > 0.99f)) {
            continue;
        }
        WriteRigidInfluence(document, vertex, target);
        ++changed;
    }
    return changed;
}

std::size_t FloodNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    const std::string_view boneName) {
    if (document.mesh.positions.empty() || boneName.empty()) {
        return 0;
    }
    EnsureVertexInfluences(document);
    const std::string target{boneName};
    std::size_t changed = 0;
    for (std::size_t vertex = 0; vertex < document.vertexBoneNames.size(); ++vertex) {
        if (document.vertexBoneNames[vertex] == target
            && document.vertexInfluences[vertex].size() == 1U
            && document.vertexInfluences[vertex].front().boneName == target) {
            continue;
        }
        WriteRigidInfluence(document, vertex, target);
        ++changed;
    }
    return changed;
}

std::size_t FloodUnboundNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    const std::string_view boneName) {
    if (document.mesh.positions.empty() || boneName.empty()) {
        return 0;
    }
    EnsureVertexInfluences(document);
    const std::string target{boneName};
    std::size_t changed = 0;
    for (std::size_t vertex = 0; vertex < document.vertexBoneNames.size(); ++vertex) {
        if (!document.vertexBoneNames[vertex].empty() || !document.vertexInfluences[vertex].empty()) {
            continue;
        }
        WriteRigidInfluence(document, vertex, target);
        ++changed;
    }
    return changed;
}

std::size_t TransferNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    const std::string_view fromBoneName,
    const std::string_view toBoneName) {
    if (document.mesh.positions.empty() || fromBoneName.empty() || toBoneName.empty()
        || fromBoneName == toBoneName) {
        return 0;
    }
    EnsureVertexInfluences(document);
    const std::string from{fromBoneName};
    const std::string to{toBoneName};
    std::size_t changed = 0;
    for (std::size_t vertex = 0; vertex < document.vertexBoneNames.size(); ++vertex) {
        bool touched = false;
        if (document.vertexBoneNames[vertex] == from) {
            document.vertexBoneNames[vertex] = to;
            touched = true;
        }
        auto& influences = document.vertexInfluences[vertex];
        float transferred = 0.0f;
        for (auto it = influences.begin(); it != influences.end();) {
            if (it->boneName != from) {
                ++it;
                continue;
            }
            transferred += it->weight;
            it = influences.erase(it);
            touched = true;
        }
        if (transferred > 0.0f) {
            bool merged = false;
            for (ri::content::NativeSculptVertexInfluence& influence : influences) {
                if (influence.boneName == to) {
                    influence.weight += transferred;
                    merged = true;
                    break;
                }
            }
            if (!merged) {
                if (influences.size()
                    < static_cast<std::size_t>(ri::content::NativeSculptDocument::kMaxInfluences)) {
                    influences.push_back(
                        ri::content::NativeSculptVertexInfluence{.boneName = to, .weight = transferred});
                } else if (!influences.empty()) {
                    influences.back().weight += transferred;
                    influences.back().boneName = to;
                } else {
                    WriteRigidInfluence(document, vertex, to);
                }
            }
            NormalizeVertexInfluences(influences, document.vertexBoneNames[vertex]);
        } else if (touched && influences.empty() && document.vertexBoneNames[vertex] == to) {
            WriteRigidInfluence(document, vertex, to);
        }
        if (touched) {
            ++changed;
        }
    }
    return changed;
}

std::size_t SwapNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    const std::string_view leftBoneName,
    const std::string_view rightBoneName) {
    if (document.mesh.positions.empty() || leftBoneName.empty() || rightBoneName.empty()
        || leftBoneName == rightBoneName) {
        return 0;
    }
    EnsureVertexInfluences(document);
    const std::string left{leftBoneName};
    const std::string right{rightBoneName};
    std::size_t changed = 0;
    for (std::size_t vertex = 0; vertex < document.vertexBoneNames.size(); ++vertex) {
        bool touched = false;
        if (document.vertexBoneNames[vertex] == left) {
            document.vertexBoneNames[vertex] = right;
            touched = true;
        } else if (document.vertexBoneNames[vertex] == right) {
            document.vertexBoneNames[vertex] = left;
            touched = true;
        }
        for (ri::content::NativeSculptVertexInfluence& influence : document.vertexInfluences[vertex]) {
            if (influence.boneName == left) {
                influence.boneName = right;
                touched = true;
            } else if (influence.boneName == right) {
                influence.boneName = left;
                touched = true;
            }
        }
        if (touched) {
            ++changed;
        }
    }
    return changed;
}

std::size_t InvertNativeSculptBoneWeights(
    ri::content::NativeSculptDocument& document,
    const std::string_view boneName) {
    if (document.mesh.positions.empty() || boneName.empty()) {
        return 0;
    }
    EnsureVertexInfluences(document);
    const std::string target{boneName};
    std::size_t changed = 0;
    for (std::size_t vertex = 0; vertex < document.vertexInfluences.size(); ++vertex) {
        auto& influences = document.vertexInfluences[vertex];
        float selectedWeight = 0.0f;
        bool found = false;
        for (const ri::content::NativeSculptVertexInfluence& influence : influences) {
            if (influence.boneName == target) {
                selectedWeight = influence.weight;
                found = true;
                break;
            }
        }
        if (!found) {
            if (document.vertexBoneNames[vertex] != target) {
                continue;
            }
            selectedWeight = 1.0f;
            found = true;
        }
        const float inverted = std::clamp(1.0f - selectedWeight, 0.0f, 1.0f);
        float otherSum = 0.0f;
        for (const ri::content::NativeSculptVertexInfluence& influence : influences) {
            if (influence.boneName != target) {
                otherSum += influence.weight;
            }
        }
        std::vector<ri::content::NativeSculptVertexInfluence> next{};
        next.reserve(influences.size());
        if (inverted > 0.000001f) {
            next.push_back(
                ri::content::NativeSculptVertexInfluence{.boneName = target, .weight = inverted});
        }
        const float remaining = std::clamp(1.0f - inverted, 0.0f, 1.0f);
        if (remaining > 0.000001f) {
            if (otherSum > 0.000001f) {
                const float scale = remaining / otherSum;
                for (const ri::content::NativeSculptVertexInfluence& influence : influences) {
                    if (influence.boneName == target) {
                        continue;
                    }
                    next.push_back(ri::content::NativeSculptVertexInfluence{
                        .boneName = influence.boneName,
                        .weight = influence.weight * scale,
                    });
                }
            } else if (inverted <= 0.000001f) {
                // Fully inverted rigid bind with no partners — leave unbound.
            }
        }
        influences = std::move(next);
        NormalizeVertexInfluences(influences, document.vertexBoneNames[vertex]);
        ++changed;
    }
    return changed;
}

std::size_t ScaleNativeSculptBoneWeights(
    ri::content::NativeSculptDocument& document,
    const std::string_view boneName,
    const float factor) {
    if (document.mesh.positions.empty() || boneName.empty() || !std::isfinite(factor) || factor <= 0.0f) {
        return 0;
    }
    EnsureVertexInfluences(document);
    const std::string target{boneName};
    std::size_t changed = 0;
    for (std::size_t vertex = 0; vertex < document.vertexInfluences.size(); ++vertex) {
        auto& influences = document.vertexInfluences[vertex];
        bool found = false;
        for (ri::content::NativeSculptVertexInfluence& influence : influences) {
            if (influence.boneName != target) {
                continue;
            }
            influence.weight *= factor;
            found = true;
            break;
        }
        if (!found) {
            if (document.vertexBoneNames[vertex] != target) {
                continue;
            }
            // Rigid bind with no influence list entry yet — scale from 1.0.
            if (factor >= 0.999f && factor <= 1.001f) {
                continue;
            }
            influences = {
                ri::content::NativeSculptVertexInfluence{.boneName = target, .weight = factor},
            };
            found = true;
        }
        if (!found) {
            continue;
        }
        NormalizeVertexInfluences(influences, document.vertexBoneNames[vertex]);
        ++changed;
    }
    return changed;
}

NativeSculptWeightAudit AuditNativeSculptWeights(const ri::content::NativeSculptDocument& document) {
    NativeSculptWeightAudit audit{};
    audit.vertexCount = document.mesh.positions.size();
    if (audit.vertexCount == 0U) {
        return audit;
    }
    const bool hasNames = document.vertexBoneNames.size() == audit.vertexCount;
    const bool hasInfluences = document.vertexInfluences.size() == audit.vertexCount;
    for (std::size_t vertex = 0; vertex < audit.vertexCount; ++vertex) {
        const bool boundByName = hasNames && !document.vertexBoneNames[vertex].empty();
        const auto& influences =
            hasInfluences ? document.vertexInfluences[vertex]
                          : std::vector<ri::content::NativeSculptVertexInfluence>{};
        const float sum = InfluenceWeightSum(influences);
        const bool bound = boundByName || sum > 0.000001f;
        if (bound) {
            ++audit.boundCount;
            if (influences.size() > 1U) {
                ++audit.blendedCount;
            }
            if (!influences.empty() && std::abs(sum - 1.0f) > 0.02f) {
                ++audit.nonNormalizedCount;
            }
        } else {
            ++audit.unboundCount;
        }
    }
    return audit;
}

std::size_t SmoothNativeSculptWeights(ri::content::NativeSculptDocument& document) {
    if (document.mesh.positions.empty()) {
        return 0;
    }
    EnsureVertexInfluences(document);
    const auto previous = document.vertexInfluences;
    const std::vector<std::vector<std::size_t>> adjacency = BuildVertexAdjacency(document.mesh);
    std::size_t changed = 0;
    for (std::size_t vertex = 0; vertex < document.mesh.positions.size(); ++vertex) {
        std::unordered_map<std::string, float> sums{};
        std::size_t sampleCount = 1;
        const auto accumulate = [&](const std::size_t index) {
            if (index >= previous.size()) {
                return;
            }
            for (const ri::content::NativeSculptVertexInfluence& influence : previous[index]) {
                sums[influence.boneName] += influence.weight;
            }
        };
        accumulate(vertex);
        for (const std::size_t neighbor : adjacency[vertex]) {
            accumulate(neighbor);
            ++sampleCount;
        }
        std::vector<ri::content::NativeSculptVertexInfluence> mixed{};
        mixed.reserve(sums.size());
        const float inv = 1.0f / static_cast<float>(sampleCount);
        for (const auto& [name, weight] : sums) {
            mixed.push_back(ri::content::NativeSculptVertexInfluence{
                .boneName = name,
                .weight = weight * inv,
            });
        }
        std::string dominant = document.vertexBoneNames[vertex];
        NormalizeVertexInfluences(mixed, dominant);
        if (mixed == document.vertexInfluences[vertex]
            && dominant == document.vertexBoneNames[vertex]) {
            continue;
        }
        document.vertexInfluences[vertex] = std::move(mixed);
        document.vertexBoneNames[vertex] = std::move(dominant);
        ++changed;
    }
    return changed;
}

std::size_t NormalizeAllNativeSculptWeights(ri::content::NativeSculptDocument& document) {
    if (document.mesh.positions.empty()) {
        return 0;
    }
    EnsureVertexInfluences(document);
    std::size_t changed = 0;
    for (std::size_t vertex = 0; vertex < document.mesh.positions.size(); ++vertex) {
        const auto before = document.vertexInfluences[vertex];
        const std::string beforeName = document.vertexBoneNames[vertex];
        NormalizeVertexInfluences(document.vertexInfluences[vertex], document.vertexBoneNames[vertex]);
        if (document.vertexInfluences[vertex] != before || document.vertexBoneNames[vertex] != beforeName) {
            ++changed;
        }
    }
    return changed;
}

std::size_t PruneAllNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    const float minWeight) {
    if (document.mesh.positions.empty() || minWeight <= 0.0f) {
        return 0;
    }
    EnsureVertexInfluences(document);
    std::size_t changed = 0;
    for (std::size_t vertex = 0; vertex < document.mesh.positions.size(); ++vertex) {
        const auto before = document.vertexInfluences[vertex];
        const std::string beforeName = document.vertexBoneNames[vertex];
        NormalizeVertexInfluences(
            document.vertexInfluences[vertex], document.vertexBoneNames[vertex], minWeight);
        if (document.vertexInfluences[vertex] != before || document.vertexBoneNames[vertex] != beforeName) {
            ++changed;
        }
    }
    return changed;
}

std::size_t MirrorNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    const bool mirrorX,
    const bool mirrorY,
    const bool mirrorZ) {
    if (document.mesh.positions.empty() || (!mirrorX && !mirrorY && !mirrorZ)) {
        return 0;
    }
    EnsureVertexInfluences(document);
    const float tolerance = MirrorMatchTolerance(document.mesh);
    const float toleranceSquaredValue = tolerance * tolerance;
    std::size_t changed = 0;
    for (std::size_t vertex = 0; vertex < document.mesh.positions.size(); ++vertex) {
        if (!IsSourceMirrorHalf(document.mesh.positions[vertex], mirrorX, mirrorY, mirrorZ)) {
            continue;
        }
        const std::optional<std::size_t> partner = FindMirrorPartnerVertex(
            document.mesh, vertex, mirrorX, mirrorY, mirrorZ, toleranceSquaredValue);
        if (!partner.has_value()) {
            continue;
        }
        auto mirrored = MirrorInfluenceList(document.vertexInfluences[vertex]);
        std::string dominant = document.vertexBoneNames[vertex];
        if (!mirrored.empty()) {
            dominant = SwapMirrorBoneName(dominant);
        } else {
            dominant.clear();
        }
        NormalizeVertexInfluences(mirrored, dominant);
        if (mirrored == document.vertexInfluences[*partner]
            && dominant == document.vertexBoneNames[*partner]) {
            continue;
        }
        document.vertexInfluences[*partner] = std::move(mirrored);
        document.vertexBoneNames[*partner] = std::move(dominant);
        ++changed;
    }
    return changed;
}

std::size_t RenameNativeSculptBone(
    ri::content::NativeSculptDocument& document,
    const std::string_view oldName,
    const std::string_view newName) {
    if (oldName.empty() || newName.empty() || oldName == newName) {
        return 0;
    }
    std::size_t changed = 0;
    for (std::string& boneName : document.vertexBoneNames) {
        if (boneName == oldName) {
            boneName = std::string(newName);
            ++changed;
        }
    }
    for (std::vector<ri::content::NativeSculptVertexInfluence>& influences : document.vertexInfluences) {
        for (ri::content::NativeSculptVertexInfluence& influence : influences) {
            if (influence.boneName == oldName) {
                influence.boneName = std::string(newName);
                ++changed;
            }
        }
    }
    return changed;
}

std::size_t RemoveNativeSculptBone(
    ri::content::NativeSculptDocument& document,
    const std::string_view boneName) {
    if (boneName.empty() || document.mesh.positions.empty()) {
        return 0;
    }
    EnsureVertexInfluences(document);
    const std::string target{boneName};
    std::size_t changed = 0;
    for (std::size_t vertex = 0; vertex < document.vertexBoneNames.size(); ++vertex) {
        if (document.vertexBoneNames[vertex] == target) {
            WriteRigidInfluence(document, vertex, "");
            ++changed;
            continue;
        }
        auto& influences = document.vertexInfluences[vertex];
        const std::size_t before = influences.size();
        influences.erase(
            std::remove_if(
                influences.begin(),
                influences.end(),
                [&](const ri::content::NativeSculptVertexInfluence& influence) {
                    return influence.boneName == target;
                }),
            influences.end());
        if (influences.size() == before) {
            continue;
        }
        NormalizeVertexInfluences(influences, document.vertexBoneNames[vertex]);
        ++changed;
    }
    return changed;
}

std::size_t ClearNativeSculptWeights(ri::content::NativeSculptDocument& document) {
    if (document.mesh.positions.empty()) {
        return 0;
    }
    EnsureVertexInfluences(document);
    std::size_t changed = 0;
    for (std::size_t vertex = 0; vertex < document.vertexBoneNames.size(); ++vertex) {
        if (document.vertexBoneNames[vertex].empty() && document.vertexInfluences[vertex].empty()) {
            continue;
        }
        WriteRigidInfluence(document, vertex, "");
        ++changed;
    }
    return changed;
}

std::size_t UnbindNativeSculptFromRig(ri::content::NativeSculptDocument& document) {
    const std::size_t cleared = ClearNativeSculptWeights(document);
    const bool hadRig = !document.rigPath.empty();
    document.rigPath.clear();
    return cleared + (hadRig ? 1U : 0U);
}

} // namespace ri::scene
