#include "assets/AssetManager.h"
#include "core/Jobs.h"
#include "render/MeshScene.h"
#include "render/Renderer.h"
#include "rhi/Bindless.h"
#include "rhi/UploadQueue.h"
#include "rhi/VulkanContext.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
int main(int argc, char** argv) {
  using namespace ember;
  if (argc < 3) return 2;
  VulkanContext context({true, nullptr});
  JobSystem jobs(2); IoQueue io(jobs); AssetManager assets(jobs, io, {});
  if (!assets.mount(argv[1])) return 2;
  Renderer renderer(context,nullptr,{512,512}); UploadQueue uploads(context);
  MeshScene meshes(context, renderer.bindless(), assets, uploads, {.geometryBytes=256ull<<20});
  meshes.setRegionStreaming(true); meshes.setStreamGate(true); meshes.openStreaming();
  std::vector<MeshId> ids;
  for(int i=2;i<argc;++i) ids.push_back(meshes.add(assetId(argv[i])));
  bool placed=false, earlyPlacement=false;
  const auto start=std::chrono::steady_clock::now();
  for(unsigned frame=0;std::chrono::steady_clock::now()-start<std::chrono::seconds(20);++frame) {
    assets.update(); meshes.update();
    const bool parsed=std::all_of(ids.begin(),ids.end(),[&](MeshId id){return meshes.header(id)!=nullptr;});
    if(parsed && meshes.stats().meshesReady<ids.size()) {
      if(!meshes.placeable()) {std::cerr<<"FAIL: parsed models blocked by GPU residency\n"; return 5;}
      earlyPlacement=true;
    }
    if(!placed && meshes.placeable()) {
      std::vector<InstanceGpu> instances;
      for(auto id:ids) {
        InstanceGpu inst{}; inst.rows[0]={1,0,0,0};inst.rows[1]={0,1,0,0};inst.rows[2]={0,0,1,0};inst.mesh=id;inst.scale=1;
        instances.push_back(inst);
      }
      meshes.setInstances(instances); placed=true;
    }
    uploads.flush(); VK_CHECK(vkQueueWaitIdle(context.transferQueue));
    if(placed && meshes.view().instanceCount==ids.size()) {
      const auto* h=meshes.header(ids.front());
      const glm::vec3 centre{h->boundsCenter[0],h->boundsCenter[1],h->boundsCenter[2]};
      const float radius=std::max(h->boundsRadius,1.0f);
      const auto camera=Camera::lookAt(centre+glm::vec3(1,0.7f,1)*radius*2.5f,centre,glm::radians(45.0f),1);
      renderer.setScene(camera,{});
      for(int n=0;n<16;++n) {
        assets.update();meshes.update();uploads.flush();VK_CHECK(vkQueueWaitIdle(context.transferQueue));
        renderer.setMeshes(meshes.view());renderer.renderFrame(0);
      }
      VK_CHECK(vkDeviceWaitIdle(context.device));
      renderer.renderFrame(0);
      const auto draw=renderer.meshStats();
      std::cout<<"DRAW: visible="<<draw.visibleInstances<<" clusters="<<draw.clusters<<" triangles="<<draw.triangles<<"\n";
      if(!draw.clusters) return 4;
      if(!earlyPlacement) {std::cerr<<"FAIL: early placement was not exercised\n";return 6;}
      std::cout<<"PASS: parsed models placed before GPU residency; "<<ids.size()<<" real models reached GPU instance list; validation errors="<<context.validationErrors()<<"\n";
      return context.validationErrors()?3:0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  const auto s=meshes.stats();
  std::cerr<<"FAIL: placed="<<placed<<" meshes="<<s.meshesReady<<"/"<<s.meshes<<" textures="<<s.texturesReady<<"/"<<s.textures<<" instances="<<meshes.view().instanceCount<<"\n";
  return 1;
}
