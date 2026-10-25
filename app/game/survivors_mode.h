#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "ro/audio/audio_engine.h"
#include "ro/net/connection.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/game_images.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/world.h"
#include "ro/vfs/map_display_names.h"
#include "ro/vfs/vfs.h"

#include "game/minigame_stage.h"
#include "game/survivors_book_mode.h"
#include "game/survivors_meta_desk.h"
#include "game/survivors_ranking_mode.h"
#include "game/survivors_trophy_count.h"
#include "game/survivors_views.h"
#include "game/ui_stage_survivors.h"
#include "options/graphics_options.h"
#include "survivors/arena_measures.h"
#include "survivors/arena_music.h"
#include "survivors/arena_session.h"
#include "survivors/arena_sound_budget.h"
#include "survivors/posture_plan.h"
#include "survivors/real_world_watch.h"
#include "survivors/survivors_door.h"
#include "survivors/survivors_keys.h"
#include "survivors/survivors_route.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_stage.h"
#include "survivors/survivors_world_facts.h"
#include "world/map_music.h"
#include "world/map_sign_stage.h"

namespace moonclient {

// LE MODE SURVIVORS DANS LA PARTIE : la porte (préparation, garde, entrée), la
// partie qui tient l'écran (relevé du monde réel, intention, étage, arène,
// sons, musique, mesures), la sortie et la liaison perdue. `Game` le nourrit
// de ce qu'il emprunte, image par image, et ne garde que l'étage
// (`SurvivorsStage`), que la route des événements interroge.
//
// 🔴 LE PERSONNAGE RESTE EN LIGNE ET IMMOBILE : rien de ce qui se passe dans
// l'arène ne part au serveur, sauf s'asseoir à l'entrée et se relever à la
// sortie (U4).

// CE QUE LE MODE EMPRUNTE À LA PARTIE pour une image. ⚠ TOUT PEUT MANQUER sauf
// le monde, les fenêtres de la salle et le son : sans garde-robe ni archives,
// la partie refuse de commencer, en le disant.
struct SurvivorsContext {
  ro::vfs::Vfs* vfs = nullptr;
  ro::render::ActorWardrobe* wardrobe = nullptr;
  ro::audio::AudioEngine* audio = nullptr;
  MapMusic* music = nullptr;
  ro::net::Connection* link = nullptr;
  const ro::sim::World* world = nullptr;
  MinigameWindows* minigames = nullptr;
  // Les trophées : une partie finie y inscrit les siens.
  MinigameStage* minigame_stage = nullptr;
  // Le classement : une partie classée ou un défi y demande son jeton et y
  // envoie sa fin. Nul, toute partie part libre.
  SurvivorsRankingMode* ranking = nullptr;
  WorldAside aside;
  const GraphicsOptions* graphics = nullptr;
  SurvivorsTables tables;
  const ro::vfs::MapDisplayNames* map_names = nullptr;
  ro::render::GameImages* images = nullptr;
  MapSignMode sign_mode = MapSignMode::kEveryTitled;
  std::filesystem::path client_root;
  ScancodeNamer key_namer;
  // La fenêtre, en points : la souris et l'interface en parlent.
  float width = 0.0F;
  float height = 0.0F;
  // `/quake` : la caméra de l'arène se secoue.
  bool quake = true;
  // Une ligne au joueur, dans la conversation.
  std::function<void(std::string)> say;
  std::uint32_t now_ms = 0;
};

// CE QUE L'IMAGE DU MODE A PEINT, et ce qu'app doit encore faire.
struct SurvivorsPainted {
  // L'arène se monte : l'écran de chargement couvre l'image.
  bool loading = false;
  // L'arène est peinte ; faux, l'image se vide en noir.
  bool arena = false;
  // La teinte d'écran à poser par-dessus : voir `SurvivorsDesaturation`.
  float desaturation = 0.0F;
};

class SurvivorsMode {
 public:
  // CE QUE LA PARTIE PRÊTE POUR CETTE IMAGE, gardé jusqu'à la suivante :
  // l'interface en compose les fenêtres après le dessin.
  void Remember(SurvivorsContext context) { context_ = std::move(context); }
  [[nodiscard]] const SurvivorsContext& context() const noexcept { return context_; }
  // L'image a montré l'écran de chargement : elle n'a pas d'autre interface.
  [[nodiscard]] bool loading_shown_now() const noexcept { return loading_now_; }

  // À CHAQUE IMAGE, partie ou non : la salle rouverte montre le bilan d'une
  // partie interrompue (Q3), et la préparation se compose, juge l'entrée et
  // fait entrer.
  void FollowDoor(SurvivorsStage& stage, const SurvivorsContext& context);
  // Un événement pendant que la partie tient l'écran : vrai quand il est pris.
  [[nodiscard]] bool Take(const SurvivorsEvent& event) { return route_.Take(event); }
  // UNE IMAGE DE PARTIE : relevé, intention, étage, arène, sons, musique.
  // L'arène se peint dans la cible liée par l'appelant.
  [[nodiscard]] SurvivorsPainted Frame(SurvivorsStage& stage, const SurvivorsContext& context);
  // LES FENÊTRES DE L'IMAGE, après `Frame`.
  [[nodiscard]] SurvivorsFrameView View(const SurvivorsStage& stage,
                                        const SurvivorsContext& context);
  // Ce que les fenêtres ont demandé, pour l'image suivante.
  void NoteAsk(const SurvivorsAsk& ask) { ask_ = ask; }
  // LA LIAISON EST PERDUE, ou la partie se ferme (Q3) : une partie en cours
  // s'abandonne au pas de la coupure, son journal et sa fin s'écrivent, et
  // elle compte pour les trophées ce qu'elle a déjà atteint.
  void Freeze(SurvivorsStage& stage, MinigameStage& minigames,
              const std::filesystem::path& client_root);
  // LA PARTIE DE JEU SE DÉTRUIT : l'arène part avant la garde-robe qu'elle
  // habille, que la partie possède et détruit avant ses modes.
  void Forget() noexcept { arena_.reset(); }

 private:
  void Enter(SurvivorsStage& stage, const SurvivorsContext& context, std::uint16_t first_skill_id);
  // Monte l'arène, ou reprend celle gardée pour « Rejouer », et fait arriver
  // la partie. Faux quand l'arène ne se monte pas : le mode s'est refermé.
  [[nodiscard]] bool Mount(SurvivorsStage& stage, const SurvivorsContext& context);
  // Refait le départ que le classement vient de régler. Faux quand il ne
  // commence pas : le mode s'est refermé.
  [[nodiscard]] bool Resettle(SurvivorsStage& stage, const SurvivorsContext& context);
  void Exit(SurvivorsStage& stage, const SurvivorsContext& context);
  [[nodiscard]] SurvivorsIntent IntentOf(const SurvivorsStage& stage,
                                         const SurvivorsContext& context,
                                         const SurvivorsGestures& gestures,
                                         const RealWorldReport& report);
  void ApplyOptions(const ro::ui::SurvivorsPauseAsk& asked, const SurvivorsContext& context);
  // Écrit les réglages qui diffèrent de `before`.
  void SaveOptions(const SurvivorsOptions& before, const SurvivorsContext& context);
  void FollowEntered(SurvivorsStage& stage, const SurvivorsContext& context);
  void PaintArena(const SurvivorsStage& stage, const SurvivorsContext& context);
  void PlaySounds(const ro::sim::SurvivorsNews& news, const SurvivorsContext& context);
  void PlaySound(std::string_view name, ArenaSoundRank rank, const SurvivorsContext& context);
  void FollowMusic(const SurvivorsStage& stage, const SurvivorsContext& context);
  void LogMeasures() const;
  // `Freeze`, les trophées comptés si `minigames` n'est pas nul.
  void FreezeRun(SurvivorsStage& stage, MinigameStage* minigames,
                 const std::filesystem::path& client_root);
  [[nodiscard]] std::uint16_t ChosenBolt() const;

  SurvivorsContext context_;
  bool loading_now_ = false;
  SurvivorsRoute route_;
  SurvivorsViews views_;
  SurvivorsAsk ask_;
  std::optional<ArenaSession> arena_;
  std::vector<ro::sim::SurvivorsOrb> orbs_;
  RealWorldWatch watch_;
  WhisperCounter whispers_;
  // L'alerte qui a mis la partie en pause, pour son bandeau.
  std::optional<RealWorldAlert> banner_;
  PosturePlan posture_;
  SurvivorsOptions options_;
  bool options_loaded_ = false;
  ArenaSoundBudget sounds_;
  // Les bruits que l'installation n'a pas : dits une fois au journal.
  std::set<std::string, std::less<>> missing_sounds_;
  ArenaMusic music_;
  bool music_muted_ = false;
  // Le volume de la musique à l'entrée, rendu à la sortie.
  float entry_music_volume_ = 1.0F;
  std::optional<float> music_volume_set_;
  ArenaMeasures measures_;
  MapSignStage sign_;
  std::optional<ro::ui::MapSignView> sign_view_;
  WatchedClock clock_;
  // L'écran de chargement a été présenté une fois : l'arène peut se monter
  // derrière lui à l'image suivante.
  bool loading_shown_ = false;
  HubDoor hub_door_;
  // Le journal de la partie finie est écrit.
  bool run_saved_ = false;
  SurvivorsTrophyCount trophies_;
  SurvivorsBookMode book_;
  // Le carnet de l'accueil et les intentions de ses fenêtres de méta.
  SurvivorsMetaDesk desk_;
  // L'apparence du héros posée, et si elle était celle de l'évolution.
  std::optional<bool> hero_evolved_;
  int pressed_card_key_ = 0;
  std::array<std::string, 4> key_labels_;
};

}  // namespace moonclient
