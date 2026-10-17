// L'ÉCRAN DE VEILLE, branché sur la partie : ce que la boucle lui donne, et ce
// qu'il en fait.
//
// ⚠ MÊME CLASSE QUE `game.cpp`, AUTRE UNITÉ DE TRADUCTION. La décision, la pose
// de caméra et le filtre d'entrées vivent dans `moonclient_afk`, qui ne connaît
// ni SDL ni le réseau ; ce fichier-ci est la couture — il traduit les
// événements, tient l'horloge, et porte au serveur ce que la veille veut dire.

#include <array>
#include <cstdio>
#include <ctime>
#include <string>

#include <SDL3/SDL.h>

#include "ro/core/i18n.h"
#include "ro/core/local_time.h"
#include "ro/net/player_settings.h"
#include "ro/sim/pvp_rank.h"

#include "game/game.h"

namespace moonclient {
namespace {

constexpr std::uint32_t kMillisecondsPerMinute = 60000;
constexpr std::uint32_t kMinutesPerHour = 60;

// L'heure locale, « 14:05 ». Vide si le système refuse de la rendre.
[[nodiscard]] std::string LocalHourAndMinute() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
  if (!ro::core::ToLocalTime(now, local)) {
    return {};
  }
  std::array<char, 16> written{};
  // ⚠ `strftime` REND ZÉRO QUAND ÇA NE TIENT PAS, et laisse alors le tampon
  // dans un état qu'on ne doit pas lire.
  if (std::strftime(written.data(), written.size(), "%H:%M", &local) == 0) {
    return {};
  }
  return std::string(written.data());
}

}  // namespace

bool Game::NoteAfkInput(const SDL_Event& event, std::uint32_t now_ms) {
  AfkInput input;
  switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
      input.kind = AfkInputKind::kKeyPress;
      break;
    case SDL_EVENT_KEY_UP:
      input.kind = AfkInputKind::kKeyRelease;
      break;
    case SDL_EVENT_TEXT_INPUT:
      input.kind = AfkInputKind::kTextInput;
      break;
    case SDL_EVENT_MOUSE_MOTION:
      input.kind = AfkInputKind::kMouseMove;
      input.x = event.motion.x;
      input.y = event.motion.y;
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      input.kind = AfkInputKind::kMouseWheel;
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      input.kind = AfkInputKind::kMousePress;
      input.button = event.button.button;
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      input.kind = AfkInputKind::kMouseRelease;
      input.button = event.button.button;
      break;
    default:
      // 🔴 TOUT LE RESTE NE COMPTE PAS, et notamment ce que le système envoie
      // sans que personne n'ait touché à rien — redimensionnement, focus,
      // horloge. Les compter, c'est une veille qui ne se déclenche jamais.
      return false;
  }
  return afk_idle_.Note(input, now_ms);
}

void Game::StartAfkNow(std::uint32_t now_ms) {
  afk_screen_.StartNow(afk_idle_, camera_, now_ms);
  // 🔴 LE CLIC QUI VIENT D'APPUYER SUR « ESSAYER MAINTENANT » EST ENCORE
  // ENFONCÉ. Son relâchement — le second temps d'un geste déjà fini — rouvrirait
  // les yeux aussitôt fermés : la veille durerait une image, et le bouton
  // passerait pour cassé. On marque donc les boutons tenus comme déjà avalés.
  //
  // ⚠ LE MASQUE DE SDL EST CELUI DU COMPTEUR : `SDL_BUTTON_MASK(n)` vaut
  // `1 << (n - 1)`, exactement la numérotation d'`AfkIdle`.
  afk_idle_.SwallowHeldButtons(SDL_GetMouseState(nullptr, nullptr));
}

void Game::AdvanceAfk(std::uint32_t now_ms) {
  const AfkSettings& how = settings_->options().afk();
  // 🔴 PAS DE VEILLE APRÈS UNE PERTE DE LIAISON, ni automatique ni demandée :
  // « masquer l'interface » cacherait la boîte qui la dit. Le joueur absent au
  // moment de la panne doit la trouver à son retour, et non un monde qui tourne.
  if (link_loss_.declared()) {
    afk_requested_ = false;
    afk_screen_.EndNow(afk_idle_, camera_);
    afk_screen_.ForgetAnnounce();
    return;
  }
  if (afk_requested_) {
    afk_requested_ = false;
    StartAfkNow(now_ms);
  }
  afk_screen_.Advance(how, afk_idle_, camera_, now_ms);

  // ── PRÉVENIR L'ENTOURAGE ──────────────────────────────────────────────────
  //
  // 🔴 HORS DE TOUTE GARDE SUR LA VEILLE : l'annonce a son propre délai et ne
  // dépend ni de `enabled` ni de la phase en cours. Se signaler absent et se
  // regarder tourner sont deux envies distinctes.
  if (!link_.is_open()) {
    // Le personnage n'est pas dans le monde : sa session meurt de l'autre côté,
    // et l'absence avec elle. ⚠ ICI ET NON AU CHANGEMENT DE CARTE, où la
    // session survit — voir `AfkScreen::ForgetAnnounce`.
    afk_screen_.ForgetAnnounce();
    return;
  }
  const std::uint8_t wanted = afk_screen_.WantedAnnounce(how, afk_idle_.IdleMs(now_ms));
  if (wanted == afk_screen_.announced()) {
    // ⚠ AUX TRANSITIONS SEULEMENT. Le serveur ignore un masque qu'il a déjà,
    // mais un paquet par image reste un paquet par image — et le retrait de
    // l'étiquette coûte, lui, un nom refait chez chaque voisin.
    return;
  }
  ro::net::SendPlayerSetting(link_, ro::net::kSettingAfk, wanted);
  afk_screen_.NoteAnnounced(wanted);
}

ro::ui::AfkClockView Game::AfkClock(std::uint32_t now_ms) {
  const AfkSettings& how = settings_->options().afk();
  afk_clock_text_.clear();
  afk_away_text_.clear();
  if (!how.show_clock) {
    return {};
  }
  afk_clock_text_ = LocalHourAndMinute();

  if (how.show_away) {
    const std::uint32_t minutes =
        (now_ms - afk_screen_.sleep_started_ms()) / kMillisecondsPerMinute;
    // ⚠ LES TROIS GABARITS SONT GRAVÉS, au caractère près : le catalogue les
    // traduit déjà. Les réécrire « 3 minutes » plutôt que « 3 min » coûterait
    // trois traductions à refaire pour rien.
    std::array<char, 96> written{};
    if (minutes < 1) {
      afk_away_text_ = ro::core::Tr("Absent à l'instant");
    } else if (minutes < kMinutesPerHour) {
      std::snprintf(written.data(), written.size(), ro::core::Tr("Absent depuis %u min"),
                    static_cast<unsigned>(minutes));
      afk_away_text_ = written.data();
    } else {
      std::snprintf(written.data(), written.size(), ro::core::Tr("Absent depuis %uh%02u"),
                    static_cast<unsigned>(minutes / kMinutesPerHour),
                    static_cast<unsigned>(minutes % kMinutesPerHour));
      afk_away_text_ = written.data();
    }
  }

  return ro::ui::AfkClockView{.time = afk_clock_text_.c_str(),
                              .away = afk_away_text_.empty() ? nullptr : afk_away_text_.c_str(),
                              .anchor = how.clock_anchor,
                              .margin = how.clock_margin,
                              .scale = how.clock_scale,
                              .shadow = how.clock_shadow,
                              .color = how.clock_color,
                              .fade = afk_screen_.blend()};
}

}  // namespace moonclient
