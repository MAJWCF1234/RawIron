#include "RawIron/Scene/Scene.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}
template<class F> void Reject(F action) {
    bool rejected=false;
    try {action();} catch(const std::logic_error&) {rejected=true;}
    Require(rejected,"malformed hierarchy was not diagnosed");
}
}
int main() {
    try {
        ri::scene::Scene scene("Deep hierarchy");
        constexpr int depth=12000;
        int parent=ri::scene::kInvalidHandle;
        for(int i=0;i<depth;++i) {
            parent=scene.CreateNode("chain",parent);
            scene.GetNode(parent).localTransform.position={1,0,0};
        }
        Require(scene.ComputeWorldPosition(parent).x==depth,"deep hierarchy transform incorrect");
        Require(scene.ComputeWorldPosition(parent).x==depth,"cached transform incorrect");
        scene.GetNode(0).localTransform.position.x=2;
        Require(scene.ComputeWorldPosition(parent).x==depth+1,"deep hierarchy cache did not invalidate");

        ri::scene::Scene malformed("Malformed hierarchy");
        const int root=malformed.CreateNode("root");
        const int child=malformed.CreateNode("child",root);
        const int other=malformed.CreateNode("other");
        malformed.GetNode(root).parent=child;
        Reject([&]{(void)malformed.ComputeWorldMatrix(child);});
        Require(!malformed.SetParent(other,root),"reparent traversed an existing cycle");
        Require(malformed.SetParent(root,ri::scene::kInvalidHandle),"cycle could not be detached");
        Require(malformed.ComputeWorldPosition(child).x==0,"cycle diagnosis poisoned transform cache");
        malformed.GetNode(child).parent=999999;
        Reject([&]{(void)malformed.ComputeWorldMatrix(child);});
        Require(!malformed.SetParent(other,child),"reparent accepted an invalid ancestor");
        Require(malformed.SetParent(child,root),"malformed parent could not be repaired");
        const auto& children=static_cast<const ri::scene::Scene&>(malformed).GetNode(root).children;
        Require(std::count(children.begin(),children.end(),child)==1,"repair duplicated child references");
        Require(malformed.ComputeWorldPosition(child).x==0,"repaired transform incorrect");
        std::cout<<"12000-node hierarchy, cache invalidation, cycles, invalid parents and repair passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
