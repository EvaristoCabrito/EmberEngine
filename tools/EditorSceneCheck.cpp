#include "world/Board.h"
#include "world/SceneBuilder.h"
#include <fstream>
#include <iostream>
#include <map>
int main() {
  using namespace ember;
  auto data=BoardData::load("content/data/board.json"); EditorCatalog catalog;
  std::ifstream in("build/editor/playtest/maps/custom-mw45tld8.json"); nlohmann::json saved; in>>saved;
  if(!data||!catalog.load("content/data/editor-catalog.json")||!saved.contains("draft")) return 1;
  auto draft=saved["draft"]; draft["decorations"]=nlohmann::json::array();
  auto sceneOf=[&](const nlohmann::json& j) {
    auto parsed=MapDraft::fromJson(j); if(!parsed) throw std::runtime_error("draft parse failed");
    Board board(*data,*parsed); board.loadRelief("content/assets");
    std::map<std::string,int> skipped; auto scene=buildScene(board,catalog,&skipped);
    if(!skipped.empty()) throw std::runtime_error("scene skipped objects");
    return scene;
  };
  draft["decorations"].push_back({{"id","prop-3d-tavern-barrel"},{"x",4},{"y",4},{"rot",0}});
  auto placed=sceneOf(draft); if(placed.models.size()!=1) return 2;
  draft["decorations"][0]["x"]=7;draft["decorations"][0]["y"]=6;draft["decorations"][0]["rot"]=1;
  auto moved=sceneOf(draft); if(moved.models.size()!=1 || (moved.models[0].x==placed.models[0].x && moved.models[0].z==placed.models[0].z) || moved.models[0].rotY==placed.models[0].rotY) return 3;
  auto roundtrip=nlohmann::json::parse(draft.dump()); auto reopened=sceneOf(roundtrip);
  if(reopened.models.size()!=1||reopened.models[0].x!=moved.models[0].x||reopened.models[0].z!=moved.models[0].z||reopened.models[0].rotY!=moved.models[0].rotY) return 4;
  draft["decorations"].erase(0);if(!sceneOf(draft).models.empty()) return 5;
  std::cout<<"PASS: real 3D object placed, moved, rotated, serialized/reopened and removed in scene builder\n";
}
