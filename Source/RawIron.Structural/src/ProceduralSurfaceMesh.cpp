#include "ProceduralSurfaceMesh.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace ri::structural::detail {
namespace {
using ri::math::Vec3;
constexpr float tau = 2.0f * std::numbers::pi_v<float>;
constexpr std::size_t maxVertices = 1048576;
void Check(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
bool Finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
Vec3 Unit(Vec3 p) {
    const float length = ri::math::Length(p);
    Check(Finite(p) && std::isfinite(length) && length > 1.0e-7f, "Degenerate procedural tangent/normal");
    return p / length;
}
void Budget(std::size_t rows, std::size_t columns, std::size_t extra = 0) {
    Check(rows > 1 && columns > 1 && extra < maxVertices && rows <= (maxVertices - extra) / columns,
          "Procedural mesh exceeds vertex budget");
}
void Vertex(Mesh& mesh, Vec3 p, Vec3 n, float u, float v) {
    Check(Finite(p), "Non-finite procedural position");
    mesh.positions.push_back(p); mesh.normals.push_back(n); mesh.texCoords.push_back({u,v});
}
void GridIndices(Mesh& mesh, unsigned rows, unsigned columns) {
    for (unsigned i = 0; i + 1 < rows; ++i) for (unsigned j = 0; j + 1 < columns; ++j) {
        const int a = static_cast<int>(i * columns + j), b = a + static_cast<int>(columns);
        mesh.indices.insert(mesh.indices.end(), {a,b,a+1, b,b+1,a+1});
    }
}
Mesh Finish(Mesh mesh, const char* name) {
    mesh.name = name;
    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        const auto a = mesh.positions[mesh.indices[i]], b = mesh.positions[mesh.indices[i+1]], c = mesh.positions[mesh.indices[i+2]];
        const auto area = ri::math::Cross(b-a,c-a);
        Check(Finite(area) && ri::math::LengthSquared(area) > 1.0e-20f, "Degenerate procedural triangle");
    }
    return mesh;
}
Vec3 Rotate(Vec3 v, Vec3 axis, float angle) {
    return v * std::cos(angle) + ri::math::Cross(axis,v) * std::sin(angle)
        + axis * (ri::math::Dot(axis,v) * (1.0f-std::cos(angle)));
}
}

Mesh BuildParametricMesh(const SurfaceEvaluator& evaluate, ParametricMeshOptions o) {
    Check(evaluate && o.segmentsU >= 2 && o.segmentsV >= 2, "Invalid parametric evaluator/segments");
    Budget(static_cast<std::size_t>(o.segmentsU)+1, static_cast<std::size_t>(o.segmentsV)+1);
    const auto sample = [&](float u, float v) {
        if (o.periodicU) u -= std::floor(u);
        if (o.periodicV) v -= std::floor(v);
        const Vec3 p = evaluate(u,v);
        Check(Finite(p), "Non-finite parametric evaluator result");
        return p;
    };
    Mesh mesh;
    for (unsigned i = 0; i <= o.segmentsU; ++i) for (unsigned j = 0; j <= o.segmentsV; ++j) {
        const float u = static_cast<float>(i)/o.segmentsU, v = static_cast<float>(j)/o.segmentsV;
        const float su = o.periodicU && i == o.segmentsU ? 0.0f : u;
        const float sv = o.periodicV && j == o.segmentsV ? 0.0f : v;
        constexpr float h = 0.0005f;
        const float u0 = o.periodicU ? su-h : std::max(0.0f,su-h), u1 = o.periodicU ? su+h : std::min(1.0f,su+h);
        const float v0 = o.periodicV ? sv-h : std::max(0.0f,sv-h), v1 = o.periodicV ? sv+h : std::min(1.0f,sv+h);
        const auto du = Unit(sample(u1,sv)-sample(u0,sv)), dv = Unit(sample(su,v1)-sample(su,v0));
        Vertex(mesh, sample(su,sv), Unit(ri::math::Cross(du,dv)), u,v);
    }
    GridIndices(mesh,o.segmentsU+1,o.segmentsV+1);
    return Finish(std::move(mesh),"ParametricSurface");
}

Mesh BuildLatheMesh(std::span<const ri::math::Vec2> profile, LatheMeshOptions o) {
    Check(profile.size() >= 2 && o.radialSegments >= 3 && std::isfinite(o.startRadians)
        && std::isfinite(o.sweepRadians) && o.sweepRadians > 0 && o.sweepRadians <= tau,
        "Invalid lathe profile/segments/sweep");
    Budget(static_cast<std::size_t>(o.radialSegments)+1,profile.size());
    std::vector<float> length(profile.size(),0);
    for (std::size_t j = 0; j < profile.size(); ++j) {
        Check(std::isfinite(profile[j].x) && std::isfinite(profile[j].y) && profile[j].x > 0,
              "Lathe requires finite positive radii");
        if (j) {
            Check(profile[j].y > profile[j-1].y,"Lathe heights must increase strictly");
            length[j] = length[j-1] + std::hypot(profile[j].x-profile[j-1].x,profile[j].y-profile[j-1].y);
        }
    }
    Check(std::isfinite(length.back()),"Lathe profile length overflow");
    Mesh mesh;
    for (unsigned i = 0; i <= o.radialSegments; ++i) {
        const float u = static_cast<float>(i)/o.radialSegments;
        const float angle = o.startRadians + ((o.sweepRadians == tau && i == o.radialSegments) ? 0.0f : o.sweepRadians*u);
        for (std::size_t j = 0; j < profile.size(); ++j) {
            Vec3 tangent{};
            if (j) tangent = Unit({profile[j].x-profile[j-1].x,profile[j].y-profile[j-1].y,0});
            if (j+1 < profile.size()) tangent = tangent + Unit({profile[j+1].x-profile[j].x,profile[j+1].y-profile[j].y,0});
            tangent = Unit(tangent);
            const float c = std::cos(angle), s = std::sin(angle);
            Vertex(mesh,{profile[j].x*c,profile[j].y,profile[j].x*s},
                {tangent.y*c,-tangent.x,tangent.y*s},u,length[j]/length.back());
        }
    }
    GridIndices(mesh,o.radialSegments+1,static_cast<unsigned>(profile.size()));
    // dP/dangle cross dP/dheight points inward, so reverse each triangle.
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) std::swap(mesh.indices[i+1],mesh.indices[i+2]);
    return Finish(std::move(mesh),"Lathe");
}

std::vector<Vec3> SampleCatmullRomPath(std::span<const Vec3> points, unsigned segments, bool closed) {
    Check(points.size() >= (closed ? 3U : 2U) && points.size() <= maxVertices && segments >= 2 && segments < maxVertices,
          "Invalid Catmull-Rom points/segment budget");
    for (std::size_t i = 0; i < points.size(); ++i) {
        Check(Finite(points[i]),"Non-finite Catmull-Rom control point");
        if (i) (void)Unit(points[i]-points[i-1]);
    }
    if (closed) (void)Unit(points.front()-points.back());
    const int n = static_cast<int>(points.size());
    const auto at = [&](int i) {
        if (closed) return points[(i%n+n)%n];
        if (i < 0) return points[0]*2-points[1];
        if (i >= n) return points[n-1]*2-points[n-2];
        return points[i];
    };
    std::vector<Vec3> result;
    result.reserve(static_cast<std::size_t>(segments)+1);
    for (unsigned i = 0; i <= segments; ++i) {
        if (i == segments) { result.push_back(closed ? points.front() : points.back()); continue; }
        const float t = static_cast<float>(i)/segments * (closed ? n : n-1);
        const int k = static_cast<int>(t);
        const float f = t-k;
        const Vec3 a=at(k-1), b=at(k), c=at(k+1), d=at(k+2);
        const Vec3 p = (b*2 + (c-a)*f + (a*2-b*5+c*4-d)*(f*f) + (b*3-a-c*3+d)*(f*f*f))*.5f;
        Check(Finite(p),"Catmull-Rom evaluation overflow");
        result.push_back(p);
    }
    return result;
}

Mesh BuildTubeMesh(std::span<const Vec3> path, TubeMeshOptions o) {
    Check(path.size() >= (o.closed ? 4U : 2U) && std::isfinite(o.radius) && o.radius > 0 && o.radialSegments >= 3,
          "Invalid tube path/radius/segments");
    const std::size_t columns = static_cast<std::size_t>(o.radialSegments)+1;
    Budget(path.size(),columns,(!o.closed && o.capEnds) ? 2*(columns+1) : 0);
    if (o.closed) Check(ri::math::DistanceSquared(path.front(),path.back()) <= 1.0e-12f,"Closed tube must repeat first point");
    const std::size_t count = path.size();
    std::vector<Vec3> tangent(count), normal(count), binormal(count);
    std::vector<float> lengths(count,0);
    for (std::size_t i = 0; i < count; ++i) {
        Check(Finite(path[i]),"Non-finite tube path");
        if (i) { (void)Unit(path[i]-path[i-1]); lengths[i]=lengths[i-1]+ri::math::Distance(path[i],path[i-1]); }
        const Vec3 before = i ? path[i-1] : (o.closed ? path[count-2] : path[0]*2-path[1]);
        const Vec3 after = i+1 < count ? path[i+1] : (o.closed ? path[1] : path[i]*2-path[i-1]);
        tangent[i] = Unit(Unit(path[i]-before)+Unit(after-path[i]));
    }
    Check(std::isfinite(lengths.back()),"Tube path length overflow");
    const Vec3 axis = std::abs(tangent[0].y) < .9f ? Vec3{0,1,0} : Vec3{1,0,0};
    normal[0] = Unit(ri::math::Cross(tangent[0],axis));
    for (std::size_t i = 1; i < count; ++i) {
        const Vec3 cross = ri::math::Cross(tangent[i-1],tangent[i]);
        const float sine = ri::math::Length(cross), cosine = std::clamp(ri::math::Dot(tangent[i-1],tangent[i]),-1.0f,1.0f);
        Check(cosine > -.9999f,"Tube path reverses direction");
        normal[i] = sine > 1.0e-6f ? Rotate(normal[i-1],cross/sine,std::atan2(sine,cosine)) : normal[i-1];
        normal[i] = Unit(normal[i]-tangent[i]*ri::math::Dot(normal[i],tangent[i]));
    }
    if (o.closed) {
        const float correction = std::atan2(ri::math::Dot(tangent[0],ri::math::Cross(normal.back(),normal.front())),
                                            ri::math::Dot(normal.back(),normal.front()));
        for (std::size_t i = 1; i < count; ++i) normal[i] = Rotate(normal[i],tangent[i],correction*lengths[i]/lengths.back());
        normal.back() = normal.front(); tangent.back() = tangent.front();
    }
    Mesh mesh;
    for (std::size_t i = 0; i < count; ++i) {
        binormal[i] = Unit(ri::math::Cross(tangent[i],normal[i]));
        for (unsigned j = 0; j <= o.radialSegments; ++j) {
            const float v = static_cast<float>(j)/o.radialSegments;
            const float angle = j == o.radialSegments ? 0.0f : tau*v;
            const Vec3 n = normal[i]*std::cos(angle)+binormal[i]*std::sin(angle);
            Vertex(mesh,(o.closed && i+1 == count ? path.front() : path[i])+n*o.radius,n,lengths[i]/lengths.back(),v);
        }
    }
    GridIndices(mesh,static_cast<unsigned>(count),static_cast<unsigned>(columns));
    for (std::size_t i=0; i<mesh.indices.size(); i+=3) std::swap(mesh.indices[i+1],mesh.indices[i+2]);
    if (!o.closed && o.capEnds) for (const std::size_t ring : {std::size_t{0},count-1}) {
        const int base = static_cast<int>(mesh.positions.size());
        const Vec3 n = tangent[ring]*(ring ? 1.0f : -1.0f);
        Vertex(mesh,path[ring],n,.5f,.5f);
        for (unsigned j=0; j<=o.radialSegments; ++j) {
            const float a = j == o.radialSegments ? 0 : tau*static_cast<float>(j)/o.radialSegments;
            Vertex(mesh,mesh.positions[ring*columns+j],n,.5f+.5f*std::cos(a),.5f+.5f*std::sin(a));
        }
        for (unsigned j=0; j<o.radialSegments; ++j) {
            const int a=base+1+static_cast<int>(j), b=a+1;
            mesh.indices.insert(mesh.indices.end(),{base,ring ? a : b,ring ? b : a});
        }
    }
    return Finish(std::move(mesh),"Tube");
}

Mesh BuildTorusMesh(float radius, float tube, unsigned rings, unsigned sides) {
    Check(std::isfinite(radius) && std::isfinite(tube) && tube > 0 && radius > tube,"Invalid torus radii");
    auto mesh = BuildParametricMesh([=](float u,float v) {
        const float a=tau*u, b=tau*v, r=radius+tube*std::cos(b);
        return Vec3{r*std::cos(a),tube*std::sin(b),-r*std::sin(a)};
    },{rings,sides,true,true});
    mesh.name="Torus"; return mesh;
}
Mesh BuildMobiusMesh(float radius, float width, unsigned rings, unsigned sides) {
    Check(std::isfinite(radius) && std::isfinite(width) && width > 0 && radius > width,"Invalid Mobius dimensions");
    auto mesh = BuildParametricMesh([=](float u,float v) {
        const float a=tau*u, w=width*(2*v-1), r=radius+w*std::cos(a*.5f);
        return Vec3{r*std::cos(a),w*std::sin(a*.5f),r*std::sin(a)};
    },{rings,sides,false,false});
    mesh.name="Mobius"; return mesh;
}
CompiledMesh BuildSmoothStructuralSurface(std::string_view type, const StructuralPrimitiveOptions& o) {
    try {
        Mesh mesh;
        if (type == "revolve") {
            std::vector<ri::math::Vec2> profile;
            for (auto p : o.points) profile.push_back({p.x,p.y});
            if (profile.empty()) profile={{.25f,-.5f},{.4f,-.25f},{.3f,.15f},{.2f,.5f}};
            mesh=BuildLatheMesh(profile,{static_cast<unsigned>(o.radialSegments),o.startDegrees*tau/360,
                                        o.sweepDegrees == 360 ? tau : o.sweepDegrees*tau/360});
        } else if (type == "spline_sweep") {
            auto points=o.points;
            if (points.empty()) points=o.closedPath
                ? std::vector<Vec3>{{.4f,0,0},{0,.3f,.4f},{-.4f,0,0},{0,-.3f,-.4f}}
                : std::vector<Vec3>{{-.5f,0,-.5f},{.5f,0,.5f}};
            if (o.closedPath && points.size()>2 && ri::math::DistanceSquared(points.front(),points.back())<1.e-12f) points.pop_back();
            mesh=BuildTubeMesh(SampleCatmullRomPath(points,static_cast<unsigned>(o.pathSegments),o.closedPath),
                               {o.thickness,static_cast<unsigned>(o.sides),o.closedPath,o.capEnds});
        } else if (type == "torus") {
            const float radius=std::clamp(o.thickness>0 ? o.thickness : .14f,.03f,.22f);
            mesh=BuildTorusMesh(.5f-radius,radius,static_cast<unsigned>(o.radialSegments),static_cast<unsigned>(o.sides));
            const float angle=o.startDegrees*tau/360, c=std::cos(angle), s=std::sin(angle);
            for (auto* stream : {&mesh.positions,&mesh.normals}) for (auto& p : *stream)
                p={p.x*c-p.z*s,p.y,p.x*s+p.z*c};
        } else if (type == "mobius") {
            mesh=BuildMobiusMesh(.35f,o.thickness,static_cast<unsigned>(o.radialSegments),static_cast<unsigned>(o.sides));
        } else if (type == "parametric_patch") {
            const int cellsX = o.cellsX == 0 && o.vertices.empty() ? 32 : o.cellsX;
            const int cellsY = o.cellsY == 0 && o.vertices.empty() ? 32 : o.cellsY;
            Check(cellsX>=2 && cellsY>=2,"Parametric patch requires cellsX/cellsY >= 2");
            Budget(static_cast<std::size_t>(cellsX)+1,static_cast<std::size_t>(cellsY)+1);
            const auto count=(static_cast<std::size_t>(cellsX)+1)*(static_cast<std::size_t>(cellsY)+1);
            Check(o.vertices.empty() || o.vertices.size()==count,"Parametric patch lattice size mismatch");
            mesh=BuildParametricMesh([&](float u,float v) {
                if (o.vertices.empty()) {
                    const float x=u-.5f,z=v-.5f;
                    return Vec3{x,o.depth*(x*x-z*z),-z};
                }
                const float x=u*cellsX,y=v*cellsY;
                const int ix=std::min(static_cast<int>(x),cellsX-1),iy=std::min(static_cast<int>(y),cellsY-1);
                const auto at=[&](int a,int b){return o.vertices[static_cast<std::size_t>(a)*(cellsY+1)+b];};
                return ri::math::Lerp(ri::math::Lerp(at(ix,iy),at(ix+1,iy),x-ix),
                                      ri::math::Lerp(at(ix,iy+1),at(ix+1,iy+1),x-ix),y-iy);
            },{static_cast<unsigned>(cellsX),static_cast<unsigned>(cellsY)});
        } else return {};
        // The structural compiler keeps triangle soup. Carry smooth normals and
        // UV splits through its existing payload instead of projecting them away.
        CompiledMesh result;
        result.positions.reserve(mesh.indices.size());
        result.normals.reserve(mesh.indices.size());
        result.texCoords.reserve(mesh.indices.size());
        for (const int index : mesh.indices) {
            const auto p=mesh.positions[index];
            result.positions.push_back(p); result.normals.push_back(mesh.normals[index]); result.texCoords.push_back(mesh.texCoords[index]);
            if (!result.hasBounds) { result.boundsMin=p; result.boundsMax=p; result.hasBounds=true; }
            result.boundsMin={std::min(result.boundsMin.x,p.x),std::min(result.boundsMin.y,p.y),std::min(result.boundsMin.z,p.z)};
            result.boundsMax={std::max(result.boundsMax.x,p.x),std::max(result.boundsMax.y,p.y),std::max(result.boundsMax.z,p.z)};
        }
        result.triangleCount=result.positions.size()/3;
        return result;
    } catch (const std::invalid_argument&) {
        // Existing structural API reports failures as empty output; validation
        // turns this into an authoring error. Never substitute a different shape.
        return {};
    }
}
} // namespace ri::structural::detail
