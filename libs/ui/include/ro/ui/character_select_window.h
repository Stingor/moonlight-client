#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/character_seats.h"
#include "ro/ui/seat_layout.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/wheel_gate.h"

namespace ro::ui {

// LA SCÈNE DU BANQUET : une salle peinte, vingt-cinq sièges, et les convives.
//
// 🔴 CE N'EST PAS L'ÉCRAN DE SÉLECTION DU CLIENT D'ORIGINE, et c'est une
// décision de produit, pas un manque. Le natif aligne trois vignettes dans un
// cadre et fait défiler les emplacements trois par trois ; celui-ci montre
// toute la maisonnée d'un coup, attablée. C'est ce que les joueurs de ce
// serveur connaissent, jusqu'aux trois millièmes de `kBanquetSeats`.
//
// ⚠ ELLE OCCUPE TOUT L'ÉCRAN, seule fenêtre de ce client à le faire. Ce n'est
// pas une fenêtre de jeu posée sur un décor : c'est le décor lui-même, et la
// ville vivante passe derrière elle, pas dessous.

// UN CONVIVE, ou une place.
//
// ⚠ DES DONNÉES PURES, et c'est l'éditeur de liens qui l'impose : `ro_ui` ne
// lie ni `proto` ni `sim`, donc `CharacterInfo` lui est injoignable. `app`
// recopie ce dont l'écran a besoin — et c'est d'ailleurs juste : une ligne de
// liste est un artefact de PROTOCOLE, pas un morceau du monde.
struct CharacterSlotView {
  // ⚠ UN DRAPEAU, ET NON UN NOM VIDE : un personnage dont le nom n'a pas été
  // décodé est une ligne abîmée, pas une place libre, et les deux ne se
  // dessinent pas pareil.
  bool occupied = false;
  std::string name;
  std::int32_t base_level = 0;
  std::int32_t job_level = 0;
  // L'identifiant du personnage. ⚠ LES TROIS PAQUETS DE SUPPRESSION LE
  // PORTENT, jamais l'emplacement : c'est lui que la scène rend dans son
  // intention.
  std::uint32_t character_id = 0;
  // COMBIEN DE SECONDES AVANT QUE LA SUPPRESSION SOIT POSSIBLE.
  //
  // ⚠ MOINS UN QUAND ELLE N'EST PAS PROGRAMMÉE, zéro quand l'heure est venue.
  // Trois états, et le troisième n'est pas « zéro seconde restante » : un
  // personnage qu'on n'a pas mis en file et un personnage dont l'heure est
  // arrivée ne se dessinent pas pareil, et ne proposent pas le même geste.
  //
  // ⚠ IL VIENT DE `app` : `DelRevDate` est une date, et la comparer demande
  // une horloge que cette couche n'a pas.
  int delete_in_seconds = -1;

  [[nodiscard]] bool doomed() const noexcept { return delete_in_seconds >= 0; }
  // LE PANTIN, déjà composé et téléversé par `app` ; sa taille est celle de la
  // BOÎTE, dont le bas est la ligne du siège (voir `SeatFrame`).
  //
  // Inutilisable tant que les sprites ne répondent pas : la place montre alors
  // une silhouette d'attente, de même encombrement, plutôt qu'un trou qui
  // ferait sauter la scène quand elle se remplit.
  SkinImage doll;
};

// LE FONDU AU NOIR DE L'ENTRÉE EN JEU, en secondes.
//
// 🔴 LA CARTE SE CHARGE EN BLOQUANT, et la dernière image présentée reste à
// l'écran tout ce temps. Sans ce voile, ce serait la tablée — qui a l'air
// FIGÉE, pas occupée. `app` ne rend donc la main qu'une fois le voile complet.
inline constexpr float kEnterFadeSeconds = 0.26F;

// CE QUE LA SCÈNE SAIT.
struct CharacterSelectView {
  // Les emplacements, dans l'ordre du serveur.
  //
  // ⚠ VIDE EST UNE RÉPONSE, et c'est celle d'un compte qui vient d'être créé.
  std::vector<CharacterSlotView> slots;
  // LA SALLE. Sans elle, la scène se dessine tout de même — sur le décor
  // vivant, qui n'est pas un fond neutre mais fait l'affaire bien mieux qu'un
  // aplat.
  SkinImage hall;
  // COMBIEN D'EMPLACEMENTS CE COMPTE PEUT REMPLIR.
  //
  // 🔴 CE N'EST PAS `slots.size()`, et les deux diffèrent dans les DEUX sens.
  // La vue s'étend jusqu'au plus haut emplacement OCCUPÉ, qui peut dépasser ce
  // que le compte peut remplir — un abonnement qui expire réduit le compte sans
  // effacer les personnages qui étaient au-delà, et ceux-là ne se créent plus
  // mais se jouent encore. En sens inverse, un compte neuf a neuf places
  // libres et aucun personnage.
  //
  // ⚠ C'est ce nombre, et lui seul, qui décide où un « + » se propose.
  int creatable_slots = 0;
  // Un refus en clair, déjà traduit, ou vide.
  std::string message;
  // LE CHOIX EST PARTI, LA CARTE SE MONTE.
  //
  // ⚠ LA SCÈNE RESTE, INERTE. L'effacer laisserait un écran noir de plusieurs
  // secondes — le temps de la carte —, et le joueur ne saurait pas si son clic
  // a porté.
  bool busy = false;
  // DEPUIS COMBIEN DE SECONDES LE CHOIX EST PARTI : c'est lui qui fait monter
  // le voile noir de l'entrée en jeu. Voir `kEnterFadeSeconds`.
  float entering_seconds = 0.0F;
  // UNE FENÊTRE DE CRÉATION OU DE SUPPRESSION EST POSÉE SUR LA SCÈNE. F10 ne
  // bascule alors pas la personnalisation : le joueur se trouverait devant deux
  // interactions concurrentes.
  bool dialog_open = false;

  // ── CE QUE LE PANNEAU « PERSONNALISER » MONTRE ──────────────────────────
  //
  // Les décors proposés, celui d'usine en tête.
  std::vector<LayoutBackground> backgrounds;
  // Le décor choisi s'est-il chargé ? ⚠ SINON ON LE DIT : un fichier supprimé,
  // ou un BMP que le lecteur refuse, laisserait sinon le joueur devant la ville
  // sans comprendre.
  bool background_loaded = true;
  // Combien de décors viennent d'une archive : dit au joueur qu'il en a reçu
  // du serveur, sans rien avoir installé.
  int packed_backgrounds = 0;
  // Combien d'images dorment dans un dossier que le client NE LIT PAS, et
  // lequel, déjà écrit pour l'affichage.
  int misplaced_backgrounds = 0;
  std::string misplaced_folder;
  // Les noms des mises en page enregistrées, dans l'ordre du fichier.
  std::vector<std::string> presets;
  // LE NOMBRE D'IMAGES de l'action que montre chaque siège, relevé au rendu :
  // il dépend de la pose ET de la classe, deux convives n'en ont pas forcément
  // autant. Zéro tant que rien n'est dessiné.
  std::array<int, kSeatCount> seat_frames = {};
};

// CE QUE LE JOUEUR A POSÉ SUR LA SCÈNE. Il appartient à l'APPELANT, comme tout
// état dans cette couche.
struct CharacterSelectState {
  // L'emplacement désigné, ou -1. ⚠ C'EST UN NUMÉRO D'EMPLACEMENT, pas un
  // numéro de siège : les deux coïncident sur la première page et divergent
  // ensuite.
  int selected = -1;
  int page = 0;

  // ── LE MODE « PERSONNALISER » ────────────────────────────────────────────
  //
  // 🔴 LA MISE EN PAGE VIVANTE : la scène la lit, et pendant la
  // personnalisation elle l'ÉCRIT — glisser un siège, le tourner, changer de
  // décor. Chargée et enregistrée par `app`.
  SeatLayout layout;
  bool customizing = false;
  // ⚠ LA SCÈNE MARQUE, ELLE N'ÉCRIT PAS : enregistrer à chaque pixel de
  // glissement réécrirait le fichier des dizaines de fois par seconde. `app`
  // grave en sortant du mode, en entrant en jeu et en quittant l'écran.
  bool layout_dirty = false;
  // Le nom saisi pour « Enregistrer », la mise en page désignée dans la liste,
  // et la suppression armée — en deux temps plutôt qu'une boîte de plus.
  std::array<char, kPresetNameBytes> preset_name = {};
  int preset_selected = -1;
  bool preset_delete_armed = false;
  // Le siège dont le menu de pose est ouvert, et la demande d'ouverture,
  // différée hors de la pile d'identifiants du siège.
  int pose_seat = -1;
  bool pose_menu_asked = false;
  // La molette qui tourne un siège passe par le portail commun.
  WheelGateState wheel;
};

// CE QUE LA SCÈNE A DEMANDÉ.
//
// ⚠ ELLE N'ENVOIE RIEN ELLE-MÊME : elle rend un verbe, et c'est `app` qui
// parle au serveur. Même règle que ses voisines.
enum class CharacterSelectVerb : std::uint8_t {
  kNothing,
  // Entrer en jeu avec l'emplacement que porte l'action.
  kEnter,
  // Ouvrir la création à l'emplacement que porte l'action.
  kCreate,
  // ── LA SUPPRESSION, EN DEUX TEMPS ───────────────────────────────────────
  //
  // ⚠ TROIS VERBES POUR DEUX GESTES DU JOUEUR, et le troisième est le plus
  // important : `kAskDeletion` n'envoie RIEN. Il ouvre la confirmation, où
  // l'on tape le secret du compte. Un bouton « Supprimer » qui partirait au
  // premier clic serait irréversible sans qu'on l'ait dit.
  kReserveDeletion,
  kCancelDeletion,
  kAskDeletion,
  // Revenir au formulaire. ⚠ LA LISTE NE SURVIT PAS : elle venait d'une
  // liaison qu'on referme.
  kBack,
  // Fermer le client.
  kQuit,
  // ── LA PERSONNALISATION ─────────────────────────────────────────────────
  //
  // ⚠ L'ÉTAT A DÉJÀ BASCULÉ quand ces deux verbes sortent : `app` n'a plus qu'à
  // relire la galerie en entrant — le joueur vient peut-être d'y déposer une
  // image — et à graver en sortant.
  kBeginCustomizing,
  kEndCustomizing,
  kRescanBackgrounds,
  kOpenBackgroundFolder,
  // ⚠ LES MISES EN PAGE ENREGISTRÉES VIVENT CHEZ `app`, qui les écrit tout de
  // suite : le joueur attend d'« Enregistrer » un effet immédiat sur le
  // disque. Le nom est dans `preset_name`, l'index dans `preset`.
  kSavePreset,
  kApplyPreset,
  kDeletePreset,
};

struct CharacterSelectAction {
  CharacterSelectVerb verb = CharacterSelectVerb::kNothing;
  // L'emplacement visé, ou -1. Renseigné pour `kEnter` et `kCreate`.
  int slot = -1;
  // Le personnage visé, ou zéro. Renseigné pour les trois verbes de
  // suppression, qui le désignent ainsi.
  std::uint32_t character_id = 0;
  // La mise en page enregistrée visée, ou -1.
  int preset = -1;
};

// DESSINE LA SCÈNE.
[[nodiscard]] CharacterSelectAction DrawCharacterSelect(CharacterSelectState& state,
                                                        const CharacterSelectView& view,
                                                        const SkinArt& art);

}  // namespace ro::ui
