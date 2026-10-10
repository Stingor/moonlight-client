#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// CE QU'UN ÉTAT FAIT, montré au SURVOL.
//
// 🔴 UNE INFOBULLE, ET PAS UNE FENÊTRE : le client d'origine n'a pas de
// fenêtre de description d'état. Tout ce qu'on sait d'un état — son nom, son
// effet, son icône — tient dans une bulle, et une fenêtre de plus ne dirait
// rien de neuf tout en demandant à être refermée.
//
// ⚠ ELLE NE MONTRE AUCUNE ÉCHÉANCE. Ce qu'on survole ici est un ÉTAT, pas une
// occurrence en cours : un compte à rebours donnerait à croire qu'il tourne.
// Celui d'une occurrence vit sur la case qui la porte — voir `status_cell.h`.

// Ce que la bulle montre. `app` a déjà croisé la table de l'installation : `ui`
// ne sait ni lire un Lua ni charger une icône.
struct StatusPreviewView {
  // Zéro : rien à montrer, et rien n'est dessiné.
  std::uint32_t effect_id = 0;
  // Vide quand la table ne nomme pas cet état : la bulle écrit alors son
  // numéro, qui est la clé qu'on retrouve dans `efstids`.
  std::string_view name;
  // CE QUE L'ÉTAT FAIT, ligne à ligne. Vide quand la table ne l'explique pas —
  // six cent trente-trois en portent une dans cette installation.
  std::span<const std::string_view> description;
  // L'image, quand l'installation la dessine. Les deux tables du client ne se
  // recouvrent pas : un état nommé n'a pas forcément d'icône.
  SkinImage icon;
};

// LA TAILLE DE L'ICÔNE DANS LA BULLE. ⚠ PLUS GRANDE QUE DANS LA BARRE : on
// regarde ici, on compte là-bas.
inline constexpr float kStatusPreviewIconPixels = 32.0F;

// L'aperçu. Ne dessine rien quand aucun état n'est désigné.
void DrawStatusPreview(const StatusPreviewView& view, const SkinArt& art);

}  // namespace ro::ui
