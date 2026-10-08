#include "RawIron/Scene/MeshFeatureOps.h"
#include "RawIron/Scene/GltfExporter.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/XR/HardwareSceneBuilder.h"
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
bool Near(float a,float b){return std::abs(a-b)<1e-5f;}
template<class F> void Reject(F action){bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}Require(rejected,"invalid feature request accepted");}
}
int main(){
    try{
        using namespace ri::scene;
        Mesh source{};source.primitive=PrimitiveType::Custom;source.name="FeatureTriangle";
        source.positions={{-1,-1,0},{1,-1,0},{0,1,0}};source.normals={{0,0,1},{0,0,1},{0,0,1}};
        source.texCoords={{0,0},{1,0},{.5f,1}};source.indices={0,1,2};source.colors={{1,0,0},{0,0,1},{0,1,0}};
        const auto identity=TransformMeshUvs(source,{});
        Require(identity.texCoords[1].x==1 && source.texCoords[1].x==1,"UV identity/source preservation");
        const auto rotation=TransformMeshUvs(source,{{0,0},{1,1},{.5f,.5f},1.570796327f});
        Require(Near(rotation.texCoords[1].x,0)&&Near(rotation.texCoords[1].y,0),"UV pivot/rotation analytic result");
        const auto mirrored=TransformMeshUvs(source,{{0,0},{-1,1},{.5f,.5f},0});
        Require(Near(mirrored.texCoords[0].x,1)&&Near(mirrored.texCoords[1].x,0),"mirrored repeat");
        Reject([&]{(void)TransformMeshUvs(source,{{0,0},{1,1},{0,0},std::numeric_limits<float>::infinity()});});
        MeshMorphTarget lift{},stretch{};for(auto p:source.positions){lift.positions.push_back(p+ri::math::Vec3{0,0,1});stretch.positions.push_back(p*2);}
        const std::array<MeshMorphTarget,2> targets{lift,stretch};
        const std::array<float,2> weights{.25f,.5f};const auto blend=BlendMeshMorphTargets(source,targets,weights);
        Require(Near(blend.positions[0].x,-1.5f)&&Near(blend.positions[0].z,.25f),"morph blend analytic result");
        Require(blend.indices==source.indices&&blend.texCoords[1].x==1&&blend.colors.size()==3,"morph attributes");
        Require(Near(ri::math::Length(blend.normals[0]),1),"morph normals unit length");
        const std::array<float,2> badWeights{std::numeric_limits<float>::quiet_NaN(),0};
        Reject([&]{(void)BlendMeshMorphTargets(source,targets,badWeights);});
        Require(source.positions[0].z==0,"morph source changed");
        auto normalTarget=lift;normalTarget.normals.assign(3,{0,1,0});
        const std::array<MeshMorphTarget,1> normalTargets{normalTarget};
        const std::array<float,1> normalWeight{.5f};
        const auto normalBlend=BlendMeshMorphTargets(source,normalTargets,normalWeight);
        Require(Near(normalBlend.normals[0].y,std::sqrt(.5f))&&Near(normalBlend.normals[0].z,std::sqrt(.5f)),"authored morph normal interpolation");
        auto incompleteNormals=targets;incompleteNormals[0].normals=normalTarget.normals;
        Reject([&]{(void)BlendMeshMorphTargets(source,incompleteNormals,weights);});
        const std::array<MeshClipPlane,1> plane{{{{1,0,0},0}}};const auto clipped=ClipMeshPlanes(source,plane);
        Require(clipped.positions.size()==3&&clipped.colors.size()==3,"clip topology and colors");
        bool intersection=false;
        for(std::size_t i=0;i<clipped.positions.size();++i){const auto& p=clipped.positions[i];
            Require(p.x>=-1e-6f,"clipping escaped kept half space");
            if(Near(p.x,0)&&Near(p.y,-1))intersection=Near(clipped.texCoords[i].x,.5f)&&Near(clipped.colors[i].x,.5f)&&Near(clipped.colors[i].z,.5f);
        }
        Require(intersection,"clip UV/color edge interpolation");
        const std::array<MeshClipPlane,1> outside{{{{1,0,0},-3}}};
        Require(ClipMeshPlanes(source,outside).positions.empty(),"fully removed geometry must be empty");
        const std::array<MeshClipPlane,1> invalid{{{{0,0,0},0}}};Reject([&]{(void)ClipMeshPlanes(source,invalid);});
        auto malformed=source;malformed.colors.pop_back();Reject([&]{(void)ClipMeshPlanes(malformed,plane);});
        Scene scene;const int node=scene.CreateNode("VertexColors");scene.GetNode(node).mesh=scene.AddMesh(source);
        Material material{};material.name="VertexMaterial";material.shadingModel=ShadingModel::Unlit;material.doubleSided=true;
        scene.GetNode(node).material=scene.AddMaterial(material);
        const int cameraNode=scene.CreateNode("ColorCamera");
        scene.GetNode(cameraNode).localTransform.position={0,0,-3};
        Camera camera{};scene.AttachCamera(cameraNode,scene.AddCamera(camera));
        ri::render::software::ScenePreviewOptions preview{};preview.width=96;preview.height=96;
        preview.fogStrength=0;preview.orderedDither=false;
        const auto rgb=ri::render::software::RenderScenePreview(scene,cameraNode,preview);
        scene.GetMesh(scene.GetNode(node).mesh).colors.clear();
        const auto white=ri::render::software::RenderScenePreview(scene,cameraNode,preview);
        Require(rgb.pixels!=white.pixels,"software renderer ignored vertex color stream");
        scene.GetMesh(scene.GetNode(node).mesh).colors=source.colors;
        ri::xr::HardwareTextureAtlas atlas;
        std::vector<ri::xr::HardwareSceneVertex> vertices;
        ri::xr::AppendHardwareMesh(vertices,source,material,scene.ComputeWorldMatrix(node),atlas);
        Require(vertices.size()==3 && vertices[0].color[0]==1 && vertices[0].color[2]==0
            && vertices[1].color[2]==1 && vertices[2].color[1]==1,"XR vertex color upload stream");
        const auto temp=std::filesystem::temp_directory_path()/
            ("rawiron-mesh-feature-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(temp);GltfExportReport report;std::string error;
        Require(ExportSceneToGltf(scene,temp/"feature.gltf",{},report,error),error.c_str());
        Scene imported;const int model=AddGltfModelNode(imported,{.sourcePath=temp/"feature.gltf"},&error);
        bool colorsRestored=false;for(std::size_t i=0;i<imported.MeshCount();++i){const auto& mesh=imported.GetMesh(static_cast<int>(i));
            if(mesh.colors.size()==3)colorsRestored=mesh.colors[0].x==1&&mesh.colors[1].z==1&&mesh.colors[2].y==1;
        }
        std::filesystem::remove_all(temp);
        Require(model!=kInvalidHandle && colorsRestored,"glTF COLOR_0 round trip");
        std::cout<<"Mesh features: UV math, morph blend, clipping, invalid requests and glTF colors passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
