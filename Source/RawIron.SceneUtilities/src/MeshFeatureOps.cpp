#include "RawIron/Scene/MeshFeatureOps.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ri::scene {
namespace {
bool Finite(const ri::math::Vec3& p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
bool Finite(const ri::math::Vec2& p){return std::isfinite(p.x)&&std::isfinite(p.y);}
bool Bounded(const ri::math::Vec3& p){return Finite(p)&&std::abs(p.x)<=1e6f&&std::abs(p.y)<=1e6f&&std::abs(p.z)<=1e6f;}
void Validate(const Mesh& mesh){
    const auto count=mesh.positions.size();
    if(count==0 || count>1000000 || mesh.indices.size()>3000000 || mesh.geometryMode!=MeshGeometryMode::SurfaceTriangles
        || (!mesh.normals.empty()&&mesh.normals.size()!=count)
        || (!mesh.texCoords.empty()&&mesh.texCoords.size()!=count)
        || (!mesh.colors.empty()&&mesh.colors.size()!=count)
        || (!mesh.indices.empty()&&mesh.indices.size()%3!=0)
        || (mesh.indices.empty()&&count%3!=0)) throw std::invalid_argument("Invalid mesh feature streams/topology");
    for(const auto& p:mesh.positions)if(!Bounded(p))throw std::invalid_argument("Nonfinite or unbounded mesh position");
    for(const auto& n:mesh.normals)if(!Finite(n))throw std::invalid_argument("Nonfinite mesh normal");
    for(const auto& uv:mesh.texCoords)if(!Finite(uv))throw std::invalid_argument("Nonfinite mesh UV");
    for(const auto& color:mesh.colors)if(!Finite(color))throw std::invalid_argument("Nonfinite mesh color");
    for(int index:mesh.indices)if(index<0||static_cast<std::size_t>(index)>=count)throw std::invalid_argument("Invalid mesh index");
}
std::vector<int> Indices(const Mesh& mesh){
    if(!mesh.indices.empty())return mesh.indices;
    std::vector<int> result(mesh.positions.size());
    for(std::size_t i=0;i<result.size();++i)result[i]=static_cast<int>(i);
    return result;
}
struct Corner {ri::math::Vec3 p,n,color{1,1,1};ri::math::Vec2 uv;};
Corner Mix(const Corner& a,const Corner& b,float t){
    return {ri::math::Lerp(a.p,b.p,t),ri::math::Lerp(a.n,b.n,t),ri::math::Lerp(a.color,b.color,t),
        {a.uv.x+(b.uv.x-a.uv.x)*t,a.uv.y+(b.uv.y-a.uv.y)*t}};
}
void Normalise(ri::math::Vec3& n){
    n=ri::math::LengthSquared(n)>1e-12f?ri::math::Normalize(n):ri::math::Vec3{0,1,0};
}
}
Mesh TransformMeshUvs(const Mesh& source,const MeshUvTransform& transform){
    Validate(source);
    if(source.texCoords.empty()||!Finite(transform.offset)||!Finite(transform.repeat)||!Finite(transform.center)
        || !std::isfinite(transform.rotationRadians))throw std::invalid_argument("Invalid UV transform");
    Mesh result=source;result.primitive=PrimitiveType::Custom;
    const float c=std::cos(transform.rotationRadians),s=std::sin(transform.rotationRadians);
    for(auto& uv:result.texCoords){
        const float x=uv.x-transform.center.x,y=uv.y-transform.center.y;
        // Texture-space inverse rotation, matching Matrix3.setUvTransform semantics.
        uv={transform.repeat.x*(c*x+s*y)+transform.center.x+transform.offset.x,
            transform.repeat.y*(-s*x+c*y)+transform.center.y+transform.offset.y};
        if(!Finite(uv))throw std::invalid_argument("UV transform overflow");
    }
    return result;
}
Mesh BlendMeshMorphTargets(const Mesh& source,std::span<const MeshMorphTarget> targets,std::span<const float> weights){
    Validate(source);
    if(targets.size()!=weights.size()||targets.size()>64)throw std::invalid_argument("Morph target/weight mismatch");
    Mesh result=source;result.primitive=PrimitiveType::Custom;
    for(std::size_t target=0;target<targets.size();++target){
        if(targets[target].positions.size()!=source.positions.size()||!std::isfinite(weights[target]))
            throw std::invalid_argument("Invalid morph target");
        for(std::size_t i=0;i<source.positions.size();++i){
            if(!Bounded(targets[target].positions[i]))throw std::invalid_argument("Nonfinite or unbounded morph target");
            result.positions[i]=result.positions[i]+(targets[target].positions[i]-source.positions[i])*weights[target];
            if(!Bounded(result.positions[i]))throw std::invalid_argument("Morph overflow");
        }
    }
    const bool authoredNormals=std::any_of(targets.begin(),targets.end(),[](const auto& target){return !target.normals.empty();});
    if(authoredNormals){
        if(source.normals.size()!=source.positions.size())throw std::invalid_argument("Morph normals require base normals");
        result.normals=source.normals;
        for(std::size_t target=0;target<targets.size();++target){
            if(targets[target].normals.size()!=source.positions.size())throw std::invalid_argument("Incomplete morph normals");
            for(std::size_t i=0;i<source.positions.size();++i){
                const auto& n=targets[target].normals[i];
                if(!Bounded(n))throw std::invalid_argument("Invalid morph normal");
                result.normals[i]=result.normals[i]+(n-source.normals[i])*weights[target];
                if(!Bounded(result.normals[i]))throw std::invalid_argument("Morph normal overflow");
            }
        }
        for(auto& n:result.normals)Normalise(n);
        return result;
    }
    result.normals.assign(result.positions.size(),{});
    const auto indices=Indices(result);
    for(std::size_t i=0;i<indices.size();i+=3){
        const int a=indices[i],b=indices[i+1],c=indices[i+2];
        const auto n=ri::math::Cross(result.positions[b]-result.positions[a],result.positions[c]-result.positions[a]);
        result.normals[a]=result.normals[a]+n;result.normals[b]=result.normals[b]+n;result.normals[c]=result.normals[c]+n;
    }
    for(auto& n:result.normals)Normalise(n);
    return result;
}
Mesh ColorMeshByPosition(const Mesh& source,ri::math::Vec3 minimum,ri::math::Vec3 maximum){
    Validate(source);
    if(!Bounded(minimum)||!Bounded(maximum)||maximum.x<=minimum.x||maximum.y<=minimum.y||maximum.z<=minimum.z)
        throw std::invalid_argument("Invalid vertex color bounds");
    Mesh result=source;result.primitive=PrimitiveType::Custom;result.colors.clear();result.colors.reserve(source.positions.size());
    for(const auto& p:source.positions)result.colors.push_back({std::clamp((p.x-minimum.x)/(maximum.x-minimum.x),0.f,1.f),
        std::clamp((p.y-minimum.y)/(maximum.y-minimum.y),0.f,1.f),std::clamp((p.z-minimum.z)/(maximum.z-minimum.z),0.f,1.f)});
    return result;
}
Mesh ClipMeshPlanes(const Mesh& source,std::span<const MeshClipPlane> planes){
    Validate(source);
    if(planes.size()>8)throw std::invalid_argument("At most eight mesh clipping planes");
    for(const auto& plane:planes)if(!Bounded(plane.normal)||!std::isfinite(plane.constant)||std::abs(plane.constant)>1e6f
        ||ri::math::LengthSquared(plane.normal)<1e-12f)throw std::invalid_argument("Invalid mesh clip plane");
    if(planes.empty())return source;
    Mesh result{};result.name=source.name;result.primitive=PrimitiveType::Custom;
    const auto indices=Indices(source);
    const auto corner=[&](int i){return Corner{source.positions[i],source.normals.empty()?ri::math::Vec3{}:source.normals[i],
        source.colors.empty()?ri::math::Vec3{1,1,1}:source.colors[i],source.texCoords.empty()?ri::math::Vec2{}:source.texCoords[i]};};
    for(std::size_t i=0;i<indices.size();i+=3){
        std::vector<Corner> polygon{corner(indices[i]),corner(indices[i+1]),corner(indices[i+2])};
        for(const auto& plane:planes){
            std::vector<Corner> clipped;
            for(std::size_t edge=0;edge<polygon.size();++edge){
                const auto& a=polygon[edge];const auto& b=polygon[(edge+1)%polygon.size()];
                const float da=ri::math::Dot(plane.normal,a.p)+plane.constant,db=ri::math::Dot(plane.normal,b.p)+plane.constant;
                if(da>=0)clipped.push_back(a);
                if((da>=0)!=(db>=0))clipped.push_back(Mix(a,b,std::clamp(da/(da-db),0.f,1.f)));
            }
            polygon=std::move(clipped);
            if(polygon.empty())break;
        }
        for(std::size_t j=1;j+1<polygon.size();++j){
            if(ri::math::LengthSquared(ri::math::Cross(polygon[j].p-polygon[0].p,polygon[j+1].p-polygon[0].p))<1e-16f)continue;
            for(auto vertex:{polygon[0],polygon[j],polygon[j+1]}){
                if(result.positions.size()>=4000000)throw std::invalid_argument("Clipped mesh exceeds budget");
                result.indices.push_back(static_cast<int>(result.positions.size()));result.positions.push_back(vertex.p);
                if(!source.normals.empty()){Normalise(vertex.n);result.normals.push_back(vertex.n);}
                if(!source.texCoords.empty())result.texCoords.push_back(vertex.uv);
                if(!source.colors.empty())result.colors.push_back(vertex.color);
            }
        }
    }
    result.vertexCount=static_cast<int>(result.positions.size());result.indexCount=static_cast<int>(result.indices.size());
    return result;
}
} // namespace ri::scene
