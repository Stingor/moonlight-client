// LES RÉGLAGES QU'ON CHANGE SANS OUVRIR LE PANNEAU.
//
// 🔴 TROIS FAMILLES, ET UNE SEULE RAISON D'ÊTRE ENSEMBLE : aucune ne passe par
// la table des rangées. `Toggle*` vient d'une touche ou du menu du jeu, `Set*`
// d'un appelant qui sait déjà quelle valeur il veut, `Note*` d'une fenêtre qui a
// édité SA copie et la remonte. Le fichier d'à côté, lui, ne fait qu'une chose :
// parcourir la table.
//
// ⚠ TOUS REPASSENT PAR `Apply` ET `Record`, ou rendent « quelque chose a
// changé » à un appelant qui touchera le fichier. Écrire le champ et s'en aller
// laisserait un réglage qui se voit à l'écran et meurt à la fermeture.

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include <SDL3/SDL.h>

#include "ro/core/error.h"
#include "ro/core/i18n.h"
#include "ro/core/utf8_path.h"
// ⚠ POUR LE NOMBRE DE DIRECTIONS D'UN .ACT : c'est lui qui borne la direction
// relue du fichier, et le redéclarer ici en ferait une seconde vérité.
#include "ro/render/actor_layers.h"
#include "ro/render/doll_animation.h"
#include "ro/ui/system_fonts.h"

#include "options/game_options.h"

namespace moonclient {
namespace {

// Le catalogue d'une langue, à côté du jeu. ⚠ CE SONT NOS TEXTES et non ceux de
// l'installation du joueur : ils se versionnent avec le code qui les demande.
//
// 🔴 À CÔTÉ DE L'EXÉCUTABLE D'ABORD, et non sous le dossier courant : les deux
// diffèrent dès qu'un raccourci change le dossier de départ, et le joueur
// anglophone retombait alors en français sans rien en voir. Le dossier
// courant reste le repli : c'est là qu'un exe de développement, bâti loin de
// ses catalogues, les trouve.
[[nodiscard]] std::filesystem::path CataloguePath(std::string_view code) {
  const std::filesystem::path file = std::filesystem::path("lang") / (std::string(code) + ".txt");
  if (const char* base = SDL_GetBasePath(); base != nullptr) {
    std::filesystem::path beside = ro::core::PathFromUtf8(base) / file;
    std::error_code missing;
    if (std::filesystem::exists(beside, missing)) {
      return beside;
    }
  }
  return file;
}

}  // namespace

bool GameOptions::ToggleMusic(const OptionTargets& to) {
  sound_options_.music = !sound_options_.music;
  Apply(GameOption::kMusic, to);
  Record(to);
  return sound_options_.music;
}

bool GameOptions::ToggleSound(const OptionTargets& to) {
  sound_options_.sound = !sound_options_.sound;
  Apply(GameOption::kSound, to);
  Record(to);
  return sound_options_.sound;
}

bool GameOptions::ToggleEffectsShown(const OptionTargets& to) {
  effects_shown_ = !effects_shown_;
  Apply(GameOption::kEffectsShown, to);
  Record(to);
  return effects_shown_;
}

bool GameOptions::ToggleMinimalEffects(const OptionTargets& to) {
  minimal_effects_ = !minimal_effects_;
  Apply(GameOption::kMinimalEffects, to);
  Record(to);
  return minimal_effects_;
}

bool GameOptions::ToggleSkyEffectsShown(const OptionTargets& to) {
  sky_effects_shown_ = !sky_effects_shown_;
  Apply(GameOption::kSkyEffectsShown, to);
  Record(to);
  return sky_effects_shown_;
}

bool GameOptions::ToggleFixedCamera(const OptionTargets& to) {
  fixed_camera_ = !fixed_camera_;
  Apply(GameOption::kFixedCamera, to);
  Record(to);
  return fixed_camera_;
}

// ⚠ L'APPEL À `Apply` NE POUSSE RIEN pour ces deux-là, et il reste : c'est lui
// qui garde les deux chemins — la commande et la fenêtre — identiques. Le jour
// où l'un des deux réglages aura quelque chose à pousser, il n'y aura rien à
// se rappeler d'ajouter ici.
bool GameOptions::ToggleNoShift(const OptionTargets& to) {
  no_shift_ = !no_shift_;
  Apply(GameOption::kNoShift, to);
  Record(to);
  return no_shift_;
}

bool GameOptions::ToggleExpandedZoom(const OptionTargets& to) {
  expanded_zoom_ = !expanded_zoom_;
  Apply(GameOption::kExpandedZoom, to);
  Record(to);
  return expanded_zoom_;
}

bool GameOptions::ToggleLightmaps(const OptionTargets& to) {
  const bool on = pages_.graphics.ToggleLightmaps();
  Record(to);
  return on;
}

bool GameOptions::ToggleFog(const OptionTargets& to) {
  const bool on = pages_.graphics.ToggleFog();
  Record(to);
  return on;
}

bool GameOptions::ToggleShopSetting(ShopRow row, const OptionTargets& to) {
  const bool on = pages_.shops.Toggle(row);
  Record(to);
  return on;
}

bool GameOptions::ToggleNoCtrl(const OptionTargets& to) {
  no_ctrl_ = !no_ctrl_;
  Apply(GameOption::kNoCtrl, to);
  Record(to);
  return no_ctrl_;
}

// ⚠ RIEN À POUSSER : les icônes dessinent la place même que la page tient.
bool GameOptions::ToggleCashShopButton(const OptionTargets& to) {
  const bool shown = pages_.menu_icons.ToggleCashShopButton();
  Record(to);
  return shown;
}

bool GameOptions::ToggleBattleMode(const OptionTargets& to) {
  chat_look_.battle_mode = !chat_look_.battle_mode;
  Apply(GameOption::kChatBattleMode, to);
  Record(to);
  return chat_look_.battle_mode;
}

float GameOptions::SetInterfaceScale(float scale, const OptionTargets& to) {
  interface_scale_ = scale;
  Apply(GameOption::kInterfaceScale, to);
  Record(to);
  return interface_scale_;
}

bool GameOptions::SetLanguage(std::string_view code, const OptionTargets& to, std::string* why) {
  if (!PoseLanguage(code, why)) {
    return false;
  }
  Record(to);
  return true;
}

bool GameOptions::SetFontFamily(int family, const OptionTargets& to) {
  if (family < 0 || family >= static_cast<int>(ro::ui::SystemFontFamilies().size())) {
    return false;
  }
  font_family_ = family;
  Apply(GameOption::kFontFamily, to);
  Record(to);
  return true;
}

void GameOptions::SetBackdropWanted(bool wanted, const OptionTargets& to) {
  // ⚠ RIEN À POUSSER : personne ne surveille ce réglage. L'écran de connexion
  // le RELIT à chaque image, et c'est lui seul qui s'en sert — pousser vers un
  // moteur qui n'existe pas encore à cette heure n'aurait aucun destinataire.
  backdrop_wanted_ = wanted;
  Record(to);
}

bool GameOptions::PoseLanguage(std::string_view code, std::string* why) {
  const std::span<const Language> languages = KnownLanguages();
  const auto known = std::find_if(languages.begin(), languages.end(),
                                  [code](const Language& one) { return code == one.code; });
  if (known == languages.end()) {
    if (why != nullptr) {
      // ⚠ UNE PHRASE, ET NON LE CODE NU : « langue : de » ne disait pas ce qui
      // n'allait pas avec « de ».
      *why = "code inconnu « " + std::string(code) + " »";
    }
    return false;
  }
  // ⚠ LA PREMIÈRE LANGUE N'A PAS DE CATALOGUE : c'est celle dans laquelle le
  // code est écrit, et la charger reviendrait à traduire le français en
  // français.
  if (known == languages.begin()) {
    ro::core::UseFrench();
  } else if (const ro::core::Result<void> loaded =
                 ro::core::LoadLanguage(CataloguePath(known->code), known->code);
             !loaded) {
    if (why != nullptr) {
      *why = ro::core::Describe(loaded.error());
    }
    return false;
  }
  language_ = known->code;
  return true;
}

bool GameOptions::NoteChatMacros(std::span<const std::string> seen) {
  bool moved = false;
  for (std::size_t slot = 0; slot < chat_macros_.size() && slot < seen.size(); ++slot) {
    if (chat_macros_[slot] == seen[slot]) {
      continue;
    }
    chat_macros_[slot] = seen[slot];
    moved = true;
  }
  return moved;
}

bool GameOptions::NoteCaptureZone(const ro::ui::ZoneRect& drawn) noexcept {
  if (capture_.zone == drawn) {
    return false;
  }
  capture_.zone = drawn;
  return true;
}

bool GameOptions::NoteGrimoireSmoothIcons(bool seen) noexcept {
  if (grimoire_smooth_icons_ == seen) {
    return false;
  }
  grimoire_smooth_icons_ = seen;
  return true;
}

bool GameOptions::NoteCharacterStaffPane(bool seen) noexcept {
  if (character_staff_pane_ == seen) {
    return false;
  }
  character_staff_pane_ = seen;
  return true;
}

bool GameOptions::NoteStaffBattleIgnore(bool seen) noexcept {
  if (staff_battle_ignore_ == seen) {
    return false;
  }
  staff_battle_ignore_ = seen;
  return true;
}

ro::ui::CharacterPose GameOptions::character_avatar_pose() const noexcept {
  // 🔴 BORNÉ À LA RELECTURE, jamais à l'écriture : le fichier est du texte que
  // quelqu'un peut avoir édité, et un rang hors table ferait un `switch` sans
  // cas — la silhouette disparaîtrait sans un mot.
  if (character_avatar_pose_ < 0 ||
      character_avatar_pose_ >= static_cast<int>(ro::ui::kCharacterPoseCount)) {
    return ro::ui::CharacterPose::kReady;
  }
  return static_cast<ro::ui::CharacterPose>(character_avatar_pose_);
}

std::uint8_t GameOptions::character_avatar_direction() const noexcept {
  // Même borne, même raison : une direction hors des huit du .act montrerait
  // l'action d'un autre rang, c'est-à-dire n'importe quoi.
  const auto turns = static_cast<int>(ro::render::kDirectionCount);
  if (character_avatar_direction_ < 0 || character_avatar_direction_ >= turns) {
    return 0;
  }
  return static_cast<std::uint8_t>(character_avatar_direction_);
}

std::uint8_t GameOptions::character_avatar_gif_scale() const noexcept {
  // Même borne : une échelle hors de celles que la fiche propose ferait un
  // GIF démesuré, ou vide.
  if (character_avatar_gif_scale_ < ro::render::kSmallestDollGifScale ||
      character_avatar_gif_scale_ > ro::render::kLargestDollGifScale) {
    return ro::ui::kDefaultDollGifScale;
  }
  return static_cast<std::uint8_t>(character_avatar_gif_scale_);
}

bool GameOptions::NoteCharacterAvatar(const ro::ui::CharacterAvatarState& avatar) noexcept {
  const int rank = static_cast<int>(avatar.pose);
  const int turned = static_cast<int>(avatar.direction);
  const int gif_scale = static_cast<int>(avatar.gif_scale);
  if (character_avatar_pose_ == rank && character_avatar_animated_ == avatar.animated &&
      character_avatar_direction_ == turned && character_avatar_gif_scale_ == gif_scale) {
    return false;
  }
  character_avatar_pose_ = rank;
  character_avatar_animated_ = avatar.animated;
  character_avatar_direction_ = turned;
  character_avatar_gif_scale_ = gif_scale;
  return true;
}

bool GameOptions::NoteStaffTools(const ro::ui::StaffToolsLook& seen) noexcept {
  // ⚠ LA COMPARAISON EST CELLE DU TYPE, et non une liste de champs recopiée
  // ici : le nuancier se change sans toucher à la case, et un champ oublié se
  // lirait « ce réglage-là ne s'enregistre pas ». Voir `StaffToolsLook`.
  if (staff_tools_ == seen) {
    return false;
  }
  staff_tools_ = seen;
  return true;
}

}  // namespace moonclient
