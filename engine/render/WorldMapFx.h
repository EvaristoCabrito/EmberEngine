#pragma once

// Mapa-múndi e Submundo: a luz e a névoa das duas telas do mapa (apps/sandbox/OverworldStage.h), um sistema só delas, sem
// relação com a névoa, o céu e a atmosfera das batalhas. A pintura 2D é desenhada por shaders/worldmap.hlsl dentro do passe
// da UI (um callback do ImDrawList, no lugar da imagem da UI): a luz por hora do relógio da campanha (mapa-múndi) ou o
// ciclo dos seres que brilham (Submundo), a névoa de guerra viva, a névoa baixa / de caverna e o acabamento de cor.
// Os valores ajustáveis: content/worldmap/overworld-light-fog.json e underworld-light-fog.json (WorldMapConfig).

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <imgui.h>

#include "rhi/Image.h"
#include "rhi/VkCommon.h"

namespace ember {

class VulkanContext;
class Bindless;

// Os 24 float4 lidos pelo shader (worldmap.hlsl Params), já interpolados pela hora.
struct WorldMapParams {
  glm::vec4 tint, grade, light, vignette, fogColor, fogShape, lowColor, lowShape, glowColor, dark, caveColor, caveShape;
  glm::vec4 extra0{};
  // Mapa-múndi, o look 2D (2026-10-28; worldmap.hlsl diz cada campo): 13..21
  // (o Submundo lê 13..22 com significado próprio: worldmap.hlsl underworldLook)
  glm::vec4 hourMix{}, sun{}, lightColor{}, clouds{}, mistShape{}, mistColor{}, water{}, party{}, moon{};
  glm::vec4 creatures{};
  glm::vec4 spare{};
};
static_assert(sizeof(WorldMapParams) == 24 * 16);

// O arquivo de configuração de uma tela (content/worldmap/*.json). Um valor ausente fica no padrão daqui.
struct WorldMapConfig {
  bool enabled = true;
  std::string mask;  // a máscara pintada (PNG, R = névoa baixa / de caverna, G = seres que brilham); vazio = heurística no shader
  // Mapa-múndi, o look 2D (o usuário, 2026-10-28: "um 2D de qualidade superior"): as LUTs por hora e as máscaras pintadas
  // pelo usuário (PNG na proporção da pintura; vazio = aquela parte fica desligada).
  struct LutKey {
    float hour = 12;
    std::string file;
  };
  std::vector<LutKey> luts;  // "lut": [{"hour", "file"}], cíclico em 24 h, misturadas duas a duas
  float lutStrength = 1.0f;
  std::string lightMask, heightMap, waterMask, mistMask;
  // a noite (0..1) e o dia (0..1) pela hora: rampas entre estas horas
  float nightFrom = 19.5f, nightFull = 21.0f, dayFrom = 5.0f, dayFull = 7.0f;
  float mistDawnFrom = 4.0f, mistDawnPeak = 6.0f, mistDawnEnd = 9.0f, mistNight = 0.6f;  // a névoa da hora: pico na madrugada, parte da noite
  float reliefStrength = 0.35f, sunLean = 0.45f;  // o relevo; quanto o sol pende para o sul (y da pintura) ao meio-dia
  glm::vec3 lightColor{1.0f, 0.55f, 0.28f};
  float lightStrength = 1.0f, lightFlicker = 0.5f;
  int lightGlowRadius = 8, lightGlowDownsample = 4;  // o borrado do halo (em pixels da cópia reduzida)
  float cloudStrength = 0.12f, cloudScale = 3.0f, cloudSpeed = 0.012f;
  float mistScale = 14.0f, mistSpeed = 0.02f, mistThreshold = 0.36f, mistStrength = 0.55f;
  glm::vec3 mistColor{0.78f, 0.82f, 0.90f};
  float waterShimmer = 0.6f, waterMoon = 0.5f, waterScale = 60.0f, waterSpeed = 0.05f;
  glm::vec3 moonColor{0.55f, 0.65f, 0.90f};
  float partyRadius = 0.08f, partyDark = 0.45f;  // a luz do grupo à noite (Opções): o raio em uv, quanto escurece o resto
  float lightsFrom = 18.0f, lightsFull = 19.5f;  // as luzes acendem no entardecer (apagam com dayFrom..dayFull)
  glm::vec3 waterDayColor{1.0f, 0.97f, 0.90f}, waterDuskColor{1.0f, 0.55f, 0.35f};  // o reflexo pela luz principal da hora
  float duskFrom = 17.0f, duskPeak = 18.5f, duskEnd = 20.0f;
  // Submundo, o look 2D no ciclo invertido (2026-10-28): "lut" keyed by the SURFACE hour; the masks (PNG at the atlas's aspect).
  std::string fungalMask, plantMask, windowMask, emberMask, heatMask, sporeMask;  // + waterMask above
  glm::vec3 fungalColor{0.30f, 0.85f, 0.80f};
  float fungalStrength = 1.0f, breathe = 0.5f;
  glm::vec3 plantColor{0.55f, 0.35f, 0.85f};
  float plantStrength = 0.6f;
  glm::vec3 windowColor{1.0f, 0.72f, 0.40f};
  float windowStrength = 0.9f;
  glm::vec3 emberColor{1.0f, 0.32f, 0.10f};
  float emberStrength = 1.0f, emberThrob = 0.35f, emberThrobSpeed = 1.3f, heatAmplitude = 1.5f, heatSpeed = 0.6f;
  glm::vec3 underWaterTint{0.35f, 0.80f, 0.85f};
  float sporeScale = 90.0f, sporeSpeed = 0.01f, sporeSize = 0.08f, sporeStrength = 0.5f;
  glm::vec3 creatureColor{0.75f, 0.95f, 0.60f};
  float creatureStrength = 0.6f;
  // Mapa-múndi: uma chave por hora (0..24, cíclico), interpolada suavemente.
  struct HourKey {
    float hour = 12;
    glm::vec3 tint{1, 1, 1};
    float exposure = 1;
    float contrast = 1.1f, saturation = 1, lift = 0, gamma = 1;
    glm::vec2 lightDir{-0.6f, -0.8f};
    float relief = 0.6f, rays = 0;
    glm::vec3 fogColor{0.05f, 0.05f, 0.06f};
    glm::vec3 lowColor{0.5f, 0.55f, 0.6f};
    float lowStrength = 0.5f;
  };
  std::vector<HourKey> keys;
  float fogDensity = 0.92f, fogScale = 9, fogWind = 0.02f, fogEdge = 0.22f, fogShade = 0.35f;
  float lowScale = 18, lowSpeed = 0.025f, lowHeight = 1, lowThreshold = 0.32f;
  float vignette = 0.45f, vignetteRadius = 0.55f, vignetteSoft = 0.6f, shoulder = 1;
  float hourEase = 2.5f;  // segundos para a luz passar de uma hora à outra na tela (sem saltos)
  // Submundo: o ciclo dos seres (horas do relógio da campanha) e as cores.
  float wakeHour = 6, riseHours = 2, glowHours = 8, fadeHours = 3, periodHours = 20;
  glm::vec3 glowColor{0.35f, 0.9f, 0.8f};
  float glowRadius = 0.02f;
  glm::vec3 darkColor{0.45f, 0.5f, 0.6f};
  float darkLevel = 0.12f;
  float peakContrast = 1.15f, peakSaturation = 1.05f, sleepSaturation = 0.7f, glowAmbient = 0.35f;
  glm::vec3 caveColor{0.2f, 0.24f, 0.28f};
  float caveDensity = 0.75f, caveScale = 10, caveSpeed = 0.012f, caveGlow = 1.2f, caveThreshold = 0.3f;

  static WorldMapConfig load(const std::filesystem::path& file, bool underworld);
};

class WorldMapFx {
 public:
  WorldMapFx(VulkanContext& context, Bindless& bindless);
  ~WorldMapFx();
  WorldMapFx(const WorldMapFx&) = delete;
  WorldMapFx& operator=(const WorldMapFx&) = delete;

  // Uma textura de um arquivo (PNG / JPG, RGBA8), subida de uma vez (immediateSubmit: a tela abre sob a cortina de carregamento).
  // kNoTexture quando o arquivo não pode ser lido.
  uint32_t loadTexture(const std::filesystem::path& file);
  // A máscara de luzes e a sua cópia borrada (o halo): a imagem reduzida `downsample` vezes e borrada `radius` pixels (caixa,
  // três passes), subida ao lado da nítida. {kNoTexture, kNoTexture} se o arquivo não pode ser lido.
  std::pair<uint32_t, uint32_t> loadLightMask(const std::filesystem::path& file, int downsample, int radius);
  // O revelado do mapa-múndi (R8, 1 = explorado), refeito só quando os hexágonos explorados mudam (o chamador decide).
  void setReveal(const std::vector<uint8_t>& texels, uint32_t w, uint32_t h);
  static constexpr uint32_t kNoTexture = 0xFFFFFFFFu;

  // O quadro: a pintura `map` no retângulo `rect` da UI (unidades do canvas), recortada em `clip`, no viewport `view`.
  struct Draw {
    ImVec2 rectMin, rectMax, viewMin, viewMax;
    uint32_t map = kNoTexture, mask = kNoTexture;
    bool underworld = false, fogOfWar = true;
    int quality = 2;  // 0 baixa, 1 média, 2 alta (Qualidade gráfica)
    float time = 0, rounding = 8, glow = 0;
    // O look 2D: as LUTs da hora e as texturas de cada tela (worldmap.hlsl Push diz o que é cada tex[i] em cada tela;
    // kNoTexture = aquela parte desligada) e a luz do grupo (Opções, mapa-múndi).
    static constexpr int kTextures = 9;
    uint32_t lutA = kNoTexture, lutB = kNoTexture;
    uint32_t tex[kTextures] = {kNoTexture, kNoTexture, kNoTexture, kNoTexture, kNoTexture, kNoTexture, kNoTexture, kNoTexture, kNoTexture};
    bool partyLight = false;
    WorldMapParams params;
  };
  void draw(ImDrawList* dl, const Draw& d);

  // Mapa-múndi: os parâmetros da hora (0..24, fração), das chaves do arquivo. `partyUv` = o grupo na pintura (0..1).
  static WorldMapParams overworldParams(const WorldMapConfig& c, float hour, glm::vec2 partyUv = {0.5f, 0.5f});
  // Mapa-múndi: as duas LUTs da hora (índices em c.luts; -1 sem) e a mistura entre elas.
  struct LutMix {
    int a = -1, b = -1;
    float t = 0;
  };
  static LutMix lutMix(const WorldMapConfig& c, float hour);
  // Submundo, o look 2D: os parâmetros à hora da SUPERFÍCIE (o ciclo invertido: floração na noite da superfície).
  static WorldMapParams underworldLookParams(const WorldMapConfig& c, float hour);
  // A floração L (0 dormente .. 1 auge) à hora da superfície (dayNight do arquivo do Submundo).
  static float bloomAt(const WorldMapConfig& c, float hour);
  // Submundo: o ponto do ciclo (0 dormindo .. 1 auge) às `totalHours` horas do relógio da campanha, e os parâmetros nesse ponto.
  static float underworldGlow(const WorldMapConfig& c, double totalHours);
  static WorldMapParams underworldParams(const WorldMapConfig& c, float glow);

  // Memória de vídeo das texturas deste módulo, em bytes (a medida pedida).
  uint64_t textureBytes() const { return textureBytes_; }

 private:
  struct Slot {
    WorldMapFx* self = nullptr;
    Draw draw;
    uint32_t paramsSlot = 0;
  };
  static void callback(const ImDrawList* list, const ImDrawCmd* cmd);
  void record(VkCommandBuffer cmd, const Slot& slot);
  uint32_t upload(const unsigned char* pixels, uint32_t w, uint32_t h, VkFormat format, uint32_t bytesPerPixel, Image& into, uint32_t index);

  VulkanContext& context_;
  Bindless& bindless_;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  VkSampler sampler_ = VK_NULL_HANDLE, repeatSampler_ = VK_NULL_HANDLE;
  uint32_t samplerIndex_ = 0, repeatSamplerIndex_ = 0;
  Image noise_;  // a textura de ruído repetível (256 x 256 RGBA8: fbm 5 / 3 / 2 oitavas + fino), feita uma vez na CPU
  uint32_t noiseIndex_ = kNoTexture;
  std::vector<Image> images_;
  std::vector<uint32_t> indices_;
  Image reveal_;
  uint32_t revealIndex_ = kNoTexture;
  uint64_t textureBytes_ = 0;
  // Os parâmetros por quadro: um anel de buffers visíveis ao host, lidos pelo shader pelo endereço (kFramesInFlight + folga).
  static constexpr uint32_t kParamSlots = 8;
  std::array<Buffer, kParamSlots> params_{};
  std::array<Slot, kParamSlots> slots_{};
  uint32_t next_ = 0;
};

}  // namespace ember
