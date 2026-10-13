#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/render/actor_pick.h"
#include "ro/render/camera.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/actor.h"
#include "ro/sim/pending_action.h"
#include "ro/sim/pickup_intent.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"

#include "character/skill_book_view.h"
#include "world/click_rules.h"
#include "world/deferred_walk.h"
#include "world/entity_menu.h"
#include "world/follow_rules.h"
#include "world/map_stage.h"
#include "world/mouse_engagement.h"
#include "world/overlay_view.h"
#include "world/targeting.h"

namespace ro::sim {
class GuildBook;
class PartyRoster;
}  // namespace ro::sim

namespace moonclient {

struct GroundUnitPick;

// De combien de temps une marche TENUE se redemande, en millisecondes.
//
// Le jeu marche tant que le bouton gauche est tenu : la demande se refait
// pendant qu'on tient, vers la case que la souris désigne ALORS.
//
// 🔴 CENT EST LE CHIFFRE DU CLIENT JOUÉ, ET SIX CENTS CELUI DU VANILLA. Le
// client d'origine attend SIX CENTS millisecondes après son dernier envoi avant
// de reprendre une marche au bouton maintenu. Moonlight-Destiny.exe est PATCHÉ
// par WARP et attend cent.
//
// ⚠ ET C'EST UN PIÈGE : une constante du client vanilla n'est pas forcément
// celle qui tourne chez le joueur, et le désaccord ressemble alors à un
// commentaire faux — au point qu'on le « corrige ». C'est le client patché qui
// fait foi.
//
// ⚠ CE N'EST PAS UN DÉBIT RÉSEAU, c'est ce qui sépare un CLIC d'un MAINTIEN.
// La caméra SUIT le personnage : une souris qui ne bouge pas désigne, image
// après image, une case qui s'ÉLOIGNE avec lui. Redemander vingt fois par
// seconde ferait donc courir le personnage après une destination qui fuit, et
// un clic bref le mènerait DEUX cases plus loin que la case cliquée. Sous le
// délai, un clic ne demande qu'une fois — la case qu'on a désignée, et pas une
// autre.
inline constexpr std::uint32_t kWalkRepeatMs = 100;

// De combien de temps un demi-tour TENU se redemande, en millisecondes.
//
// 🔴 C'EST CE QUI ÉVITE DE RECLIQUER. Un demi-tour n'avance que d'un huitième
// à la fois (voir ro::sim::TurnToward) : sans répétition, viser à
// quatre-vingt-dix degrés demanderait deux clics et un demi-tour complet
// resterait hors d'atteinte du geste. Tenir le bouton achève le tour.
//
// ⚠ ET LE NATIF SE RÈGLE SUR L'HORLOGE, PAS SUR LA CASE. Il rejoue son clic au
// sol tant que le bouton est tenu, et REPREND alors la case que la souris
// désigne — d'où l'impression qu'il réagit au changement de case. Le
// déclencheur est le temps ; la case n'est que ce qu'il lit en passant. Une
// répétition attachée au changement de case, elle, n'achèverait jamais un tour
// visant une case immobile.
//
// C'est la MÊME cadence que la marche, et le même patch WARP la règle : les
// deux relèvent de la même répétition du clic au sol. Voir kWalkRepeatMs pour
// le vanilla à six cents et pour le piège qu'il tend.
inline constexpr std::uint32_t kTurnRepeatMs = 100;

// La case du sol sous un point de l'écran, s'il y en a une.
//
// C'est le même calcul pour le clic et pour sa répétition : le refaire à deux
// endroits inviterait les deux à diverger.
[[nodiscard]] std::optional<ro::sim::CellPosition> GroundCellAt(
    const MapStage& stage, const ro::render::Camera& camera, float aspect_ratio, float screen_x,
    float screen_y, float viewport_width, float viewport_height);

// Écrit la raison d'une demande qui n'a pas pu partir. Il n'y en a qu'une :
// une disposition de paquet qui ne s'accorde plus avec le serveur.
void ReportRefusal(const ro::core::Result<void>& sent);

// LA GARDE DE POIDS, jouée : au-delà de 90 % du poids maximum, la conversation
// dit le refus, et l'ordre d'action sur une entité — attaque, compétence
// visée, suivi — est abandonné. `skill_id` : la compétence armée, zéro pour les
// autres. Rend vrai si l'ordre part. Voir `WeightGuardOf`.
[[nodiscard]] bool PassesWeightGuard(ro::sim::World& world, std::uint16_t skill_id,
                                     std::uint32_t now_ms);

// UNE COMPÉTENCE VISÉE SUR UNE ENTITÉ PART-ELLE ? La garde de poids, puis la
// posture du personnage (`TakesSkillOrder`). Refusée, rien ne part : à
// l'appelant de désarmer la visée, comme après un lancer.
[[nodiscard]] bool SkillOnActorPasses(ro::sim::World& world, std::uint16_t skill_id,
                                      std::uint32_t now_ms);

// Vrai si un acteur PREND le clic ; faux s'il le laisse descendre au SOL.
//
// ⚠ UN ACTEUR SOUS LA SOURIS N'ABSORBE PAS TOUJOURS LE CLIC, et c'est le point
// que le client rend le moins évident. Le survol rend un code que le clic au
// sol consulte pour décider s'il marche encore ; ZÉRO veut dire « je n'ai rien
// fait », et la marche a lieu.
//
// Le survol rend zéro dans trois cas, et le PREMIER examiné est le plus
// important : le GID survolé comparé à celui du compte. On marche donc À
// TRAVERS son propre personnage. Elle rend zéro aussi pour un autre JOUEUR non
// hostile, pour un familier et pour une unité de compétence.
//
// Prennent le clic : un monstre VIVANT, un PNJ, un objet au sol, et un autre
// JOUEUR quand la carte ou le geste ouvre le ciblage. Un cadavre ne le prend
// pas — on marche dessus.
//
// La règle se lit d'un coup : un acteur prend le clic exactement quand le
// CURSEUR CHANGE DE FORME pour lui. Ce n'est pas une coïncidence qu'on
// entretiendrait — c'est ainsi que la fonction est écrite, à partir de la
// forme, pour que les deux ne puissent pas diverger. C'est aussi ce qui fait
// qu'ouvrir le ciblage d'un joueur suffit à lui faire prendre le clic : il n'y
// a rien d'autre à penser à changer.
[[nodiscard]] bool ActorTakesClick(const ro::sim::Actor& actor, VersusGate versus);

// Un clic sur cet acteur engage-t-il une attaque ? Un monstre ou un joueur
// vivant, et rien d'autre.
[[nodiscard]] bool ClickEngagesAttack(const ro::sim::Actor& actor);

// Demande de marcher vers `destination`, comme un clic au sol : voir
// `GroundWalkDue`.
void RequestGroundWalk(ro::net::Connection& link, const ro::sim::World& world,
                       ro::sim::ApproachWalkClock& clock, DeferredWalk* deferred,
                       std::optional<PlayerGesture> player_gesture,
                       ro::sim::CellPosition destination, std::uint32_t now_ms);

// Libère la marche retenue quand le geste qui la retenait a changé : voir
// `ReleasedWalk`. Rend vrai si une marche est partie.
bool ReleaseDeferredWalk(ro::net::Connection& link, const ro::sim::World& world,
                         ro::sim::ApproachWalkClock& clock, DeferredWalk& deferred,
                         std::optional<PlayerGesture> player_gesture, std::uint32_t now_ms);

// CE QU'UN CLIC LAISSE DERRIÈRE LUI, et que le tic reprendra.
//
// Deux intentions du CLIENT, dont le serveur ne sait rien, et qu'un clic
// remplace TOUTES LES DEUX : on ne ramasse plus ce qu'on a cessé de viser, et
// on ne court plus après une cible qu'on a cessé de désigner. Les tenir
// ensemble est ce qui rend cette règle-là impossible à oublier — le client
// natif écrase de même son action en attente, il ne l'empile pas.
struct ClickIntents {
  std::optional<ro::sim::PickupIntent>& pickup;
  std::optional<ro::sim::PendingAction>& attack;
  // L'HORLOGE DES MARCHES DU PERSONNAGE, que le clic partage avec les
  // poursuites : voir `ro::sim::AdmitWalkRequest`.
  ro::sim::ApproachWalkClock& walk_clock;
  // La marche que le geste retient, et ce que la scène montre du personnage :
  // voir `RequestGroundWalk`. La même marche retenue pour tous les clics, celui
  // du monde, du menu d'entité et du mode Ciblage ; nulle hors partie.
  DeferredWalk* deferred_walk = nullptr;
  std::optional<PlayerGesture> player_gesture;
};

// Ce qu'un clic sur un acteur demande au serveur, et l'intention qu'il laisse.
//
// 🔴 UN MONSTRE DEBOUT NE DEMANDE RIEN ENCORE : le coup s'INSCRIT, et c'est le
// tic qui, selon la distance, marche vers lui ou frappe. Le serveur ne
// rapproche PAS un joueur d'une cible trop loin — voir `world/attack_intent.h`
// pour ce que ce contresens a coûté.
//
// Un objet se ramasse s'il est à portée ; sinon on marche jusqu'à lui, et
// l'intention attend l'arrivée.
//
// Un PORTAIL ne se demande pas : on MARCHE jusqu'à sa case, et c'est le serveur
// qui téléporte à l'arrivée. Il n'y a donc aucun paquet de passage — un client
// qui en chercherait un ne trouverait rien.
//
// 🔴 UN AUTRE JOUEUR SE FRAPPE EXACTEMENT COMME UN MONSTRE, et par le MÊME
// chemin : la même inscription, la même approche, le même paquet d'action. Le
// serveur ne fait pas non plus de différence — `clif_parse_ActionRequest` ne
// regarde pas l'espèce de la cible, c'est `battle_check_target` qui tranche
// ensuite. Lui écrire un second chemin ferait diverger deux copies de la même
// poursuite.
//
// ⚠ ET ON N'ARRIVE ICI QUE SI LA PORTE EST OUVERTE : `ActorTakesClick` a déjà
// refusé le clic sur un joueur hors zone versus, qui est donc descendu au sol.
//
// Un PNJ ne demande rien de plus : le dialogue attend sa fenêtre.
// ⚠ `repeat` EST CE QUE LE GESTE A DÉCIDÉ, et il voyage jusqu'au coup : celui-ci
// ne part qu'une fois la cible atteinte, parfois plusieurs secondes plus tard.
[[nodiscard]] ro::core::Result<void> ClickOn(ro::net::Connection& link, const ro::sim::World& world,
                                             const ro::sim::Actor& target, std::uint32_t now_ms,
                                             const ClickIntents& intents, bool repeat);

// CE QUE `ClickOn` LAISSE AU PERSONNAGE, pour l'engagement de la souris : une
// attaque, un ramassage, ou — pour un portail — une marche, qui fait retomber.
// Parler à un PNJ ne change rien, et une attaque que le personnage ne prend pas
// (voir `TakesAttackOrder`) non plus. Nul : rien à noter.
void NoteDeedOfClickOn(MouseEngagement* engagement, const ro::sim::World& world,
                       const ro::sim::Actor& target, bool repeat);

// LES LIENS DU PERSONNAGE QUE LA VISÉE CONSULTE : son groupe et sa guilde.
// Nuls hors partie : personne n'est alors membre, ni allié.
struct SocialTies {
  const ro::sim::PartyRoster* party = nullptr;
  const ro::sim::GuildBook* guild = nullptr;
};

// Les touches d'une visée, relevées à l'appui pour le clic, à l'image pour le
// curseur.
struct AimKeys {
  bool shift_held = false;
  bool no_shift = false;
  bool alt_held = false;
};

// LA COMPÉTENCE VISÉE ATTEINT-ELLE CE SPRITE ? Un joueur ou un monstre suit
// `AimedSkillReachesPlayer` ou `AimedSkillReachesMonster` (`world/name_click.h`) ;
// tout autre acteur, oui. `for_cursor` épargne en outre un monstre de ma
// guilde, ce que le clic ne fait pas.
[[nodiscard]] bool AimedSkillReachesActor(const ro::sim::World& world, const ro::sim::Actor& target,
                                          const ro::sim::KnownSkill& skill, SocialTies ties,
                                          AimKeys keys, bool siege_map, bool for_cursor);

// Ce qu'un clic gauche a sous la main.
//
// Passé par pointeurs plutôt que gardé : le routage ne POSSÈDE rien. C'est
// aussi ce qui laisse la carte changer sous lui sans qu'il ait à s'en
// apercevoir.
struct WorldClick {
  ro::net::Connection* link = nullptr;
  // Ouvert en ÉCRITURE : le grimoire dit tout haut ce qu'il fait, et la
  // conversation est le seul endroit du jeu où l'on puisse le lire.
  ro::sim::World* world = nullptr;
  const MapStage* stage = nullptr;
  const ro::render::Camera* camera = nullptr;
  SkillBookPanel* skills = nullptr;
  // Les deux moitiés de ce que la scène vient de peindre : le quoi et le qui.
  //
  // C'est l'image PRÉCÉDENTE. Une image de retard sur qui traverse une case en
  // cent cinquante millisecondes ne se voit pas, et attendre la suivante
  // retarderait chaque coup d'autant.
  std::span<const ro::render::DrawnActor> drawn;
  std::span<const std::uint32_t> drawn_ids;
  float aspect_ratio = 1.0F;
  // La taille de la FENÊTRE, en pixels de fenêtre — celle où la souris vit.
  // Sur un écran à facteur d'échelle, elle diffère du tampon de rendu, et s'en
  // remettre à l'un pour l'autre fait viser à côté d'autant.
  float viewport_width = 0.0F;
  float viewport_height = 0.0F;
  // Ce que le chemin de la marche a planifié pour chaque acteur : il dit vers
  // où le personnage regarde AUJOURD'HUI, ce que sa case n'annonce plus une
  // fois qu'il a marché.
  ro::sim::WalkPlanner* planner = nullptr;
  // MAJ tenue à l'instant du clic. Sur le sol, elle fait TOURNER au lieu de
  // marcher ; CTRL, lui, n'y fait rien du tout (`ctrl_held`).
  //
  // ⚠ ET LA MÊME TOUCHE FAIT TROIS CHOSES SANS RAPPORT, selon ce qu'elle
  // désigne : sur un JOUEUR elle force la cible, sur un MONSTRE elle retient
  // le clic (`MonsterClickWithheld`), sur le SOL elle fait tourner.
  // Le client d'origine les tient dans deux fonctions distinctes, et
  // `/noshift` ne touche qu'à la première — voir `VersusGate::forced`.
  bool shift_held = false;
  // CE QUI OUVRE LE CIBLAGE D'UN AUTRE JOUEUR, relevé au moment du clic.
  //
  // ⚠ RELEVÉ, ET NON RECALCULÉ PLUS BAS : la carte se lit dans le monde, mais
  // « le geste force » réunit la touche et un réglage, et cette réunion doit
  // être la MÊME que celle dont le curseur s'est servi pour choisir sa forme.
  // Deux calculs séparés finiraient par se contredire d'une image, et le clic
  // ferait alors autre chose que ce que le curseur promettait.
  VersusGate versus;
  // SANS RELÂCHE, ou un seul coup : CTRL tenu, ou `/noctrl` allumé, hors siège.
  // Voir `Targeting::WantsContinuousAttack`, et `PendingAction::repeat` pour la
  // raison de le figer au clic plutôt que de relire la touche quand le coup part.
  //
  // ⚠ VRAI PAR DÉFAUT, à l'inverse de la porte : le réglage du client d'origine
  // est ALLUMÉ d'usine, et un clic qui ne porterait qu'un coup surprendrait.
  bool attack_repeats = true;
  std::uint32_t now_ms = 0;
  // Le mode Ciblage : le clic désigne ce qu'il touche, et peut ne rien engager.
  // Nul hors partie.
  Targeting* targeting = nullptr;
  // LES PNJ QU'ON A RENDUS SOURDS : un clic sur l'un d'eux n'a aucun effet. Nul
  // hors partie.
  EntityMenuGestures* entity_menu = nullptr;
  // L'ENGAGEMENT DE LA SOURIS : un clic d'action le lève, une marche le fait
  // retomber. Nul hors partie.
  MouseEngagement* engagement = nullptr;
  // Ni sous ALT, ni au second appui d'un double-clic, un clic n'engage : il
  // agit, sans lever la flèche.
  bool alt_held = false;
  bool second_of_double = false;
  // Les unités au sol que la souris peut désigner, et le plancher de leur
  // emprise : voir `PickGroundUnit`.
  std::span<const GroundUnitPick> ground_units;
  float ground_unit_floor_pixels = ro::render::kGroundPickFloorFactor;
  // CTRL tenu à l'instant du clic : le sol ne fait ni marcher ni tourner — voir
  // `GroundClickMoves`.
  bool ctrl_held = false;
  // `/noshift` : MAJ ne retient plus le clic sur un monstre — voir
  // `MonsterClickWithheld`.
  bool no_shift = false;
  // Le groupe et la guilde, que la compétence visée consulte sur un joueur.
  SocialTies ties;
};

// Ce qui arrête la souris dans le monde, à cet instant : la mort, le coffre, un
// commerce ouvert, et le dialogue d'un PNJ qui retient le monde — sauf tant que
// le joueur porte une barre : voir `WorldMouseFrozen`.
[[nodiscard]] MouseRoute MouseRouteOf(const ro::sim::World& world, const SkillBookPanel& skills,
                                      const MouseEngagement& engagement, std::uint32_t now_ms);

// Ce que le clic laisse EN COURS tant que le bouton reste enfoncé.
//
// Le jeu ne fait pas que partir sur un clic : il REFAIT sa demande vers la case
// que la souris désigne ALORS, tant qu'on tient. Le clic ouvre donc un
// maintien, et le maintien a deux formes — on marche, ou on tourne.
//
// ⚠ C'EST LE MOMENT DE L'APPUI QUI TRANCHE, et non la touche à l'instant de la
// répétition : relâcher MAJ sans relâcher le bouton continue de tourner. Le
// natif rejuge à chaque fois et se mettrait à marcher ; se mettre à marcher
// parce qu'un doigt a glissé est le genre de surprise qui coûte une vie.
enum class HeldClick : std::uint8_t { kNothing, kWalk, kTurn };

// LE BOUTON GAUCHE DANS LE MONDE, d'une image à l'autre : enfoncé ou non, ce
// que le maintien refait et depuis quand, et ce que le dernier clic d'action a
// engagé.
struct WorldMouse {
  bool pressed = false;
  HeldClick held_click = HeldClick::kNothing;
  std::uint32_t held_since_ms = 0;
  MouseEngagement engagement;
  DeferredWalk deferred_walk;
  // LE BOUTON DROIT A ÉTÉ ENFONCÉ SUR UNE ENSEIGNE, et n'est pas encore
  // relâché : elle a pris l'appui comme une fenêtre, et le glissé qui suit ne
  // tourne pas la caméra.
  bool right_on_signboard = false;
  // OÙ LE BOUTON DROIT S'EST ENFONCÉ dans le monde, jusqu'à son relâchement :
  // un relâchement loin de là clôt un glissé de caméra, pas un clic.
  std::optional<glm::vec2> right_pressed_at;
  // L'appui droit enfoncé dans le monde était le second d'un double-clic :
  // son relâchement ne donne pas d'ordre aux compagnons.
  bool right_press_doubled = false;
  // Le double-clic des boutons gauche et droit : voir `DoubleClickClocks`.
  DoubleClickClocks double_click;
  // Le joueur que le personnage suit : voir `world/follow_rules.h`.
  FollowState follow;
  // Le groupe et la guilde du personnage, posés à l'ouverture de la partie.
  SocialTies ties;
  // LA COMPÉTENCE QU'UN APPUI SUR UNE FENÊTRE VIENT DE DÉSARMER : la ligne
  // d'un nom, que l'interface lit après l'appui, la vise encore. Vide sans
  // compétence armée, ou après tout autre appui.
  std::optional<ro::sim::KnownSkill> armed_at_press;
};

// Ce qu'un clic gauche déclenche, dans l'ordre où le jeu tranche.
//
// LE GRIMOIRE D'ABORD — le panneau est devant la scène, et un clic dessus ne
// doit jamais atteindre le sol qu'il masque. Puis la compétence ARMÉE, qui
// attend qu'on lui désigne une cible. Puis un ACTEUR, qu'on frappe, ramasse ou
// traverse. Le SOL en dernier, et lui seul fait marcher.
[[nodiscard]] HeldClick LeftClick(const WorldClick& scene, glm::vec2 mouse_px,
                                  const ClickIntents& intents);

// MAJ + CLIC AU SOL : tourner vers cette case, au lieu d'y marcher.
//
// Le client natif le décide au même endroit et de la même façon — l'état de
// MAJ est lu juste avant d'émettre la demande de marche : la touche ne change
// ni la case désignée ni l'ordre des cibles, elle échange seulement le message
// 17, « marche », contre le 16, « tourne ».
//
// ⚠ IL TOURNE AUSSI SANS LA TOUCHE quand marcher est impossible — assis dans
// une salle de discussion, par exemple. Ces états-là n'existent pas encore
// ici ; le jour où ils existeront, c'est cette fonction qu'ils appelleront.
//
// ⚠ ET UN DEMI-TOUR NE SE DEMANDE PAS EN MARCHANT. Le natif refuse dans la
// plupart de ses états, et la marche en est un : le serveur reposerait la
// direction au pas suivant, après l'avoir diffusée à tous les voisins.
//
// Rend faux s'il n'y a rien à demander — pas de joueur, sa propre case, ou une
// marche en cours.
bool TurnTowardCell(const WorldClick& scene, ro::sim::CellPosition toward);

// Où va un cran de molette.
//
// LE GRIMOIRE D'ABORD, comme pour le clic : la molette sur un panneau le fait
// défiler, et une compétence ARMÉE la prend partout pour régler son rang. Rend
// vrai quand le grimoire a pris le cran : la caméra n'en fait alors rien.
[[nodiscard]] bool MouseWheel(SkillBookPanel& skills, const ro::sim::World& world,
                              const ro::sim::SkillTree* skill_tree, glm::vec2 mouse_px,
                              float wheel_steps, float viewport_width, float viewport_height,
                              std::uint32_t now_ms);

}  // namespace moonclient
