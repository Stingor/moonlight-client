#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ro::sim {

// Une COMPÉTENCE APPRISE, telle que le serveur la déclare.
//
// ⚠ Ce n'est pas l'arbre des compétences. Le serveur n'annonce que ce que le
// personnage sait DÉJÀ — une ligne par compétence apprise — et rien de ce
// qu'il pourrait apprendre : l'arbre est une table du client, pas du serveur.
struct KnownSkill {
  std::uint16_t id = 0;
  std::uint16_t level = 0;
  // Ce que lancer coûte en points de magie, au niveau appris.
  std::uint16_t sp_cost = 0;
  // La portée, en cases. Zéro pour ce qui n'en a pas.
  std::uint16_t range_cells = 0;
  // CE QUE LA COMPÉTENCE VISE, en drapeaux (e_skill_inf, dans src/map/skill.hpp
  // du serveur). C'est la seule chose qui dise comment la lancer, et le serveur
  // REFUSE une demande qui se trompe : une compétence de zone envoyée comme
  // visant quelqu'un ne fait rien du tout, et sans un mot.
  std::uint16_t targeting = 0;
  // Le nom INTERNE du serveur — « MG_COLDBOLT ». Ce n'est pas le nom affiché
  // du jeu, qui vit dans les tables Lua du client, et il n'y a rien à traduire
  // ici : le serveur envoie ce que sa base de compétences nomme.
  std::string name;
  // Vrai tant qu'il reste un rang à monter.
  bool upgradable = false;
};

// Les drapeaux de visée, dans l'ordre de e_skill_inf.
//
// Le drapeau 0x08 n'est attribué à rien, et le serveur le dit ainsi.
inline constexpr std::uint16_t kSkillPassive = 0x00;
inline constexpr std::uint16_t kSkillOnEnemy = 0x01;
inline constexpr std::uint16_t kSkillOnGround = 0x02;
inline constexpr std::uint16_t kSkillOnSelf = 0x04;
inline constexpr std::uint16_t kSkillOnFriend = 0x10;
// 🔴 UNE COMPÉTENCE QUI VISE UN PIÈGE DÉJÀ POSÉ — Remove Trap, Spring Trap —, et
// non une compétence qui en POSE un. Ankle Snare ou Land Mine s'annoncent par le
// drapeau du sol : poser un piège, c'est viser une case.
inline constexpr std::uint16_t kSkillTrap = 0x20;

// Une compétence qui se lance SUR UNE CASE : une zone, un piège qu'on pose, un
// mur.
//
// Le serveur écarte celles-là de la demande visant quelqu'un.
[[nodiscard]] bool SkillNeedsGround(const KnownSkill& skill);

// Une compétence qui se lance SUR UNE UNITÉ AU SOL — un piège posé —, désignée
// d'un clic sur elle.
//
// 🔴 ELLE PART PAR LA DEMANDE VISANT QUELQU'UN, l'identifiant de l'unité pour
// cible : le serveur écarte de la demande au sol toute compétence qui n'y porte
// pas le drapeau du sol.
[[nodiscard]] bool SkillTargetsTrap(const KnownSkill& skill);

// Talkie Box : la compétence au sol qui demande un texte avant de partir.
inline constexpr std::uint16_t kTalkieBoxSkill = 125;
// Graffiti : la même demande, le texte peint au sol.
inline constexpr std::uint16_t kGraffitiSkill = 220;

// Une compétence qui se lance SUR SOI, sans que rien ne soit à désigner.
//
// Le serveur remplace la cible d'une telle demande par le demandeur lui-même
// (clif_parse_skill_toid, « never trust the client ») : il n'y a donc rien à
// viser, et rien à se tromper.
[[nodiscard]] bool SkillCastsOnSelf(const KnownSkill& skill);

// Une compétence qui attend qu'on DÉSIGNE quelqu'un : un ennemi, un allié.
[[nodiscard]] bool SkillNeedsTarget(const KnownSkill& skill);

// Une compétence PASSIVE, qui ne se lance pas du tout.
[[nodiscard]] bool SkillIsPassive(const KnownSkill& skill);

// UN LANCER QUE LE SERVEUR A REFUSÉ, tel que son paquet le dit.
//
// 🔴 LE MOTIF NE SUFFIT PAS TOUJOURS : pour un objet manquant, le paquet porte
// AUSSI l'objet et la quantité — un Medicine Bowl pour Prepare Potion. Les
// garder est ce qui permet à la ligne de dire QUOI apporter, et non « il manque
// quelque chose ».
struct SkillRefusal {
  std::uint64_t seq = 0;  // le rang, évictions comprises : il ne recule jamais
  std::uint16_t skill_id = 0;
  std::uint8_t cause = 0;
  // Le nombre que le motif cite — la quantité d'objets requise, le plus souvent.
  std::int32_t count = 0;
  // L'objet que le motif nomme, ou zéro.
  std::uint32_t item_id = 0;
  std::uint32_t refused_ms = 0;  // l'horloge de l'appelant
};

// COMBIEN DE REFUS ON GARDE : ils s'écrivent dans la conversation à chaque
// pompage, si bien que seize couvrent largement une rafale de touches.
inline constexpr std::size_t kSkillRefusalsKept = 16;

// Le GRIMOIRE : ce que le personnage sait lancer.
//
// Il ne vit pas sur la carte. Le serveur le redit à chaque entrée, mais ce
// qu'on sait ne s'oublie pas en changeant de carte : le vider à chaque saut
// laisserait un grimoire blanc entre l'annonce de la carte et celle des
// compétences.
class SkillBook {
 public:
  // Apprend une compétence, ou remplace ce que l'on savait d'elle.
  //
  // ⚠ Un nom VIDE garde celui que l'on avait. La mise à jour d'une compétence
  // arrive par un paquet qui ne porte PAS le nom, et l'écraser laisserait une
  // ligne sans titre dès la première montée de rang.
  void Learn(KnownSkill skill);

  // Oublie une compétence. Faux si le grimoire ne la portait pas.
  bool Forget(std::uint16_t skill_id);

  void Clear();

  // Pose le TEMPS D'ATTENTE d'une compétence : le serveur l'annonce après un
  // lancement réussi, et tant qu'il court le serveur refusera de la relancer.
  //
  // Il se note même pour une compétence que le grimoire ne porte pas : le
  // serveur en annonce pour des compétences qu'un objet donne sans les
  // apprendre.
  //
  // ⚠ LE CLIENT NATIF, LUI, LE PERD. Il ne l'enregistre que si l'icône de la
  // compétence est DESSINÉE à cet instant dans l'une de ses deux barres de
  // raccourcis, listes vidées et refaites à chaque image : barre cachée, aucun
  // temps d'attente n'est jamais retenu. Suivre le paquet lui-même est donc la
  // seule lecture qui ne perde rien.
  void StartCooldown(std::uint16_t skill_id, std::uint32_t duration_ms, std::uint32_t now_ms);

  // Ce qu'il reste à attendre, en millisecondes, ou zéro pour une compétence
  // prête. L'horloge est celle de l'APPELANT, comme partout ailleurs : le
  // grimoire n'en lit aucune.
  [[nodiscard]] std::uint32_t CooldownRemainingMs(std::uint16_t skill_id,
                                                  std::uint32_t now_ms) const;

  // LA PART DU TEMPS D'ATTENTE QUI RESTE, de un à zéro.
  //
  // ⚠ ELLE DEMANDE LA DURÉE, PAS SEULEMENT L'ÉCHÉANCE, et c'est pourquoi le
  // livre garde les deux. Le voile que le jeu peint sur une icône MONTE à mesure
  // que le temps passe : sans savoir d'où l'on part, il ne peut que disparaître
  // d'un coup à la fin — ce qui ne dit rien de l'attente.
  [[nodiscard]] float CooldownFraction(std::uint16_t skill_id, std::uint32_t now_ms) const;

  [[nodiscard]] const KnownSkill* Find(std::uint16_t skill_id) const;

  // Rangées par IDENTIFIANT. Le serveur les envoie dans l'ordre de sa propre
  // table, qui n'est pas celui de son fichier ; un ordre stable est ce qui
  // permet de cliquer deux fois au même endroit sur la même compétence.
  [[nodiscard]] std::span<const KnownSkill> skills() const noexcept { return skills_; }

  [[nodiscard]] std::size_t size() const noexcept { return skills_.size(); }

  // 🔴 UNE COMPÉTENCE QU'UN OBJET PRÊTE LE TEMPS D'UN LANCER — une Feuille
  // d'Yggdrasil, un parchemin. Le serveur la décrit comme une compétence du
  // grimoire, sans l'y ranger : le client passe aussitôt en visée avec elle, à
  // son niveau, et le lancer qui suit la nomme. Sans cette visée, l'objet se
  // consume et rien ne se passe.
  void OfferItemSkill(KnownSkill skill) { offered_ = std::move(skill); }

  // La compétence offerte et pas encore reprise, qui cesse de l'être.
  [[nodiscard]] std::optional<KnownSkill> TakeOfferedSkill() { return std::exchange(offered_, {}); }

  // LES REFUS DU SERVEUR, les plus récents, relevés par leur rang : c'est
  // `app` qui les écrit dans la conversation, parce que la phrase veut le nom
  // affiché de la compétence et celui de l'objet. `seq` est ignoré et
  // attribué ici.
  //
  // ⚠ `Clear` NE LES TOUCHE PAS : le rang ne doit jamais reculer, sans quoi un
  // témoin réécrirait des refus déjà dits.
  void RecordRefusal(SkillRefusal refusal);
  [[nodiscard]] std::span<const SkillRefusal> refusals() const noexcept { return refusals_; }
  [[nodiscard]] std::uint64_t last_refusal_seq() const noexcept { return refusal_seq_; }

 private:
  std::vector<KnownSkill> skills_;
  std::optional<KnownSkill> offered_;
  // Les échéances, par compétence. Elles vivent À PART de la liste : celle-ci
  // est REMPLACÉE entière à chaque entrée sur une carte, alors que le serveur,
  // lui, garde les temps d'attente en cours. Les ranger dans la ligne les
  // effacerait à chaque saut de carte.
  // Par compétence : quand l'attente a commencé, et quand elle finit. Rangées
  // par identifiant, pour une recherche dichotomique.
  struct Cooldown {
    std::uint16_t skill_id = 0;
    std::uint32_t started_ms = 0;
    std::uint32_t until_ms = 0;
  };
  std::vector<Cooldown> cooldowns_;
  std::vector<SkillRefusal> refusals_;
  std::uint64_t refusal_seq_ = 0;
};

}  // namespace ro::sim
