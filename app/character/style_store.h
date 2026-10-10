#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
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

class StylePresets {
 public:
  struct Named {
    std::string name;
    ro::sim::StyleRecipe recipe;
  };

  // Range sous ce nom, en remplaçant un homonyme À SA PLACE — la liste ne se
  // réordonne pas sous les doigts de qui met à jour. Faux si le nom est vide
  // ou si le plafond est atteint.
  bool Save(std::string_view name, const ro::sim::StyleRecipe& recipe);

  [[nodiscard]] const ro::sim::StyleRecipe* Find(std::string_view name) const;
  void Erase(std::string_view name);

  // ⚠ DANS L'ORDRE OÙ ILS ONT ÉTÉ POSÉS, et c'est l'ordre du fichier : une
  // liste qui se réordonne à chaque écriture est illisible en diff, et
  // celui-ci se corrige à la main.
  [[nodiscard]] const std::vector<Named>& all() const noexcept { return kept_; }
  [[nodiscard]] bool full() const noexcept { return kept_.size() >= kMaxStylePresets; }

 private:
  std::vector<Named> kept_;
};

// ── LE BROUILLON ────────────────────────────────────────────────────────────

// 🔴 UN SEUL PAR CORPS, ÉCRASÉ À CHAQUE FOIS. Ce n'est pas un historique :
// c'est un FILET, et un filet à plusieurs mailles demanderait au joueur de
// choisir entre des états qu'il ne sait plus distinguer.
//
// 🔴 PAR CORPS, ET NON PAR PERSONNAGE — parce que ce client n'a pas
// d'identifiant de personnage : ce que le serveur lui donne est l'identifiant
// de COMPTE, qui vaut pour tous les personnages de ce joueur. Rendre un
// brouillon sur cette clé-là le poserait indifféremment sur un Chevalier et sur
// un Forgeron.
//
// ⚠ ET LA CLÉ DE CORPS EST CELLE QUI A UN SENS : une recette ne désigne ses
// pièces que par des index de palette, et les index n'ont aucune signification
// commune d'un corps à l'autre. Un brouillon retrouvé sur le même corps est
// exactement celui qu'on composait ; retrouvé sur un autre, il ne voudrait
// rien dire. Deux personnages de même classe partagent donc leur brouillon —
// et sur le même corps, c'est la bonne réponse.
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

inline constexpr std::string_view kStylePresetsFileName = "style_prereglages.yaml";
inline constexpr std::string_view kStyleDraftsFileName = "style_brouillon.yaml";

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
void LoadStyleDrafts(const std::filesystem::path& client_root, StyleDrafts& into);

// Écrivent. Rendent faux si le fichier n'a pas pu être écrit.
[[nodiscard]] bool SaveStylePresets(const std::filesystem::path& client_root,
                                    const StylePresets& from);
[[nodiscard]] bool SaveStyleDrafts(const std::filesystem::path& client_root,
                                   const StyleDrafts& from);

}  // namespace moonclient
