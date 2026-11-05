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
  bool evolves = false;    // la prendre ouvre une évolution
  bool objective = false;  // elle mène au sort épinglé en objectif au Codex
  // EN COOP : le héros qui la recevra, ses voix, et si le joueur a voté pour elle.
  const char* for_hero = nullptr;
  int votes = 0;
  bool my_vote = false;
};

// UN HÉROS À RELEVER, au vote d'une montée de niveau en coop.
struct SurvivorsOfferRaise {
  std::uint8_t hero_index = 0;
  const char* name = "";
  int votes = 0;
  bool my_vote = false;
};

struct SurvivorsOfferView {
  std::span<const SurvivorsOfferCard> cards;
  bool accepts_input = false;
  // ⚠ LA RANGÉE DES CHIFFRES PAR SA PLACE, posée par `app` depuis les
  // scancodes : ImGui lit la touche par son caractère, et sur AZERTY la touche
  // « 1 » sans Maj est « & ». 0 : aucune ; 1 à 4 : la carte dite. Le pavé
  // numérique et la rangée QWERTY se lisent aussi par ImGui.
  int pressed_card_key = 0;
  // UNE SAISIE EST OUVERTE : les frappes vont au texte. Les touches se taisent,
  // la fenêtre ne prend plus le clavier, et la souris répond toujours.
  bool typing = false;
  int rerolls = 0;
  int skips = 0;
  int banishes = 0;
  // Vrai au défi et en partie classée, qui partent sans charges Kafra : la
  // fenêtre le dit, pour qu'aucun joueur ne les cherche.
  bool charges_withheld = false;
  // EN COOP : le vote. Sans votants, le choix est celui du joueur seul.
  int voters = 0;
  float vote_time_left = 0.0F;  // de 1 à 0
  int leaves = 0;
  std::span<const SurvivorsOfferRaise> raises;
};

// Le bannissement armé tient d'une image à l'autre : la fenêtre le garde ici.
struct SurvivorsOfferState {
  bool banishing = false;
};

enum class SurvivorsOfferVerb : std::uint8_t { kPick, kReroll, kSkip, kBanish, kRaise };
struct SurvivorsOfferAsk {
  SurvivorsOfferVerb verb = SurvivorsOfferVerb::kPick;
  std::uint8_t card_index = 0;  // pour kPick et kBanish ; le héros pour kRaise
  [[nodiscard]] bool operator==(const SurvivorsOfferAsk&) const = default;
};

inline constexpr int kMostOfferCards = 4;

[[nodiscard]] std::optional<SurvivorsOfferAsk> DrawSurvivorsOfferWindow(
    SurvivorsOfferState& state, const SurvivorsOfferView& view, const SkinArt& art);

}  // namespace ro::ui
