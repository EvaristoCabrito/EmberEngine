#pragma once

// Mapa-múndi e Submundo: a luz e a névoa das duas telas do mapa (apps/sandbox/OverworldStage.h), um sistema só delas, sem
// relação com a névoa, o céu e a atmosfera das batalhas. A pintura 2D é desenhada por shaders/worldmap.hlsl dentro do passe
// da UI (um callback do ImDrawList, no lugar da imagem da UI): a luz por hora do relógio da campanha (mapa-múndi) ou o
// ciclo dos seres que brilham (Submundo), a névoa de guerra viva, a névoa baixa / de caverna e o acabamento de cor.
// Os valores ajustáveis: content/worldmap/overworld-light-fog.json e underworld-light-fog.json (WorldMapConfig).

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
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
  glm::vec4 extra[4]{};  // [1] mapa-múndi: a cor do céu nas sombras (rgb) e quanto (w); [2] as luzes: x acesas (0..1), y tremor, z halo; [0] Submundo: a luz ambiente no auge (x)
};
static_assert(sizeof(WorldMapParams) == 16 * 16);

// O arquivo de configuração de uma tela (content/worldmap/*.json). Um valor ausente fica no padrão daqui.
struct WorldMapConfig {
  bool enabled = true;
  std::string mask;  // a máscara pintada (PNG, R = névoa baixa / de caverna, G = seres que brilham); vazio = heurística no shader
  // Mapa-múndi: uma chave por hora (0..24, cíclico), interpolada suavemente.
  struct HourKey {
    float hour = 12;
    glm::vec3 tint{1, 1, 1};  // a cor da luz, nas partes claras da pintura
    float exposure = 1;
    float contrast = 1.1f, saturation = 1, lift = 0, gamma = 1;
    glm::vec2 lightDir{-0.6f, -0.8f};
    float relief = 0.6f, rays = 0;
    glm::vec3 fogColor{0.05f, 0.05f, 0.06f};
    glm::vec3 lowColor{0.5f, 0.55f, 0.6f};
    float lowStrength = 0.5f;
    // A cor do céu, nas sombras (o escuro da pintura): um multiply só não dá rosa no claro e azul na sombra ao mesmo tempo, que é o
    // que faz um amanhecer parecer amanhecer (o usuário, 2026-10-27). shadowMix 0 = só a tint, como antes.
    // No fim da struct: as chaves padrão em WorldMapFx.cpp são inicializadas por posição.
    glm::vec3 shadow{1, 1, 1};
    float shadowMix = 1;
    float lightsOn = 0;  // 0..1: quanto as fontes de luz do mapa estão acesas nesta hora (1 à noite)
  };
  std::vector<HourKey> keys;
  // As fontes de luz quentes sobre a pintura (a referência do usuário, 2026-10-27: Axie Terrariums à noite: a cena azul e cada
  // casa / caldeirão com o seu halo laranja iluminando o chão em volta). Uma luz em cada local do mapa-múndi (overworld.json), a
  // do grupo onde o líder está, mais as "extra" do arquivo; por local, "overrides" por id. Unidades: uv da pintura (0..1).
  struct MapLight {
    glm::vec2 uv{0, 0};
    float radius = 0.05f, strength = 1;
    glm::vec3 color{1, 0.62f, 0.3f};
    float seed = 0;
  };
  struct LightStyle {
    glm::vec3 color{1, 0.62f, 0.3f};
    float radius = 0.05f, strength = 1.2f;
  };
  LightStyle locationLight, partyLight{{1, 0.72f, 0.42f}, 0.035f, 1.0f};
  std::map<std::string, LightStyle> lightOverrides;  // por id de local
  std::vector<MapLight> extraLights;
  float lightFlicker = 0.08f, lightHalo = 0.35f;
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
    std::vector<WorldMapConfig::MapLight> lights;  // as fontes de luz deste quadro (até kMaxLights)
  };
  static constexpr uint32_t kMaxLights = 96;
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
  std::array<Buffer, kParamSlots> lights_{};  // kMaxLights x 32 bytes por slot
  std::array<Slot, kParamSlots> slots_{};
  uint32_t next_ = 0;
};

}  // namespace ember
