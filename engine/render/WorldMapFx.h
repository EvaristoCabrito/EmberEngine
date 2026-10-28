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

// Os 16 float4 lidos pelo shader (worldmap.hlsl Params), já interpolados pela hora.
struct WorldMapParams {
  glm::vec4 tint, grade, light, vignette, fogColor, fogShape, lowColor, lowShape, glowColor, dark, caveColor, caveShape;
  glm::vec4 extra[4]{};
};
static_assert(sizeof(WorldMapParams) == 16 * 16);

// O arquivo de configuração de uma tela (content/worldmap/*.json). Um valor ausente fica no padrão daqui.
struct WorldMapConfig {
  bool enabled = true;
  std::string mask;  // a máscara pintada (PNG, R = névoa baixa / de caverna, G = seres que brilham); vazio = heurística no shader
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
    WorldMapParams params;
  };
  void draw(ImDrawList* dl, const Draw& d);

  // Mapa-múndi: os parâmetros da hora (0..24, fração), das chaves do arquivo.
  static WorldMapParams overworldParams(const WorldMapConfig& c, float hour);
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
