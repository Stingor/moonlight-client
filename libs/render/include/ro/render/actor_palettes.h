#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

#include "ro/formats/palette.h"
#include "ro/formats/spr.h"
#include "ro/render/palette_ramps.h"

namespace ro::render {

// LES PALETTES QUE PORTENT LES PIÈCES D'UN ACTEUR : la fusion d'une palette
// officielle sur celle d'un sprite, les rampes qu'on y détecte, et ce qu'un
// style en peint. La chaîne elle-même vit dans `ro/render/palette_ramps.h` ;
// ce module la garde.
//
// 🔴 UN CACHE PAR ÉTAGE, ET CHACUN À LA CLÉ DE CE DONT IL DÉPEND, rien de plus :
//   * l'emploi des index ne dépend que du SPRITE ;
//   * la base et ses rampes, du sprite et de la palette OFFICIELLE ;
//   * la palette peinte, du STYLE en plus.
// Un joueur stylé qui change de coiffe ou d'arme ne recalcule donc rien, et les
// images de son corps gardent leurs textures. Vingt joueurs de même classe et
// de même teinte se partagent une seule analyse.
//
// ⚠ LES CLÉS SONT DES ADRESSES : celles des sprites et des palettes officielles
// que la garde-robe a chargés, qui ne bougent pas et vivent aussi longtemps
// qu'elle. Une palette PEINTE, elle, meurt avec son style : d'où son numéro de
// série, voir `SpriteImageKey::palette_serial`.
class ActorPalettes {
 public:
  // CE QU'IL Y A À RÉGLER SUR UN SPRITE, sous une palette officielle.
  struct Analysis {
    // La palette officielle fusionnée sur celle du sprite, ou celle du sprite
    // seule : voir `Base`.
    const formats::Palette* base = nullptr;
    std::vector<PaletteRamp> ramps;
    // Les pixels peints, et ceux qu'une rampe couvre. L'écart est la part
    // qu'aucun réglage n'atteint.
    int pixels_total = 0;
    int pixels_covered = 0;
  };

  // Une palette qu'un style peint, et son numéro de série.
  struct StyledPalette {
    formats::Palette palette;
    std::uint32_t serial = 0;
  };

  // LA BASE : la palette officielle, trous rapatriés du sprite (voir
  // `MergeServerPalette`). Sans palette officielle, celle du sprite même.
  //
  // ⚠ LE SPRITE DOIT PORTER UNE PALETTE : à l'appelant de le vérifier.
  [[nodiscard]] const formats::Palette& Base(const formats::SprSprite& sprite,
                                             const formats::Palette* official);

  // Les rampes de la base et la part qu'elles couvrent, sous la même condition.
  //
  // ⚠ SANS STYLE : c'est ce qu'on RÈGLE. Détecter sur une palette déjà peinte
  // ferait dériver les rampes un peu plus à chaque passage.
  [[nodiscard]] const Analysis& Analyze(const formats::SprSprite& sprite,
                                        const formats::Palette* official);

  // ── LES STYLES : des identifiants, et ce qu'ils peignent ────────────────
  //
  // Les règles sont celles de `ActorWardrobe::SetStyle`, qui les expose :
  // un identifiant ne se réécrit jamais et ne revient jamais.

  [[nodiscard]] std::uint32_t NewStyleId() noexcept { return ++last_style_id_; }
  [[nodiscard]] bool Knows(std::uint32_t style) const;
  // Une recette, appliquée sur la base de chaque sprite qui la porte.
  bool SetRecipe(std::uint32_t style, const PaletteRecipe& recipe);
  // Une palette toute faite, la même pour tous les sprites.
  bool SetPalette(std::uint32_t style, formats::Palette palette);
  // Oublie le style et les palettes qu'il a peintes, dont les numéros de série
  // vont à `TakeRetiredSerials`.
  void Forget(std::uint32_t style);

  // Ce que ce style peint sur ce sprite, sous cette palette officielle. Nulle
  // quand le style est inconnu, ou que la recette n'a aucune palette de sprite
  // sur quoi s'appliquer.
  //
  // ⚠ UNE PALETTE TOUTE FAITE PASSE AVANT : l'appelant a déjà fusionné et
  // peint, et rien de ce qui suit n'aurait de sens sur elle.
  [[nodiscard]] const StyledPalette* StyledFor(std::uint32_t style,
                                               const formats::SprSprite& sprite,
                                               const formats::Palette* official);

  // LES NUMÉROS DE SÉRIE DES PALETTES OUBLIÉES depuis le dernier appel.
  //
  // 🔴 UN CACHE DE TEXTURES NE LES OUBLIE PAS DE LUI-MÊME : ses clés portent
  // l'adresse d'une palette détruite, et rien ne les redemandera jamais. C'est
  // à qui le tient de les effacer.
  [[nodiscard]] std::vector<std::uint32_t> TakeRetiredSerials();

 private:
  // Un sprite, et la palette officielle posée dessus — nulle : la sienne.
  struct Source {
    const formats::SprSprite* sprite = nullptr;
    const formats::Palette* official = nullptr;

    // ⚠ `std::less` SUR LES ADRESSES : c'est le seul ordre que la norme
    // définit entre objets sans lien.
    [[nodiscard]] friend bool operator<(const Source& left, const Source& right) noexcept {
      const std::less<const void*> before;
      if (left.sprite != right.sprite) {
        return before(left.sprite, right.sprite);
      }
      return before(left.official, right.official);
    }
  };
  struct Painted {
    std::uint32_t style = 0;
    Source source;

    [[nodiscard]] friend bool operator<(const Painted& left, const Painted& right) noexcept {
      if (left.style != right.style) {
        return left.style < right.style;
      }
      return left.source < right.source;
    }
  };

  [[nodiscard]] const std::array<int, formats::kPaletteEntryCount>& Usage(
      const formats::SprSprite& sprite);

  // ⚠ DES `std::map` : leurs nœuds ne bougent pas, et les pièces pointent dedans.
  std::map<const formats::SprSprite*, std::array<int, formats::kPaletteEntryCount>> usage_;
  std::map<Source, formats::Palette> merged_;
  std::map<Source, Analysis> analyses_;
  std::map<std::uint32_t, PaletteRecipe> recipes_;
  std::map<std::uint32_t, StyledPalette> ready_made_;
  std::map<Painted, StyledPalette> painted_;
  std::vector<std::uint32_t> retired_serials_;
  // Le dernier numéro de série donné. Il ne fait que croître : c'est ce qui le
  // rend sûr là où l'adresse ne l'est pas.
  std::uint32_t last_serial_ = 0;
  std::uint32_t last_style_id_ = 0;
};

}  // namespace ro::render
