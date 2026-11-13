#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// L'ONGLET « EFFETS » DE L'ÉDITEUR DE STYLE : la teinture des effets de chapeau
// que le joueur affiche — une aura, une pluie de pétales, un anneau.
//
// Un effet n'a pas de pièces : sa teinture est un seul réglage, un tour de
// teinte, une saturation et une clarté, plus « Assombrir » pour un effet qui
// s'ajoute au fond. Elle appartient à l'exemplaire de l'objet qui pose l'effet,
// et part avec lui.

// Les bornes du réglage, celles que le serveur accepte.
inline constexpr int kPaletteEffectHueMost = 180;
inline constexpr int kPaletteEffectPercentMost = 100;

// Un réglage d'effet.
struct PaletteEffectSetting {
  int hue = 0;
  int saturation = 0;
  int lightness = 0;
  bool darken = false;

  [[nodiscard]] bool neutral() const noexcept {
    return hue == 0 && saturation == 0 && lightness == 0 && !darken;
  }
  [[nodiscard]] friend bool operator==(const PaletteEffectSetting&,
                                       const PaletteEffectSetting&) = default;
};

// POURQUOI UN EFFET NE SE TEINT PAS.
enum class PaletteEffectBlock : std::uint8_t {
  kNone,
  // Il ne peint rien : il change le corps du porteur (le rapetisse, le
  // blanchit…).
  kChangesBody,
  // Le micro de la voix : un signal, pas une parure.
  kVoice,
};

// Un effet affiché.
struct PaletteEffectRow {
  std::uint16_t number = 0;
  // Le nom de l'effet, et celui de l'objet qui le pose, ou vide.
  std::string name;
  std::string item_name;
  // L'effet s'ajoute au fond : baisser sa clarté le pâlit, « Assombrir » le
  // fonce.
  bool additive = false;
  PaletteEffectBlock blocked = PaletteEffectBlock::kNone;
  // Ce que le serveur dit qu'il porte.
  PaletteEffectSetting worn;
};

// Où en est le dernier envoi.
enum class PaletteEffectSend : std::uint8_t { kNone, kSent, kShared };

struct PaletteEffectsView {
  std::span<const PaletteEffectRow> rows;
  bool connected = false;
  // Le serveur range le calque 8 (règle 25).
  bool served = false;
  // L'atelier est ouvert, ou le serveur ne l'exige pas.
  bool atelier_open = false;
  // La cadence du serveur permet d'envoyer.
  bool can_send = false;
  PaletteEffectSend send = PaletteEffectSend::kNone;
};

struct PaletteEffectsState {
  // Le numéro de l'effet choisi, ou zéro pour le premier.
  std::uint16_t selected = 0;
  // Les réglages en cours, par numéro d'effet. Un effet absent montre ce qu'il
  // porte.
  std::map<std::uint16_t, PaletteEffectSetting> drafts;
};

struct PaletteEffectsAction {
  // Envoyer le réglage en cours de cet effet.
  std::optional<std::uint16_t> validate;
  // Retirer la teinture de cet effet.
  std::optional<std::uint16_t> clear;
};

// LE RÉGLAGE QUE L'ONGLET MONTRE pour un effet : celui qu'on règle, ou celui
// qu'il porte.
[[nodiscard]] PaletteEffectSetting EffectDraftOf(const PaletteEffectsState& state,
                                                 const PaletteEffectRow& row);

// L'effet choisi : celui du numéro retenu, ou le premier ; nul sans effet.
[[nodiscard]] const PaletteEffectRow* SelectedEffect(const PaletteEffectsState& state,
                                                     const PaletteEffectsView& view);

// Ramène un réglage dans ses bornes ; « Assombrir » ne vaut que sur un effet
// additif.
[[nodiscard]] PaletteEffectSetting ClampedEffectSetting(PaletteEffectSetting setting,
                                                        bool additive);

// Pourquoi un effet ne se teint pas, déjà traduit ; vide sinon.
[[nodiscard]] const char* EffectBlockReason(PaletteEffectBlock blocked);

// Dessine le corps de l'onglet, sous la barre des onglets.
[[nodiscard]] PaletteEffectsAction DrawPaletteEffects(PaletteEffectsState& state,
                                                      const PaletteEffectsView& view,
                                                      const SkinArt& art);

}  // namespace ro::ui
