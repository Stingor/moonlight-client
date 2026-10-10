#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// L'ONGLET MERCENAIRE DE LA FICHE : la fiche d'état du mercenaire et la liste de
// ses compétences, sur le patron de l'onglet de l'homoncule.
//
// ⚠ IL NE TIENT AUCUN ÉTAT DE JEU. `app` compose la vue — textes déjà traduits,
// nombres déjà calculés — et route ce que l'onglet demande.

// UNE COMPÉTENCE DE LA LISTE.
struct MercenarySkillRow {
  std::uint16_t skill_id = 0;
  std::uint16_t level = 0;
  // Le nom du jeu, ou à défaut le nom interne du serveur.
  std::string_view name;
  // Le nom INTERNE (« MS_BASH »), que seul le staff voit au survol.
  std::string_view idname;
  // Vide quand l'installation n'a pas l'icône.
  SkinImage icon;
  // Elle n'est pas passive. `app` tranche, d'après les drapeaux du serveur.
  bool active = false;
  // Elle part d'ici — double-clic, ou case d'une barre : celle qui vise une
  // cible ou une case s'arme, et l'IA du mercenaire la lance. `app` tranche.
  bool castable = false;
  // Elle vise une cible ou une case : l'infobulle dit que le clic suivant la
  // désigne. `app` tranche.
  bool needs_aim = false;
  int sp_cost = 0;
  int range_cells = 0;
};

struct MercenaryTabView {
  // Vrai quand le serveur a dit qu'il existe.
  bool present = false;
  // LE DROIT DU STAFF, relu par `app` à chaque image : l'identifiant paraît au
  // survol d'une compétence.
  bool staff_allowed = false;

  std::string_view name;
  // « Mercenaire : Archer », déjà traduit.
  std::string_view kind;
  int level = 0;

  int hp = 0;
  int max_hp = 0;
  int sp = 0;
  int max_sp = 0;

  // Le temps de contrat qui reste, et la date de sa fin, déjà écrite à l'heure
  // locale.
  std::uint32_t contract_left_s = 0;
  std::string_view expires_at;
  // LA FIN EST PROCHE : le temps restant s'écrit en rouge. `app` en tient le
  // seuil, celui de son premier rappel.
  bool contract_ending = false;
  int faith = 0;
  std::uint32_t calls = 0;
  std::uint32_t kills = 0;

  // Les huit statistiques, dans l'ordre de la fiche du jeu.
  int atk = 0;
  int matk = 0;
  int hit = 0;
  int crit = 0;
  int def = 0;
  int mdef = 0;
  int flee = 0;
  int aspd = 0;
  // Le délai d'attaque d'où l'ASPD se déduit, en millisecondes.
  int amotion_ms = 0;
  int range_cells = 0;

  std::span<const MercenarySkillRow> skills;

  // LE PROFIL D'IA EN VIGUEUR, déjà traduit ; vide sans IA à régler.
  std::string_view ai_profile;
  // LA PROPOSITION DE RECOPIE de profil, déjà traduite ; vide sans elle.
  std::string_view ai_proposal;
};

// La taille du tampon de confirmation : un nom du serveur et son terminateur.
inline constexpr std::size_t kMercenaryConfirmSize = 64;

struct MercenaryTabState {
  // La confirmation du renvoi est dépliée, et ce qui y a été tapé.
  bool dismiss_asked = false;
  std::array<char, kMercenaryConfirmSize> dismiss_confirm = {};
  // Le compte rendu du dernier geste.
  std::string status;
  // La section des compétences est dépliée.
  bool skills_open = true;
};

// UNE COMPÉTENCE À LANCER, au niveau appris.
struct MercenarySkillCast {
  std::uint16_t skill_id = 0;
  std::uint16_t level = 0;
};

struct MercenaryTabAction {
  std::optional<MercenarySkillCast> cast;
  // La compétence dont on veut la description, zéro pour aucune.
  std::uint16_t describe_skill_id = 0;
  // Le renvoi, confirmé.
  bool dismiss = false;
  // Ouvrir la fenêtre « IA du compagnon ».
  bool open_ai = false;
  // La réponse à la proposition de recopie : vrai pour « Recopier ».
  std::optional<bool> ai_proposal_answer;
};

[[nodiscard]] MercenaryTabAction DrawMercenaryTab(MercenaryTabState& state,
                                                  const MercenaryTabView& view, const SkinArt& art);

}  // namespace ro::ui
