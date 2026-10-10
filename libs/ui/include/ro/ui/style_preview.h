#pragma once

#include <cstdint>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/palette_editor.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// L'APERÇU D'UN STYLE REÇU : un pantin — LE NÔTRE — habillé de ce style.
//
// 🔴 C'EST LE SEUL APERÇU HONNÊTE POSSIBLE. Une recette ne porte aucune
// couleur : elle ne dit que des décalages par pièce, et ne veut rien dire hors
// d'un sprite. « Ce que ça donnerait sur moi » est exactement la question que
// se pose celui qui clique sur le lien de quelqu'un d'autre.
//
// ⚠ INDÉPENDANTE DE L'ÉDITEUR, et il le faut : elle s'ouvre depuis le fil,
// donc à un moment où l'éditeur peut n'avoir jamais servi — et refermer l'un
// ne doit pas refermer l'autre. Elle ne partage avec lui que le PANTIN.
//
// ⛔ ELLE NE MONTRE NI PIÈCES NI RÉGLAGES. Ce n'est pas un éditeur au rabais :
// on regarde un style, on ne le retouche pas. Pour cela il y a « Essayer dans
// l'éditeur », qui l'y charge sans rien appliquer au personnage.

struct StylePreviewState {
  bool open = false;
  // Le code reçu, TEL QU'IL A VOYAGÉ. ⚠ JAMAIS RÉ-ENCODÉ : un aller-retour
  // perdrait ce qu'une version future y ajouterait, et ce qu'on relaie ne
  // serait plus ce qu'on a reçu.
  std::string code;
  // L'étiquette qui l'accompagnait — un pseudo, un nom de préréglage. Peut
  // être vide : la balise l'autorise.
  std::string owner;
  // Le pantin, le même que celui de l'éditeur. ⚠ SA PIPETTE NE SERT PAS ICI —
  // il n'y a pas de liste de pièces à désigner —, et personne ne la lit.
  PaletteDollState doll;
};

// CE QUE L'APERÇU DEMANDE.
enum class StylePreviewVerb : std::uint8_t {
  kNothing,
  // Charger ce code dans l'éditeur. ⚠ N'APPLIQUE RIEN : il faudra valider.
  kTry,
  // Le code nu dans le presse-papiers.
  kCopy,
};

// Ce que la fenêtre montre. Le pantin arrive par `state.doll.image`, comme
// pour l'éditeur : c'est `app` qui compose, elle qui pose.
struct StylePreviewView {
  // 🔴 VRAI SEULEMENT QUAND ON SAIT QU'IL N'Y A RIEN À PEINDRE : pas de sprite
  // chargé, ou aucune pièce de ce corps qui se règle.
  //
  // ⚠ CE N'EST PAS « L'IMAGE N'EST PAS ENCORE LÀ ». La boîte du pantin est
  // mesurée AU DESSIN, si bien que la première image n'a jamais rien à poser :
  // confondre les deux ferait clignoter un message d'échec à chaque ouverture,
  // pour une fenêtre qui se remplit correctement l'image d'après.
  bool bodyless = false;
};

[[nodiscard]] StylePreviewVerb DrawStylePreview(StylePreviewState& state,
                                                const StylePreviewView& view, const SkinArt& art);

inline constexpr const char* kStylePreviewTitle = core::TrText("Aperçu d'un style");
inline constexpr const char* kStylePreviewId = "style_preview";

}  // namespace ro::ui
