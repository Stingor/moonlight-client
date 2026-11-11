#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/style.h"

#include "game/save_folder.h"

namespace moonclient {

// CE QUE L'ÉDITEUR DE STYLE GARDE SUR LE DISQUE, et qui n'appartient qu'au
// joueur.
//
// ⚠ PUREMENT LOCAL : le serveur n'en sait rien et n'a pas à en savoir. Un
// préréglage n'est pas une apparence, c'est un brouillon qu'on reprend ; et le
// brouillon proprement dit n'est même pas un choix, c'est un accident qu'on
// rattrape.
//
// 🔴 DEUX FICHIERS, ET C'EST LEUR NATURE QUI LES SÉPARE. Un préréglage est un
// choix NOMMÉ que le joueur a posé et qui doit lui survivre ; un brouillon
// s'écrase sans prévenir et disparaît dès qu'il est validé. Les mélanger
// obligerait à distinguer à la lecture ce qu'on distingue ici gratuitement, et
// un effacement de brouillon irait toucher le fichier où dorment des
// préréglages.
//
// ⚠ ILS RANGENT DES CODES, la même chaîne que le presse-papiers et que la base
// du serveur — voir `ro/sim/style_code.h`. Le joueur peut donc recopier un
// préréglage directement depuis le fichier, et un changement de format les
// invalide tous ENSEMBLE plutôt que d'en laisser un servir des recettes mal
// alignées.

// ── LES PRÉRÉGLAGES ─────────────────────────────────────────────────────────

// Le plafond. ⚠ IL EXISTE POUR QUE LA LISTE RESTE UNE LISTE : au-delà, on ne
// retrouve plus ce qu'on y a mis, et c'est un gestionnaire qu'il faudrait.
inline constexpr std::size_t kMaxStylePresets = 32;

// 🔴 UN PRÉRÉGLAGE EST UNE TENUE DU PERSONNAGE, brouillon compris : le corps,
// et les calques qui lui appartiennent — la tête, la peau du visage, la suite
// et la peau du corps, avec leur reste et leurs retouches. ⚠ JAMAIS une
// teinture de coiffe ou de cape : elle appartient à un exemplaire d'objet, et
// voyage par la garde-robe du compte et le code court.
class StylePresets {
 public:
  struct Named {
    std::string name;
    ro::sim::StyleRecipe recipe;
    // Sous les clés du moment où on l'a rangé ; `RekeyPresetLayers` les
    // reporte sur ce qu'on porte au chargement.
    std::vector<ro::sim::StyleLayerRecipe> layers;
  };

  // Range sous ce nom, en remplaçant un homonyme À SA PLACE — la liste ne se
  // réordonne pas sous les doigts de qui met à jour. Faux si le nom est vide
  // ou si le plafond est atteint. Seuls les calques du personnage sont gardés.
  bool Save(std::string_view name, const ro::sim::StyleRecipe& recipe,
            std::vector<ro::sim::StyleLayerRecipe> layers = {});

  [[nodiscard]] const ro::sim::StyleRecipe* Find(std::string_view name) const;
  [[nodiscard]] const Named* FindNamed(std::string_view name) const;
  void Erase(std::string_view name);
  // Renomme À SA PLACE. Faux si le nouveau nom est vide ou déjà pris, ou si
  // l'ancien n'existe pas.
  bool Rename(std::string_view from, std::string_view to);

  // ⚠ DANS L'ORDRE OÙ ILS ONT ÉTÉ POSÉS, et c'est l'ordre du fichier : une
  // liste qui se réordonne à chaque écriture est illisible en diff, et
  // celui-ci se corrige à la main.
  [[nodiscard]] const std::vector<Named>& all() const noexcept { return kept_; }
  [[nodiscard]] bool full() const noexcept { return kept_.size() >= kMaxStylePresets; }

 private:
  std::vector<Named> kept_;
};

// Un calque qu'un préréglage peut porter : la tête, la peau du visage, la suite
// et la peau du corps.
[[nodiscard]] bool PresetKeepsLayer(ro::sim::StyleLayerKind layer) noexcept;

// CE QU'UN CHARGEMENT REPREND d'un préréglage.
enum class PresetLayers : std::uint8_t {
  // Tous ses calques.
  kAll,
  // Ceux du corps : la suite et la peau du corps.
  kBody,
  // Ceux de la tête : la tête et la peau du visage.
  kHead,
};

// Les clés de ce qu'on porte, sous lesquelles un chargement range les calques.
struct PresetKeys {
  std::uint32_t head = 0;
  std::uint32_t body = 0;
};

// LES CALQUES D'UN PRÉRÉGLAGE REPORTÉS SUR CE QU'ON PORTE : la tête sous la tête
// d'aujourd'hui, la suite et la peau du corps sous le corps qu'on règle.
//
// 🔴 UNE RETOUCHE NE SURVIT PAS À UN CHANGEMENT DE CLÉ : elle désigne un index
// de la palette d'un sprite, qui n'a pas de sens sur un autre. Les pièces et le
// reste, réglés par rang et par classe de teinte, se reportent tels quels.
[[nodiscard]] std::vector<ro::sim::StyleLayerRecipe> RekeyPresetLayers(
    std::span<const ro::sim::StyleLayerRecipe> kept, PresetKeys now, PresetLayers part);

// ── LE BROUILLON ────────────────────────────────────────────────────────────

// 🔴 UN SEUL PAR CORPS, ÉCRASÉ À CHAQUE FOIS. Ce n'est pas un historique :
// c'est un FILET, et un filet à plusieurs mailles demanderait au joueur de
// choisir entre des états qu'il ne sait plus distinguer.
//
// 🔴 PAR PERSONNAGE, PUIS PAR CORPS. Le personnage, parce que deux
// personnages de même classe ne composent pas la même chose : l'essai de l'un
// n'a pas à se proposer à l'autre. Le corps, parce qu'une recette ne désigne
// ses pièces que par des index de palette, qui n'ont aucune signification
// commune d'un corps à l'autre.
//
// ⚠ CETTE CLASSE NE TIENT QU'UN PERSONNAGE, celui de la partie : c'est le
// fichier qui les range tous — voir `LoadStyleDrafts`.
class StyleDrafts {
 public:
  // Rend vrai si quelque chose a CHANGÉ — donc s'il faut écrire. Un brouillon
  // identique à celui déjà rangé ne vaut pas une écriture de fichier.
  bool Put(std::uint32_t body_key, const ro::sim::StyleRecipe& recipe);
  bool Forget(std::uint32_t body_key);

  [[nodiscard]] const ro::sim::StyleRecipe* Find(std::uint32_t body_key) const;
  [[nodiscard]] bool Has(std::uint32_t body_key) const { return Find(body_key) != nullptr; }

  [[nodiscard]] const std::map<std::uint32_t, ro::sim::StyleRecipe>& all() const noexcept {
    return kept_;
  }

 private:
  std::map<std::uint32_t, ro::sim::StyleRecipe> kept_;
};

// ── SUR LE DISQUE ───────────────────────────────────────────────────────────

inline constexpr std::string_view kStylePresetsFileName = "style_prereglages.yml";
inline constexpr std::string_view kStyleDraftsFileName = "style_brouillon.yml";

[[nodiscard]] std::filesystem::path StylePresetsPath(const std::filesystem::path& client_root);
[[nodiscard]] std::filesystem::path StyleDraftsPath(const std::filesystem::path& client_root);

// Lisent ce qui est rangé. Un fichier absent est le cas NORMAL de la première
// partie, et un fichier abîmé ne doit pas empêcher de jouer.
//
// 🔴 UNE ENTRÉE D'UNE AUTRE VERSION EST ÉCARTÉE DÈS LA LECTURE. Elle ne se
// décodera jamais — le format refuse la migration, voir `style_code.h` —, si
// bien que la garder donnerait une ligne qui ne fait rien quand on clique
// dessus, ce qui se lit comme une panne. L'argument vaut pour un brouillon
// comme pour un préréglage, dont le nom seul ne vaut rien sans son style.
void LoadStylePresets(const std::filesystem::path& client_root, StylePresets& into);
// ⚠ LES BROUILLONS SONT RANGÉS PAR PERSONNAGE : on ne lit et n'écrit que ceux
// de `character_id`, et l'écriture garde ceux des autres.
void LoadStyleDrafts(const std::filesystem::path& client_root, std::uint32_t character_id,
                     StyleDrafts& into);

// Écrivent. Rendent faux si le fichier n'a pas pu être écrit.
[[nodiscard]] bool SaveStylePresets(const std::filesystem::path& client_root,
                                    const StylePresets& from);
[[nodiscard]] bool SaveStyleDrafts(const std::filesystem::path& client_root,
                                   std::uint32_t character_id, const StyleDrafts& from);

}  // namespace moonclient
