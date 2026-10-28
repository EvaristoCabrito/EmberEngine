#pragma once

// The game's sound, ported from Ember's src/game/audio.ts: one music track at a time (the screen's theme or a
// mission's own file, looping, each track keeping its place when another one plays, as Ember's <audio> elements did)
// and one-shot effects over it (SoundFX). Ember's two effect players are kept: "layered" (a fresh copy per play, left
// to finish: playSfxFile) and "exclusive" (one voice per file, a retrigger restarts it: playSfxFileExclusive, with an
// optional start offset and a short fade-out stop). Volumes as Ember's: a track plays at base x music volume, an
// effect at its own volume x the effects volume; Silenciar silences both and stops the music.
//
// MP3s are decoded by Windows Media Foundation (as the films are): effects whole on first use and kept, music streamed
// on a worker thread. Everything is mixed by SDL3: every voice is an SDL audio stream bound to one playback device.
// Calls come from the main thread; update() once a frame runs delays, fades and reclaims finished voices.

#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct SDL_AudioStream;

namespace ember {

class AudioMixer {
 public:
  static AudioMixer& get();
  ~AudioMixer();

  // Ember's public folder: music in game/MUSIC, effects in game/SoundFX. Without one nothing plays (captures).
  // EmberEngine's own tracks live in content/music and are found there first, by the same file name.
  void setRoot(const std::filesystem::path& artRoot);
  // Stops everything and closes the device; called before SDL shuts down.
  void shutdown();

  // --- music (audio.ts playTheme / playFile / playMenuMusic / stopMusic / pauseMusic / resumeMusic) ---
  // Plays `file` (under game/MUSIC) at `base` x the music volume, silencing every other track; the same file again is
  // left playing. A track not heard before starts at its beginning; one heard before carries on where it stopped.
  void playMusic(const std::string& file, float base);
  void stopMusic();    // every track stops (keeps its place)
  bool musicPlaying() const { return !current_.empty() && !paused_; }
  const std::string& currentMusic() const { return current_; }

  // --- effects (playSfxFile / playSfxFileExclusive / stopSfxFileExclusive) ---
  // `delay`: seconds from now (Neera's bow release); `offset`: seconds into the file.
  void playSfx(const std::string& file, float volume, bool exclusive, float offset = 0.0f, float delay = 0.0f);
  void fadeSfx(const std::string& file, float seconds = 0.12f);
  // Decodes effects ahead of their first play (preloadExclusiveSfx).
  void preload(const std::vector<std::string>& files);

  // --- settings (Opções) ---
  void setVolumes(float music, float sfx, bool muted);

  void update(float dt);

  // Sandbox --audio-check: every effect in game/SoundFX decoded whole and the first seconds of every track in
  // game/MUSIC, as the game decodes them; logs each and returns how many failed.
  static int check(const std::filesystem::path& artRoot);

 private:
  AudioMixer() = default;
  struct Pcm {
    int rate = 48000, channels = 2;
    std::vector<float> samples;  // interleaved
    bool ok = false;
  };
  struct Voice {
    SDL_AudioStream* stream = nullptr;
    std::string file;
    float volume = 1.0f;  // its own (before the effects volume)
    float fade = -1.0f, fadeLeft = 0.0f;
    bool exclusive = false;
  };
  struct Pending {
    std::string file;
    float volume, offset, delay;
    bool exclusive;
  };
  struct Track;

  bool open();
  const Pcm& pcm(const std::string& file);
  void startVoice(const std::string& file, float volume, bool exclusive, float offset);
  void applyGains();

  std::filesystem::path root_;
  unsigned deviceId_ = 0;
  bool opened_ = false, failed_ = false;
  std::map<std::string, Pcm> cache_;
  std::vector<Voice> voices_;
  std::vector<Pending> pending_;
  std::map<std::string, std::unique_ptr<Track>> tracks_;
  std::string current_;
  bool paused_ = false;
  float music_ = 1.0f, sfx_ = 1.0f;
  bool muted_ = false;
};

}  // namespace ember
