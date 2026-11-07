#include "world/Battle.h"
#include "world/BattleExport.h"
#include "world/Campaign.h"
#include "world/Inn.h"
#include "world/Overworld.h"
#include "world/Quests.h"
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace ember;
static void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
static Json read(const std::filesystem::path& p) { std::ifstream in(p); check(bool(in), "missing input"); return Json::parse(in); }
int main() try {
  Overworld world; check(world.load("content/data/overworld-half.json"), "world load");
  InnData data; check(data.load("content/data/inn.json"), "inn load");
  InnRules rules(data,world);
  const auto quests=world.tables()["quests"];
  Json fetch,boss;
  for(const auto& q:quests) { if(q.value("id","")=="deep-ashen-embers") fetch=q; if(q.value("id","")=="deep-ashen-matriarch") boss=q; }
  check(fetch.is_object() && boss.is_object(), "both quests registered");
  Json save=read("content/data/campaign.json")["emptySave"];
  save["flags"]=Json::array();
  for(const char* h:Overworld::kPartyHeroes) save["flags"].push_back(std::string("recruited:")+h);
  check(!rules.questOffered(save,"deep-ashen-matriarch"),"boss quest must require investigation");
  check(!rules.turnInQuest(save,"deep-ashen-matriarch"),"cannot claim early");
  auto accepted=rules.acceptQuest(save,"deep-ashen-embers"); check(bool(accepted),"accept investigation"); save=*accepted;
  check(questStatus(fetch,save)=="active","fetch starts active");
  std::filesystem::create_directories("work/deep-ashen-validation");
  std::optional<Battle> ravine,sanctuary;
  for(const char* stem:{"deep-ashen-forest006","deep-ashen-ravine002","deep-ashen-root-sanctuary002"}) {
    const auto source=read(std::string("content/maps/ember/")+stem+".json");
    std::string error; auto exported=exportBattle(source,&error,15);
    if(!exported) throw std::runtime_error(error);
    const auto path=std::filesystem::path("work/deep-ashen-validation")/(std::string(stem)+".battle.json");
    {std::ofstream out(path);out<<exported->dump(2);}
    auto battle=Battle::load(path);check(bool(battle),"exported battle loads");
    // Reach uses the real movement cells. Dead enemies remove occupancy, not terrain/blocking.
    for(auto& unit:battle->units()) if(unit.side=="enemy") unit.alive=false;
    size_t hero=0;while(hero<battle->units().size() && battle->units()[hero].side!="player") ++hero;
    check(hero<battle->units().size(),"party exists");
    const auto reach=battle->reachable(hero,false);
    for(const auto& deco:source["draft"]["decorations"]) if(deco.contains("targetMapId")) {
      bool found=false;for(const auto& c:reach) if(c.x==deco["x"] && c.y==deco["y"]) found=true;
      check(found,"floor connector must be reachable");
    }
    if(std::string(stem)=="deep-ashen-ravine002") ravine=std::move(battle);
    if(std::string(stem)=="deep-ashen-root-sanctuary002") sanctuary=std::move(battle);
  }
  auto pickups=activeQuestPickups(quests,save,"deep-ashen-ravine");check(pickups.size()==3,"three real pickups");
  ravine->setQuestPickups(pickups);
  size_t hero=0;while(ravine->units()[hero].side!="player")++hero;
  const auto reach=ravine->reachable(hero,false);
  for(const auto& p:pickups) {
    check(ravine->cell(p.x,p.y).passable,"pickup on passable cell");
    bool found=false;for(const auto& c:reach) if(c.x==p.x&&c.y==p.y)found=true;
    check(found,"pickup reachable from entry");
    ravine->units()[hero].x=p.x;ravine->units()[hero].y=p.y;
    check(ravine->collectQuestPickups([](size_t){return true;}).size()==1,"actual pickup collected");
  }
  foldQuestProgress(save,ravine->questFound(),{});
  check(questStatus(fetch,save)=="ready","three clues complete investigation");
  check(activeQuestPickups(quests,save,"deep-ashen-ravine").empty(),"collected clues stay collected across floors");
  auto examined=rules.turnInQuest(save,"deep-ashen-embers");check(bool(examined),"Dario examination completes quest");save=*examined;
  check(rules.questOffered(save,"deep-ashen-matriarch"),"Eda quest now available");
  accepted=rules.acceptQuest(save,"deep-ashen-matriarch");check(bool(accepted),"accept boss quest");save=*accepted;
  check(questStatus(boss,save)=="active","boss quest requires kill");
  auto kills=questKillTargets(quests,"deep-ashen-root-sanctuary");check(kills.size()==1 && kills[0]=="A Matriarca do Carvão","exact boss target");
  bool bossFound=false;
  for(const auto& unit:sanctuary->units()) if(unit.name==kills[0]) {bossFound=true;check(unit.classId=="asherah","white-haired woman class");}
  check(bossFound,"boss present in actual export");
  foldQuestProgress(save,{},kills);
  const int before=rules.bagCapacity(save);
  auto rewarded=rules.turnInQuest(save,"deep-ashen-matriarch");check(bool(rewarded),"Eda rewards completed quest");save=*rewarded;
  check(save.value("bagSlotsPerHeroBonus",0)==1,"persistent holding +1");
  check(rules.bagCapacity(save)==before+8,"each of eight recruited heroes gets one extra slot");
  check(!rules.turnInQuest(save,"deep-ashen-matriarch"),"reward cannot be duplicated");
  check(questStatus(boss,save)=="done","quest completion recorded");
  save["weapons"]=Json::object();save["equipped"]=Json::object();save["equipment"]=Json::object();save["looseEquipment"]=Json::object();save["rations"]=0;
  const int cap=rules.bagCapacity(save);
  for(int i=0;i<cap;++i)save["weapons"]["test-"+std::to_string(i)]=0;
  check(world.bagHasRoom(save,0),"inventory and admission agree at capacity");
  check(!world.bagHasRoom(save,1),"inventory admission rejects one over capacity");
  const auto roundtrip=Json::parse(save.dump());check(rules.bagCapacity(roundtrip)==cap,"bonus survives save/load");
  auto original=roundtrip;original.erase("bagSlotsPerHeroBonus");check(rules.bagCapacity(original)==cap-8,"old saves retain base capacity");
  std::cout<<"PASS: real exports load; exits and clues reachable; quest accept/collect/hand-in/kill/reward; +1 slot for each of 8 heroes; no duplicate reward; save roundtrip; inventory limits agree.\n";
  return 0;
} catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
