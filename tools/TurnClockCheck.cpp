#include "world/Battle.h"
#include "world/CampaignTime.h"
#include "world/Campaign.h"
#include "world/Overworld.h"
#include <iostream>
#include <stdexcept>
using namespace ember;
int main() {
  Battle b;
  BattleUnit u;
  u.id="test"; u.side="player"; u.alive=true;
  u.poisoned=true; u.poisonTier="lesser"; u.poisonMinutesLeft=2;
  u.diseased=true; u.diseaseMinutesLeft=3;
  u.atk=9; u.mag=9; u.def=9; u.dex=9; u.mov=1;
  u.diseaseBase=BattleUnit::Stats{10,10,10,10,2};
  b.units().push_back(u);
  b.startNewRound();
  if(b.round()!=2 || b.units()[0].poisonMinutesLeft!=1 || b.units()[0].diseaseMinutesLeft!=2) return 1;
  b.advanceConditionMinutes(1); // real-time exploration: no new combat round
  if(b.round()!=2 || b.units()[0].poisoned || !b.units()[0].diseased) return 2;
  b.advanceConditionMinutes(1);
  if(b.units()[0].diseased || b.units()[0].atk!=10 || b.units()[0].mov!=2) return 3;
  for(const auto tier:{"lesser","poison","greater","deadly","lethal"}) {
    auto& v=b.units()[0]; v.poisoned=true; v.poisonTier=tier; v.poisonMinutesLeft=-1;
    b.advanceConditionMinutes(ailmentDays(tier)*1440-1);
    if(!v.poisoned || v.poisonMinutesLeft!=1) return 4;
    b.startNewRound(); if(v.poisoned) return 5;
  }
  Campaign campaign;
  const auto bank=std::filesystem::path("work/tempo-validation/test-bank.json");
  if(!campaign.open("content/data/campaign.json",bank)) return 6;
  Json save={{"gameClock",0},{"gameHour",8},{"mapMode","rpg"},{"campaignId",111},{"weatherSeed",222},{"realPlaySeconds",0}};
  if(!campaign.writeSlot(0,save)) return 7;
  campaign.addRealPlaySeconds(3723.0);
  save["weatherSeed"]=333;
  if(!campaign.saveActive(save) || campaign.realPlaySeconds()!=3723.0) return 8;
  Campaign reloaded;
  if(!reloaded.open("content/data/campaign.json",bank) || reloaded.realPlaySeconds()!=3723.0) return 9;
  save["campaignId"]=444; save["realPlaySeconds"]=0;
  if(!campaign.writeSlot(0,save) || campaign.realPlaySeconds()!=0) return 10;
  Overworld world;
  if(!world.load(kOverworldData)) return 11;
  for(const auto& cell:world.cells()) if(cell.hours!=6) return 12;
  save=campaign.activeSave();
  campaign.addRealPlaySeconds(123);
  advanceCampaignMinutes(save,360);
  if(!campaign.saveActive(save) || campaign.realPlaySeconds()!=123) return 13;
  campaign.advanceClockTo(campaignMilliseconds(save)+60000);
  if(!campaign.saveActive(save) || campaignMinute(campaign.activeSave())!=campaignMinute(save)+1) return 14;
  std::cout<<"rounds/exploration, five poison powers, disease restoration, real hours saved/reloaded OK\n";
}
