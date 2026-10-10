#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA CONFIRMATION D'UNE SUPPRESSION DÉFINITIVE.
//
// 🔴 ELLE EXISTE PARCE QUE LE GESTE EST IRRÉVERSIBLE, et qu'un bouton unique
// suffirait à perdre un personnage de dix ans sur un clic mal placé. Le serveur
// exige de son côté le secret du compte (`char_del_option`, donc l'email en
// production) : cette fenêtre est l'endroit où on le saisit, et sa lenteur est
// le but.
//
// ⚠ ELLE N'EST PAS LE PREMIER TEMPS. La suppression se RÉSERVE depuis la
// tablée, sans rien demander, et n'aboutit qu'une heure plus tard. Cette
// fenêtre est le SECOND temps, et le bouton qui l'ouvre reste gris tant que
// l'échéance n'est pas atteinte.

// LA TAILLE DU CHAMP DU SECRET, terminateur compris.
//
// 🔴 ELLE VIENT DU FIL : `CH_DELETE_CHAR` porte la clé sur CINQUANTE octets, et
// le serveur n'en lit que quarante (`char email[40]`, `chclif_parse_delchar`).
// On s'aligne sur ce qu'il LIT : laisser taper au-delà promettrait une adresse
// qu'il tronquerait, et son refus se lirait « adresse incorrecte » sur une
// adresse juste.
inline constexpr std::size_t kDeleteSecretBytes = 40;

// ⚠ SÉPARÉS, comme partout : ImGui range l'état d'une fenêtre sous son TITRE,
// qui se traduit.
inline constexpr const char* kDeleteWindowTitle = core::TrText("Suppression définitive");
inline constexpr const char* kDeleteWindowId = "character_delete";

// CE QUE LE JOUEUR A TAPÉ. Il appartient à l'APPELANT.
struct CharacterDeleteState {
  // Le personnage visé ; zéro quand la fenêtre est fermée.
  //
  // 🔴 L'IDENTIFIANT ET NON L'EMPLACEMENT, et la cible se FIGE à l'ouverture :
  // la liste peut changer sous la fenêtre — une création, une suppression
  // ailleurs —, et un emplacement retenu désignerait alors quelqu'un d'autre.
  std::uint32_t character_id = 0;
  // Le nom, recopié à l'ouverture pour la même raison.
  std::string name;
  std::array<char, kDeleteSecretBytes> secret = {};

  [[nodiscard]] bool open() const noexcept { return character_id != 0; }
};

struct CharacterDeleteView {
  // La demande est partie ; on attend la réponse du serveur.
  bool pending = false;
  // Le motif du dernier refus, EN CLAIR et déjà traduit, ou vide.
  std::string refusal;
};

enum class CharacterDeleteVerb : std::uint8_t {
  kNothing,
  // Supprimer pour de bon, avec le secret que l'état porte.
  kConfirm,
  kCancel,
};

struct CharacterDeleteAction {
  CharacterDeleteVerb verb = CharacterDeleteVerb::kNothing;
};

// DESSINE LA CONFIRMATION. Sans effet quand l'état est fermé.
[[nodiscard]] CharacterDeleteAction DrawCharacterDelete(CharacterDeleteState& state,
                                                        const CharacterDeleteView& view,
                                                        const SkinArt& art);

}  // namespace ro::ui
