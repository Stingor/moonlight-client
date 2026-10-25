// LE MODE SURVIVORS DANS LA PARTIE : ce que la partie prête au mode à chaque
// image, la branche de `Frame` qui remplace le monde par l'arène tant que la
// partie de survivors tient l'écran, et la route des événements SDL.
//
// 🔴 LA BRANCHE NE SAUTE PAS `DrawInterface` : elle remplace le dessin du
// monde et la teinte de la veille, puis l'interface sort au même point que la
// veille (`DrawSurvivorsOnly`). Tout ce qui précède ce point a tourné — chat
// trié, enregistreur avancé, bandeaux purgés — et une partie de quinze minutes
// ne laisse donc grossir aucune file du monde.
//
// Le réseau, le carnet MVP, les quêtes, le champ de bataille, les mini-jeux et
// les classements ont avancé avant : le personnage reste en ligne.

#include <array>
#include <optional>
#include <string>
#include <utility>

#include <SDL3/SDL.h>

#include "ro/render/screen_tint.h"
#include "ro/sim/deal.h"

#include "game/game.h"
#include "game/survivors_mode.h"
#include "input/hotkeys.h"
#include "survivors/survivors_route.h"

namespace moonclient {
namespace {

// L'image sans arène — un bilan relu, une arène qui vient de se refermer — se
// vide en noir.
constexpr std::array<float, 4> kNoArenaClearColor = {0.0F, 0.0F, 0.0F, 1.0F};

// La lettre que porte la touche à cette place sur le clavier du joueur.
[[nodiscard]] std::string KeyNameOf(std::uint32_t scancode) {
  const SDL_Keycode key =
      SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(scancode), SDL_KMOD_NONE, false);
  const char* name = SDL_GetKeyName(key);
  return name != nullptr ? std::string(name) : std::string();
}

[[nodiscard]] SurvivorsEvent SurvivorsEventOf(const SDL_Event& event) {
  SurvivorsEvent routed;
  switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      routed.kind = event.type == SDL_EVENT_KEY_DOWN ? SurvivorsEventKind::kKeyDown
                                                     : SurvivorsEventKind::kKeyUp;
      routed.scancode = static_cast<std::uint32_t>(event.key.scancode);
      routed.repeat = event.key.repeat;
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      routed.kind = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? SurvivorsEventKind::kMouseDown
                                                              : SurvivorsEventKind::kMouseUp;
      routed.button = event.button.button;
      routed.x = event.button.x;
      routed.y = event.button.y;
      break;
    case SDL_EVENT_MOUSE_MOTION:
      routed.kind = SurvivorsEventKind::kMouseMove;
      routed.x = event.motion.x;
      routed.y = event.motion.y;
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      routed.kind = SurvivorsEventKind::kWheel;
      routed.wheel_notches = event.wheel.y;
      break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
      routed.kind = SurvivorsEventKind::kFocusLost;
      break;
    case SDL_EVENT_TEXT_INPUT:
      routed.kind = SurvivorsEventKind::kText;
      break;
    default:
      routed.kind = SurvivorsEventKind::kOther;
      break;
  }
  return routed;
}

}  // namespace

bool Game::SurvivorsTakes(const SDL_Event& event, std::uint32_t now_ms) {
  // 🔴 LA FLÈCHE SUIT LA SOURIS que la partie prend : sa position se note ici,
  // puisque l'événement n'atteindra pas la route du jeu qui la note d'ordinaire.
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    mouse_ = glm::vec2{event.motion.x, event.motion.y};
  } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
    mouse_ = glm::vec2{event.button.x, event.button.y};
  }
  // L'ENREGISTREUR GARDE SES TOUCHES, hors de celles que la partie lit : un GIF
  // se lance et s'arrête, une image fixe se prend. Tracer la zone attend le
  // jeu, dont l'interface dessine le tracé.
  if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
      !SurvivorsReadsKey(static_cast<std::uint32_t>(event.key.scancode))) {
    const std::optional<ActionRef> action = settings_->bindings().ActionFor(
        event.key.key, (event.key.mod & SDL_KMOD_CTRL) != 0, (event.key.mod & SDL_KMOD_SHIFT) != 0,
        (event.key.mod & SDL_KMOD_ALT) != 0);
    if (action == ActionRef(GameAction::kCaptureRecord)) {
      recorder_.ToggleRecording(now_ms);
      return true;
    }
    if (action == ActionRef(GameAction::kCapturePicture)) {
      recorder_.AskPicture();
      return true;
    }
  }
  return survivors_mode_->Take(SurvivorsEventOf(event));
}

void Game::FrameSurvivors(std::uint32_t now_ms) {
  const ro::vfs::TownInfo* towns = navi_.towns();
  const ro::sim::DealPhase deal = pieces_.trade.deal().deal().phase();
  survivors_mode_->Remember(SurvivorsContext{
      .vfs = vfs_,
      .wardrobe = pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr,
      .audio = &audio_,
      .music = &music_,
      .link = &link_,
      .world = &world_,
      .minigames = &windows_.minigames,
      .minigame_stage = &pieces_.minigames,
      .ranking = &survivors_ranking_,
      .aside = WorldAside{.on_town_map = towns != nullptr && !towns->On(world_.map_name()).empty(),
                          .on_indoor_map = camera_rig_.indoor(),
                          .trade_open = deal != ro::sim::DealPhase::kNone,
                          .trade_offered = deal == ro::sim::DealPhase::kProposed,
                          .shop_open = pieces_.trade.HoldsWorld(),
                          .loading = moving_.has_value() || !stage_.has_value(),
                          .party_invited = party_.invitation().has_value(),
                          .guild_invited = guild_book_.invitation().has_value(),
                          .friend_invited = friends_.invitation().has_value(),
                          .link_open = link_.is_open()},
      .options_window = &windows_.options,
      .option_sections = settings_->options().sections(),
      .graphics = &settings_->options().graphics(),
      .tables =
          SurvivorsTables{
              .skill_tree = pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
              .skill_icons = pieces_.skill_icons.has_value() ? &*pieces_.skill_icons : nullptr,
              .items = &pieces_.trade.items(),
              .item_icons = pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
              .monsters = &pieces_.monster_list,
              .jobs = &pieces_.job_names},
      .map_names = pieces_.map_display_names.has_value() ? &*pieces_.map_display_names : nullptr,
      .images = pieces_.game_images.has_value() ? &*pieces_.game_images : nullptr,
      .sign_mode = settings_->options().pages().map_sign.mode(),
      .client_root = client_root_,
      .key_namer = KeyNameOf,
      .width = static_cast<float>(shell_.width()),
      .height = static_cast<float>(shell_.height()),
      .quake = settings_->options().graphics().screen_quake(),
      .say =
          [this, now_ms](std::string text) {
            world_.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(text), now_ms);
          },
      .now_ms = now_ms});
  const SurvivorsContext& context = survivors_mode_->context();
  survivors_mode_->FollowDoor(survivors_, context);
  if (!survivors_.owns_screen()) return;

  // LA FLÈCHE DU CURSEUR, sans rien du monde à désigner.
  ComposeCursor(nullptr, std::nullopt, now_ms);
  gl_->BindForDisplay();
  const SurvivorsPainted painted = survivors_mode_->Frame(survivors_, context);
  if (painted.loading) {
    shell_.DrawLoadingFrame(nullptr, 1.0F, now_ms);
    return;
  }
  if (!painted.arena) gl_->ClearBoundTarget(kNoArenaClearColor);
  const ro::render::DrawableSize surface = gl_->drawable_size();
  screen_tint_.Draw(ro::render::ScreenTintLook{.desaturate = painted.desaturation}, surface.width,
                    surface.height, now_ms);
}

}  // namespace moonclient
