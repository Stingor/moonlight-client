#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>

#include "ro/sim/actor.h"
#include "ro/sim/world_fwd.h"

#include "game/save_folder.h"

namespace moonclient {

// ── LES PNJ DE SERVICE RENDUS SOURDS AU CLIC GAUCHE ─────────────────────────
//
// Les PNJ de la capitale — Kafra, maître des métiers, styliste, passeurs — se
// tiennent au milieu du passage : on les clique en voulant marcher, et le
// dialogue s'ouvre. Le menu du clic droit propose une case qui les rend sourds
// au clic gauche, et elle SEULE décide : rien ne part au serveur.
//
// ⚠ SEULEMENT LES PNJ À IDENTIFIANT FIXE (`IsFixedIdNpc`) : les autres changent
// de GID à chaque démarrage du serveur, et une case cochée sur eux désignerait
// n'importe qui le lendemain.
//
// ⚠ PUREMENT CLIENT : un script déclenché en MARCHANT dessus s'exécute
// toujours, et la liste ne suit pas le joueur d'une machine à l'autre.
class BlockedNpcs {
 public:
  [[nodiscard]] bool Has(std::uint32_t id) const { return names_.contains(id); }
  [[nodiscard]] bool empty() const noexcept { return names_.empty(); }
  // GID → le nom relevé au moment du blocage. Purement informatif : il nomme la
  // ligne du panneau et la phrase de la conversation.
  [[nodiscard]] const std::map<std::uint32_t, std::string>& all() const noexcept { return names_; }

  // Bloque, ou débloque. Rend ce que le PNJ est DEVENU. ⚠ HORS DE LA PLAGE
  // RÉSERVÉE, RIEN NE SE BLOQUE.
  bool Toggle(std::uint32_t id, std::string_view name);
  void Put(std::uint32_t id, std::string name);
  // Rend vrai s'il était bloqué.
  bool Remove(std::uint32_t id);

  // ── CE QUI A DÉJÀ ÉTÉ DIT CETTE SESSION ─────────────────────────────────
  //
  // 🔴 UN BLOCAGE SURVIT AU FICHIER, ET LE JOUEUR L'OUBLIE. Des semaines plus
  // tard, il clique une Kafra qui ne répond pas et n'a plus aucune raison de
  // faire le lien avec une case cochée un soir. Le premier clic avalé de la
  // session lui redit donc pourquoi — une fois par PNJ, sinon la ligne
  // partirait à chaque clic.
  //
  // ⚠ JAMAIS ÉCRIT, et vidé en QUITTANT le monde — pas au changement de carte :
  // « une fois par session » veut dire une fois.

  // Vrai la PREMIÈRE fois qu'on le demande pour ce PNJ cette session.
  bool FirstExplanation(std::uint32_t id) { return explained_.insert(id).second; }
  void ForgetExplanations() noexcept { explained_.clear(); }

 private:
  std::map<std::uint32_t, std::string> names_;
  std::set<std::uint32_t> explained_;
};

// La phrase qui nomme un PNJ bloqué : son nom relevé, ou son GID faute de mieux
// — une phrase sans sujet ne dirait rien.
[[nodiscard]] std::string BlockedNpcLabel(std::uint32_t id, std::string_view name);

// LE CLIC GAUCHE SUR CET ACTEUR DOIT-IL ÊTRE AVALÉ ? Ni dialogue, ni marche :
// le PNJ devient une zone morte. Le premier clic avalé de la session le dit dans
// la conversation, à l'instant `now_ms`.
//
// ⚠ LE SURVOL N'EST PAS TOUCHÉ : plaque de nom et curseur restent ceux du jeu —
// un PNJ bloqué se voit et se vise toujours, il ne répond simplement plus. Et
// viser une zone avec un sort passe par-dessus lui : ce chemin-là ne désigne
// aucun acteur.
[[nodiscard]] bool SwallowsLeftClick(BlockedNpcs& blocked, bool enabled,
                                     const ro::sim::Actor& actor, ro::sim::World& world,
                                     std::uint32_t now_ms);

// La couleur des phrases que le client écrit lui-même dans la conversation,
// pour les distinguer de ce que dit le serveur. En `0xRRGGBB`.
inline constexpr std::uint32_t kClientSaysColor = 0xFFAA00;

// ── SUR LE DISQUE ────────────────────────────────────────────────────────────
//
// ⚠ SON PROPRE FICHIER, comme les favoris du coffre : c'est une LISTE, et le
// fichier de réglages ne sait décrire que des valeurs nommées.
inline constexpr std::string_view kBlockedNpcsFileName = "pnj_bloques.yaml";

[[nodiscard]] std::filesystem::path BlockedNpcsPath(const std::filesystem::path& client_root);

// Lit la liste. Un fichier absent, illisible ou vide ne bloque rien. ⚠ UNE
// ENTRÉE HORS DE LA PLAGE RÉSERVÉE SE JETTE : un GID dynamique recyclé viserait
// un jour une autre entité.
void LoadBlockedNpcs(const std::filesystem::path& client_root, BlockedNpcs& into);

// Écrit la liste. Rend faux si le fichier n'a pas pu être écrit.
[[nodiscard]] bool SaveBlockedNpcs(const std::filesystem::path& client_root,
                                   const BlockedNpcs& from);

}  // namespace moonclient
