#pragma once

// The game's one sprite system (the user, 2026-11-10: "porque todo mapa demora tanto tempo carregando os personagens antes de abrir?
// porque eles nao estao carregados durante o load game?"). Every map made its own and threw it away when it closed: each one read
// the roster again, put the header and small version of all ~1100 clips on the GPU again, then every pose of its units, the
// party's included. Now one lives from the moment the GPU is up to the end: the roster, the headers and the small versions load
// while the title is up (its bar counts them), and a fight's characters stay on the GPU into the next map
// (SpriteSystem::beginFight), so a hero already loaded is never loaded again.

#include <cstdint>

#include "assets/AssetManager.h"
#include "render/Renderer.h"
#include "render/SpriteSystem.h"
#include "rhi/UploadQueue.h"

namespace ember {

class SharedSprites {
 public:
  SharedSprites(VulkanContext& context, Renderer& renderer, AssetManager& assets, uint64_t budgetMb)
      : uploads_(context), sprites_(context, renderer.bindless(), assets, uploads_, {budgetMb << 20}) {
    sprites_.start();
    slot() = this;
  }
  ~SharedSprites() {
    if (slot() == this) slot() = nullptr;
  }
  SharedSprites(const SharedSprites&) = delete;
  SharedSprites& operator=(const SharedSprites&) = delete;

  // Once per frame, before the screen's own frame (Services::frame): streaming, its uploads, and the renderer's wait on them.
  void frame(Renderer& renderer) {
    sprites_.update();
    uploads_.flush();
    renderer.waitForSprites(uploads_.timeline(), sprites_.uploadWaitValue());
  }
  SpriteSystem& sprites() { return sprites_; }

  // The one made by the game (nullptr before the GPU is up).
  static SharedSprites* get() { return slot(); }

 private:
  static SharedSprites*& slot() {
    static SharedSprites* s = nullptr;
    return s;
  }
  UploadQueue uploads_;
  SpriteSystem sprites_;
};

}  // namespace ember
