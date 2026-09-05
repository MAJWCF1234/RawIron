#include "ProceduralSurfaceMesh.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
using namespace ri::scene;
using namespace ri::structural::detail;
using ri::math::Vec3;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool Near(Vec3 a, Vec3 b, float tolerance=1.e-5f) { return ri::math::Distance(a,b)<tolerance; }
void Validate(const Mesh& mesh) {
    Require(mesh.primitive==PrimitiveType::Custom && mesh.positions.size()==mesh.normals.size()
        && mesh.positions.size()==mesh.texCoords.size() && mesh.vertexCount==mesh.positions.size()
        && mesh.indexCount==mesh.indices.size() && mesh.indexCount%3==0,"mesh streams/counts");
    for (std::size_t i=0;i<mesh.positions.size();++i) {
        const auto p=mesh.positions[i], n=mesh.normals[i]; const auto uv=mesh.texCoords[i];
        Require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)
            && std::abs(ri::math::Length(n)-1)<.001f,"finite positions/unit normals");
        Require(uv.x>=0&&uv.x<=1&&uv.y>=0&&uv.y<=1,"UV range");
    }
    for (std::size_t i=0;i<mesh.indices.size();i+=3) {
        for (int k=0;k<3;++k) Require(mesh.indices[i+k]>=0 && mesh.indices[i+k]<mesh.vertexCount,"index range");
        const int a=mesh.indices[i], b=mesh.indices[i+1], c=mesh.indices[i+2];
        const auto face=ri::math::Cross(mesh.positions[b]-mesh.positions[a],mesh.positions[c]-mesh.positions[a]);
        Require(ri::math::Length(face)>1.e-7f,"nondegenerate triangle");
        Require(ri::math::Dot(face,mesh.normals[a]+mesh.normals[b]+mesh.normals[c])>0,"outward winding agrees with normals");
    }
}
template<class F> void Reject(F f) {
    bool rejected=false;
    try { f(); } catch (const std::invalid_argument&) { rejected=true; }
    Require(rejected,"invalid input must fail explicitly");
}
}
int main() try {
    const auto plane=BuildParametricMesh([](float u,float v){return Vec3{2*u,0,-3*v};},{4,6});
    Validate(plane);
    Require(plane.vertexCount==35 && plane.indexCount==144 && Near(plane.normals[0],{0,1,0}),"plane topology and orientation");
    const auto torus=BuildTorusMesh(2,.4f,48,16); Validate(torus);
    for (unsigned j=0;j<=16;++j) {
        Require(Near(torus.positions[j],torus.positions[48*17+j]) && Near(torus.normals[j],torus.normals[48*17+j]),"periodic U seam");
        Require(torus.texCoords[j].x==0 && torus.texCoords[48*17+j].x==1,"seam UV split");
    }
    for (unsigned i=0;i<=48;++i) Require(Near(torus.positions[i*17],torus.positions[i*17+16])
        && Near(torus.normals[i*17],torus.normals[i*17+16]),"periodic V seam");
    Require(Near(torus.positions[0],{2.4f,0,0}) && Near(torus.normals[0],{1,0,0},.001f),"analytic torus reference");
    const auto mobius=BuildMobiusMesh(2,.5f); Validate(mobius);
    for (int j=0;j<=16;++j) Require(Near(mobius.positions[j],mobius.positions[96*17+16-j]),"Mobius reverses its width at seam");

    const std::vector<ri::math::Vec2> profile{{1,0},{1,1},{1,3}};
    const auto lathe=BuildLatheMesh(profile,{12}); Validate(lathe);
    Require(lathe.vertexCount==39 && lathe.indexCount==144 && std::abs(lathe.texCoords[1].y-1.f/3)<1.e-6f,"lathe arc-length UV/counts");
    for (int j=0;j<3;++j) Require(Near(lathe.positions[j],lathe.positions[36+j]) && Near(lathe.normals[j],lathe.normals[36+j]),"lathe seam");
    const auto partial=BuildLatheMesh(profile,{12,0,std::numbers::pi_v<float>}); Validate(partial);
    Require(Near(partial.positions[36],{-1,0,0}),"partial sweep endpoint");

    const std::vector<Vec3> line{{0,0,0},{0,1,0},{0,3,0}};
    const auto capped=BuildTubeMesh(line,{.2f,8,false,true}); Validate(capped);
    Require(capped.vertexCount==47 && capped.indexCount==144 && Near(capped.normals[27],{0,-1,0})
        && Near(capped.normals[37],{0,1,0}),"tube cap topology and hard normals");
    const auto open=BuildTubeMesh(line,{.2f,8,false,false}); Validate(open);
    Require(open.vertexCount==27 && open.indexCount==96 && std::abs(open.texCoords[9].x-1.f/3)<1.e-6f,"tube side topology/arc UV");
    const std::vector<Vec3> controls{{0,0,0},{1,1,0},{2,0,1},{3,2,0}};
    const auto path=SampleCatmullRomPath(controls,60);
    Require(Near(path.front(),controls.front()) && Near(path.back(),controls.back()) && Near(path[20],controls[1]),"spline interpolates controls");
    Validate(BuildTubeMesh(path,{.1f,16}));
    const std::vector<Vec3> loop{{2,0,0},{0,1,2},{-2,0,0},{0,-.5f,-2}};
    const auto closedPath=SampleCatmullRomPath(loop,96,true);
    const auto closed=BuildTubeMesh(closedPath,{.15f,12,true,true}); Validate(closed);
    Require(closed.vertexCount==97*13 && closed.indexCount==96*12*6,"closed tube has no caps");
    for (int j=0;j<=12;++j) Require(Near(closed.positions[j],closed.positions[96*13+j])
        && Near(closed.normals[j],closed.normals[96*13+j]),"closed tube twist correction seam");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{(void)BuildParametricMesh({},{});});
    Reject([&]{(void)BuildParametricMesh([](float,float){return Vec3{};});});
    Reject([&]{(void)BuildParametricMesh([&](float,float){return Vec3{nan,0,0};});});
    Reject([&]{(void)BuildParametricMesh([](float u,float v){return Vec3{u,v,0};},{UINT_MAX,UINT_MAX});});
    Reject([&]{(void)BuildLatheMesh(profile,{2});});
    Reject([&]{(void)BuildLatheMesh(profile,{12,0,-1});});
    const std::vector<ri::math::Vec2> badProfile{{0,0},{1,1}};
    Reject([&]{(void)BuildLatheMesh(badProfile);});
    const std::vector<ri::math::Vec2> reversedProfile{{1,1},{1,0}};
    Reject([&]{(void)BuildLatheMesh(reversedProfile);});
    Reject([&]{(void)BuildTubeMesh(line,{nan});});
    Reject([&]{(void)BuildTubeMesh(line,{.2f,UINT_MAX});});
    Reject([&]{(void)BuildTubeMesh(line,{.2f,8,true});});
    const std::vector<Vec3> duplicate{{0,0,0},{0,0,0},{1,0,0}}, reversal{{0,0,0},{1,0,0},{0,0,0}};
    Reject([&]{(void)BuildTubeMesh(duplicate);});
    Reject([&]{(void)BuildTubeMesh(reversal);});
    Reject([&]{(void)SampleCatmullRomPath(duplicate,32);});
    Reject([&]{(void)SampleCatmullRomPath(controls,UINT_MAX);});
    Reject([&]{(void)BuildTorusMesh(1,2);});
    Reject([&]{(void)BuildMobiusMesh(1,nan);});
    std::cout << "Native procedural geometry: topology, winding, UVs, seams, caps, spline and invalid-input checks passed\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
