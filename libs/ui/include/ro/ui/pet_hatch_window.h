#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/ui/chat_link.h"
#include "ro/ui/pet_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LA LISTE D'ÉCLOSION ────────────────────────────────────────────────────
//
// Le serveur l'ouvre quand on lance l'éclosion : un œuf par ligne, que l'on
// choisit puis fait éclore. Une fenêtre à part de la fiche — elle paraît
// justement quand aucun familier n'est sorti.
//
// Ce que le jeu ne disait pas, et qui coûtait le geste : un œuf VIERGE figure
// dans la liste, mais le serveur refuse de l'éclore sans un mot. Il est marqué
// ici, et le bouton refuse avec la raison.

// Ce qu'une ligne dit de son œuf.
enum class PetEggState : std::uint8_t {
  // L'œuf porte un familier.
  kInhabited,
  // L'œuf est vide : le serveur refusera de l'éclore.
  kBlank,
  // Le serveur a listé une case que le sac ne porte pas.
  kMissing,
};

struct PetEggRowView {
  // La case à renvoyer telle quelle.
  std::uint16_t client_slot = 0;
  std::uint32_t item_id = 0;
  SkinImage icon;
  std::string name;
  PetEggState state = PetEggState::kInhabited;
  // Le mot du rang d'intimité, quand l'œuf le porte.
  std::optional<PetIntimacyLabel> intimacy;
  bool renamed = false;
};

struct PetHatchWindowView {
  std::vector<PetEggRowView> rows;
};

struct PetHatchWindowState {
  bool open = false;
  bool focus_asked = false;
  // La CASE choisie, pas la ligne : une liste réémise ne doit pas déplacer la
  // sélection en silence.
  std::optional<std::uint16_t> selected_slot;
};

struct PetHatchWindowAction {
  // La case de l'œuf à faire éclore.
  std::optional<std::uint16_t> hatch;
  // Le clic droit sur une ligne : la description de l'œuf.
  ChatLinkAsk link;
};

// Ouvre la liste, sans sélection.
void OpenPetHatchWindow(PetHatchWindowState& state);

[[nodiscard]] PetHatchWindowAction DrawPetHatchWindow(PetHatchWindowState& state,
                                                      const PetHatchWindowView& view,
                                                      const SkinArt& art);

}  // namespace ro::ui
