#include "RawIron/Games/CubeTest/CubeTestWorld.h"
#include "RawIron/Games/CubeTest/CubeTestGallery.h"
#include "RawIron/Scene/MeshFeatureOps.h"
#include "RawIron/Scene/NativeSculpt.h"
#include "RawIron/Scene/Helpers.h"
#include <cmath>
#include <stdexcept>

namespace ri::games::cubetest {
namespace {
constexpr int kFrames=32;
int Exhibit(CubeTestWorld& world,const char* room,const std::string& name,ri::scene::Mesh mesh,
    float z, bool textured=false){
    ri::scene::PrimitiveNodeOptions options{};
    options.nodeName=name;options.parent=world.rootNode;
    options.transform.position={FindCubeTestRoom(room)->centerX+1,1.7f,z};
    options.transform.scale={2.6f,2.6f,2.6f};
    options.materialName=name+"-material";
    options.shadingModel=ri::scene::ShadingModel::Unlit;
    options.roughness=.55f;options.doubleSided=true;
    if(std::string_view(room)=="morph-targets") {
        options.shadingModel=ri::scene::ShadingModel::Lit;
        options.baseColor={.12f,.52f,.82f};options.metallic=.12f;options.roughness=.34f;
    }
    const int node=ri::scene::AddPrimitiveNode(world.scene,options);
    mesh.name=name+"-mesh";
    world.scene.GetNode(node).mesh=world.scene.AddMesh(std::move(mesh));
    if(textured)world.scene.GetMaterial(world.scene.GetNode(node).material).baseColor={1,1,1};
    return node;
}
void Platform(CubeTestWorld& world,const char* id){
    ri::scene::PrimitiveNodeOptions platform{};
    platform.nodeName="CubeTest_"+std::string(id)+"Platform";platform.parent=world.rootNode;
    platform.primitive=ri::scene::PrimitiveType::Cube;
    platform.transform.position={FindCubeTestRoom(id)->centerX,-.12f,0};
    platform.transform.scale={16,.24f,16};platform.baseColor={.16f,.21f,.27f};
    platform.materialName=platform.nodeName+"-material";platform.roughness=.8f;
    ri::scene::AddPrimitiveNode(world.scene,platform);
    ri::scene::LightNodeOptions light{};light.parent=world.rootNode;
    light.nodeName=platform.nodeName+"-key";
    light.transform.position={FindCubeTestRoom(id)->centerX-3,5,-2};
    light.light.name=light.nodeName;light.light.type=ri::scene::LightType::Point;
    light.light.color={.9f,.95f,1};light.light.intensity=2.4f;light.light.range=14;
    ri::scene::AddLightNode(world.scene,light);
}
void Frames(CubeTestWorld& world,int node,std::vector<ri::scene::Mesh> meshes){
    CubeTestWorld::MeshFeatureAnimation animation{};animation.node=node;
    for(auto& mesh:meshes)animation.frames.push_back(world.scene.AddMesh(std::move(mesh)));
    world.scene.GetNode(node).mesh=animation.frames.front();
    world.meshFeatureAnimations.push_back(std::move(animation));
}
}
void AddCubeTestMeshFeatureRooms(CubeTestWorld& world,const std::filesystem::path& workspaceRoot){
    using namespace ri::scene;
    for(const char* id:{"vertex-colors","uv-transform","morph-targets","clipping"})Platform(world,id);
    const auto sphere=MakeNativeSculptCageMesh(NativeSculptCage::Sphere,32,16,"FeatureSphere");
    const auto cube=MakeNativeSculptCageMesh(NativeSculptCage::Cube,12,8,"FeatureCube");
    Exhibit(world,"vertex-colors","CubeTest_ColorWhite",sphere,-3.7f);
    const auto colored=ColorMeshByPosition(sphere,{-.5f,-.5f,-.5f},{.5f,.5f,.5f});
    Exhibit(world,"vertex-colors","CubeTest_ColorGradient",colored,0);
    auto inverted=colored;for(auto& color:inverted.colors)color={1-color.z,1-color.x,1-color.y};
    Exhibit(world,"vertex-colors","CubeTest_ColorChannels",std::move(inverted),3.7f);

    const auto texture=(workspaceRoot/"Games/CubeTest/assets/reference/threejs-r185/textures/uv_grid_opengl.jpg").string();
    const auto textured=[&](const char* name,Mesh mesh,float z){
        const int node=Exhibit(world,"uv-transform",name,std::move(mesh),z,true);
        world.scene.GetMaterial(world.scene.GetNode(node).material).baseColorTexture=texture;
        return node;
    };
    textured("CubeTest_UvIdentity",cube,-3.7f);
    textured("CubeTest_UvMirrored",TransformMeshUvs(cube,{{.2f,.1f},{-1.5f,2},{.5f,.5f},.785398163f}),3.7f);
    int uvNode=textured("CubeTest_UvAnimated",cube,0);
    std::vector<Mesh> uvFrames;
    for(int frame=0;frame<kFrames;++frame){
        const float phase=static_cast<float>(frame)/kFrames;
        uvFrames.push_back(TransformMeshUvs(cube,{{phase*.3f,0},{.5f,.5f},{.5f,.5f},phase*6.283185307f}));
    }
    Frames(world,uvNode,std::move(uvFrames));

    MeshMorphTarget round{},twist{};
    for(std::size_t i=0;i<cube.positions.size();++i){
        const auto& p=cube.positions[i];
        round.positions.push_back(ri::math::Normalize(p)*.62f);
        round.normals.push_back(ri::math::Normalize(p)*-1.f);
        const float a=p.y*2.2f,c=std::cos(a),s=std::sin(a);
        const ri::math::Vec3 q{c*p.x-s*p.z,p.y,s*p.x+c*p.z};
        twist.positions.push_back(q);
        const auto& n=cube.normals[i];
        const float nx=c*n.x-s*n.z,nz=s*n.x+c*n.z;
        twist.normals.push_back(ri::math::Normalize(ri::math::Vec3{nx,n.y+2.2f*(q.z*nx-q.x*nz),nz}));
    }
    const std::array<MeshMorphTarget,2> targets{round,twist};
    const int morphNode=Exhibit(world,"morph-targets","CubeTest_MorphAnimated",cube,0);
    Exhibit(world,"morph-targets","CubeTest_MorphBase",cube,-3.7f);
    const std::array<float,2> roundWeights{1,0};
    Exhibit(world,"morph-targets","CubeTest_MorphRound",BlendMeshMorphTargets(cube,targets,roundWeights),3.7f);
    std::vector<Mesh> morphFrames;
    for(int frame=0;frame<kFrames;++frame){
        const float phase=static_cast<float>(frame)/kFrames*6.283185307f;
        const std::array<float,2> weights{.5f-.5f*std::cos(phase),.25f-.25f*std::cos(phase*2)};
        morphFrames.push_back(BlendMeshMorphTargets(cube,targets,weights));
    }
    Frames(world,morphNode,std::move(morphFrames));

    Exhibit(world,"clipping","CubeTest_ClipWhole",colored,-3.7f);
    const std::array<MeshClipPlane,2> slice{{{{0,1,0},.15f},{{1,0,0},0}}};
    Exhibit(world,"clipping","CubeTest_ClipIntersection",ClipMeshPlanes(colored,slice),3.7f);
    const int clipNode=Exhibit(world,"clipping","CubeTest_ClipAnimated",colored,0);
    std::vector<Mesh> clipFrames;
    for(int frame=0;frame<kFrames;++frame){
        const float phase=static_cast<float>(frame)/kFrames*6.283185307f;
        const std::array<MeshClipPlane,1> plane{{{{0,1,0},.32f*std::cos(phase)}}};
        clipFrames.push_back(ClipMeshPlanes(colored,plane));
    }
    Frames(world,clipNode,std::move(clipFrames));
}
void AnimateCubeTestMeshFeatures(CubeTestWorld& world,double elapsedSeconds){
    if(!std::isfinite(elapsedSeconds))return;
    const int frame=world.featureFrameOverride>=0?world.featureFrameOverride:
        static_cast<int>(std::fmod(std::max(0.0,elapsedSeconds),kFrames/8.0)*8);
    for(const auto& animation:world.meshFeatureAnimations)
        world.scene.GetNode(animation.node).mesh=animation.frames[static_cast<std::size_t>(frame)%animation.frames.size()];
}
} // namespace ri::games::cubetest
