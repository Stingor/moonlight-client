#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE CHOIX DE NIVEAU (U11) : une modale centrée de 3 ou 4 cartes, le monde
// figé derrière (le grisé est la teinte d'écran d'`app`). Une carte se prend
// d'un clic ou par sa touche, 1 à 4 ; R relance, P passe, B arme le bannissement
// et la touche suivante bannit la carte dite. Ces trois boutons ne paraissent
// que si leur charge n'est pas nulle.
//
// 🔴 LA GARDE (400 ms à l'ouverture) EST CELLE DE L'ÉTAGE : tant que la vue dit
// `accepts_input` faux, ni touche ni clic ne rend d'intention.

// Ce qu'une carte offre, déjà résolu par `app`.
struct SurvivorsOfferCard {
  ImTextureID icon = 0;  // icône de skill ou illustration de carte ; 0 sans image
  ImVec2 icon_size = ImVec2(0.0F, 0.0F);
  const char* name = "";         // nom affiché, tiré des tables du jeu
  const char* description = "";  // ⚠ EN FRANÇAIS, marqué par `TrText` : traduit ici
  int owned_level = 0;           // 0 : « NEW! »
  int next_level = 1;
  bool evolves = false;  // la prendre ouvre une évolution
};

struct SurvivorsOfferView {
  std::span<const SurvivorsOfferCard> cards;
  bool accepts_input = false;
  // ⚠ LA RANGÉE DES CHIFFRES PAR SA PLACE, posée par `app` depuis les
  // scancodes : ImGui lit la touche par son caractère, et sur AZERTY la touche
  // « 1 » sans Maj est « & ». 0 : aucune ; 1 à 4 : la carte dite. Le pavé
  // numérique et la rangée QWERTY se lisent aussi par ImGui.
  int pressed_card_key = 0;
  int rerolls = 0;
  int skips = 0;
  int banishes = 0;
};

// Le bannissement armé tient d'une image à l'autre : la fenêtre le garde ici.
struct SurvivorsOfferState {
  bool banishing = false;
};

enum class SurvivorsOfferVerb : std::uint8_t { kPick, kReroll, kSkip, kBanish };
struct SurvivorsOfferAsk {
  SurvivorsOfferVerb verb = SurvivorsOfferVerb::kPick;
  std::uint8_t card_index = 0;  // pour kPick et kBanish
  [[nodiscard]] bool operator==(const SurvivorsOfferAsk&) const = default;
};

inline constexpr int kMostOfferCards = 4;

[[nodiscard]] std::optional<SurvivorsOfferAsk> DrawSurvivorsOfferWindow(
    SurvivorsOfferState& state, const SurvivorsOfferView& view, const SkinArt& art);

}  // namespace ro::ui
