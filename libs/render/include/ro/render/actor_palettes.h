#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include "ro/formats/palette.h"
#include "ro/formats/spr.h"
#include "ro/render/hue_classes.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/style_set.h"

namespace ro::render {

// LES PALETTES QUE PORTENT LES PIÈCES D'UN ACTEUR : la fusion d'une palette
// officielle sur celle d'un sprite, les rampes qu'on y détecte, et ce qu'un
// style en peint. La chaîne elle-même vit dans `ro/render/palette_ramps.h` ;
// ce module la garde.
//
// 🔴 UN CACHE PAR ÉTAGE, ET CHACUN À LA CLÉ DE CE DONT IL DÉPEND, rien de plus :
//   * l'emploi des index ne dépend que du SPRITE ;
//   * la base et ses rampes, du sprite et de la palette OFFICIELLE — et, pour
//     un calque, de son profil ;
//   * la palette peinte, du STYLE et de la PIÈCE en plus.
// Un joueur stylé qui change d'arme ne recalcule donc rien, et les images de
// son corps gardent leurs textures. Vingt joueurs de même classe et de même
// teinte se partagent une seule analyse.
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
    // Pour un corps : les rampes 9 à 12, que la suite du corps règle, et les
    // pixels qu'elles couvrent EN PLUS de `pixels_covered`.
    std::vector<PaletteRamp> continuation;
    int continuation_pixels = 0;
    // L'emploi des index du sprite, que garde le cache : de quoi mesurer le
    // reste.
    const std::array<int, formats::kPaletteEntryCount>* usage = nullptr;
  };

  // Ce qu'un style peint sur une pièce, et son numéro de série.
  struct StyledPalette {
    // ⚠ SANS OBJET pour un sprite qui ne porte aucune palette : seules ses
    // images en couleurs vraies se teignent, par `tint`.
    formats::Palette palette;
    // La teinture des images en couleurs vraies, au profil 2 ; rien sinon.
    std::optional<HueClassAdjusts> tint;
    std::uint32_t serial = 0;
  };

  // LA PIÈCE QU'ON PEINT : son calque, sa clé de teinture, et si la peau s'y
  // applique. Le défaut est un corps, peau comprise si le style en a une.
  struct Piece {
    StyleLayer layer = StyleLayer::kBody;
    std::uint32_t key = 0;
    SkinRace race = SkinRace::kHuman;
    // Faux pour une pièce qui n'a pas de peau, ou dont la peau ne se règle
    // pas : voir `SkinAdjustable`.
    bool skin = false;
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

  // La même chose pour un calque, découpé par le profil 1 : la tête sans sa
  // peau, les coiffes et la cape à leur seuil.
  [[nodiscard]] const Analysis& AnalyzeLayer(const formats::SprSprite& sprite,
                                             const formats::Palette* official, RampLayer layer,
                                             SkinRace race);

  // ── LES STYLES : des identifiants, et ce qu'ils peignent ────────────────
  //
  // Les règles sont celles de `ActorWardrobe::SetStyle`, qui les expose :
  // un identifiant ne se réécrit jamais et ne revient jamais.

  [[nodiscard]] std::uint32_t NewStyleId() noexcept { return ++last_style_id_; }
  [[nodiscard]] bool Knows(std::uint32_t style) const;
  // Un ensemble : la recette du corps et les teintures de calques. Refusé pour
  // l'identifiant nul ou déjà pris.
  bool SetStyle(std::uint32_t style, StyleSet set);
  // Une recette de corps seule, appliquée sur la base de chaque sprite qui la
  // porte.
  bool SetRecipe(std::uint32_t style, const PaletteRecipe& recipe);
  // Une palette de corps toute faite, la même pour tous les sprites.
  bool SetPalette(std::uint32_t style, formats::Palette palette);
  // Oublie le style et les palettes qu'il a peintes, dont les numéros de série
  // vont à `TakeRetiredSerials`.
  void Forget(std::uint32_t style);

  // Ce que ce style peint sur cette pièce, sous cette palette officielle.
  // Nulle quand le style est inconnu, qu'il n'a rien pour cette pièce, ou que
  // rien ne s'y applique : une recette sans palette de sprite, un profil
  // inconnu.
  //
  // ⚠ LA PALETTE DE CORPS TOUTE FAITE PASSE AVANT LA RECETTE : l'appelant a
  // déjà fusionné et peint. Seule la peau s'y pose encore, en dernier.
  [[nodiscard]] const StyledPalette* StyledFor(std::uint32_t style,
                                               const formats::SprSprite& sprite,
                                               const formats::Palette* official,
                                               const Piece& piece);
  // Le corps, sans peau.
  //
  // ⚠ UNE SURCHARGE ET NON UN ARGUMENT PAR DÉFAUT : g++ refuse `Piece{}` en
  // défaut tant que la classe qui l'entoure n'est pas complète.
  [[nodiscard]] const StyledPalette* StyledFor(std::uint32_t style,
                                               const formats::SprSprite& sprite,
                                               const formats::Palette* official) {
    return StyledFor(style, sprite, official, Piece{});
  }

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
  // Une source découpée par le profil d'un calque.
  struct LayerSource {
    Source source;
    RampLayer layer = RampLayer::kHead;
    SkinRace race = SkinRace::kHuman;

    [[nodiscard]] friend bool operator<(const LayerSource& left,
                                        const LayerSource& right) noexcept {
      if (left.source < right.source || right.source < left.source) {
        return left.source < right.source;
      }
      return std::tie(left.layer, left.race) < std::tie(right.layer, right.race);
    }
  };
  // Une pièce peinte par un style.
  //
  // 🔴 LA PIÈCE ENTRE DANS LA CLÉ, et pas seulement le sprite : deux
  // apparences de coiffe peuvent nommer le même sprite, et porter chacune sa
  // teinture ; un même corps porte sa peau ou non selon le chemin qui l'a
  // chargé.
  struct Painted {
    std::uint32_t style = 0;
    Source source;
    StyleLayer layer = StyleLayer::kBody;
    std::uint32_t key = 0;
    SkinRace race = SkinRace::kHuman;
    bool skin = false;

    [[nodiscard]] friend bool operator<(const Painted& left, const Painted& right) noexcept {
      if (left.style != right.style) {
        return left.style < right.style;
      }
      if (left.source < right.source || right.source < left.source) {
        return left.source < right.source;
      }
      return std::tie(left.layer, left.key, left.race, left.skin) <
             std::tie(right.layer, right.key, right.race, right.skin);
    }
  };

  [[nodiscard]] const std::array<int, formats::kPaletteEntryCount>& Usage(
      const formats::SprSprite& sprite);
  [[nodiscard]] Analysis Measured(const formats::Palette& base, std::vector<PaletteRamp> ramps,
                                  const std::array<int, formats::kPaletteEntryCount>& usage) const;
  // Peint une pièce, ou rien quand le style n'y change rien.
  [[nodiscard]] std::optional<StyledPalette> Paint(const StyleSet& set,
                                                   const formats::SprSprite& sprite,
                                                   const formats::Palette* official,
                                                   const formats::Palette* ready,
                                                   const Piece& piece);

  // ⚠ DES `std::map` : leurs nœuds ne bougent pas, et les pièces pointent dedans.
  std::map<const formats::SprSprite*, std::array<int, formats::kPaletteEntryCount>> usage_;
  std::map<Source, formats::Palette> merged_;
  std::map<Source, Analysis> analyses_;
  std::map<LayerSource, Analysis> layer_analyses_;
  std::map<std::uint32_t, StyleSet> sets_;
  // Les palettes de corps toutes faites, sorties de leur ensemble pour porter
  // leur numéro de série.
  std::map<std::uint32_t, StyledPalette> ready_made_;
  // ⚠ UNE PIÈCE QUE LE STYLE NE CHANGE PAS Y EST AUSSI, sans palette : sans
  // elle, chaque composition la redemanderait et referait le tri des
  // teintures pour rien.
  std::map<Painted, std::optional<StyledPalette>> painted_;
  std::vector<std::uint32_t> retired_serials_;
  // Le dernier numéro de série donné. Il ne fait que croître : c'est ce qui le
  // rend sûr là où l'adresse ne l'est pas.
  std::uint32_t last_serial_ = 0;
  std::uint32_t last_style_id_ = 0;
};

}  // namespace ro::render
