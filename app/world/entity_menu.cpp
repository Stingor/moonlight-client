#include "world/entity_menu.h"

#include <optional>
#include <string>
#include <utility>

#include "ro/core/i18n.h"

#include "chat/chat_navi_search_link.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::EntityMenuItem;
using ro::ui::EntityMenuQuestion;

// LA COMPOSITION, avec ses deux règles de forme.
//
// ⚠ UN TRAIT OUVRE UN BLOC, et il appartient au bloc, pas à sa première ligne :
// `Group` l'arme, la ligne suivante le prend. C'est ce qui garde les blocs
// séparés quand leur tête manque — une fiche de monstre absente ne doit pas
// coller « Copier le nom » à « Cibler ».
//
// ⚠ LES LIGNES DU STAFF SE SUIVENT EN FIN DE LISTE : le dessin ouvre le
// sous-menu à la première et le referme à la dernière.
class Composer {
 public:
  explicit Composer(ro::ui::EntityMenuView& view) : view_(&view) {}

  void Group() { separator_ = true; }

  EntityMenuItem& Add(const char* label, EntityVerb verb, const char* tip = nullptr) {
    EntityMenuItem item;
    item.label = label;
    item.verb = static_cast<int>(verb);
    item.separator = std::exchange(separator_, false);
    item.submenu = staff_ || named_submenu_;
    item.submenu_label = submenu_label_;
    item.staff = staff_;
    if (tip != nullptr) {
      item.tip = tip;
    }
    view_->items.push_back(std::move(item));
    return view_->items.back();
  }

  // Grise la ligne, avec sa raison.
  static void Disable(EntityMenuItem& item, const char* why) {
    item.disabled = true;
    item.tip = why;
  }

  // Tout ce qui suit va dans le sous-menu. Le trait du sous-menu lui-même
  // appartient au dessin : celui qu'on arme ici séparerait DANS le sous-menu.
  void EnterStaff() {
    LeaveSubmenu();
    staff_ = true;
    separator_ = false;
  }

  // Ce qui suit, jusqu'à `LeaveSubmenu`, va dans le sous-menu `label` (déjà
  // traduit).
  void EnterSubmenu(std::string label) {
    named_submenu_ = true;
    submenu_label_ = std::move(label);
    separator_ = false;
  }
  void LeaveSubmenu() {
    named_submenu_ = false;
    submenu_label_.clear();
    separator_ = false;
  }

 private:
  ro::ui::EntityMenuView* view_;
  bool separator_ = false;
  bool staff_ = false;
  bool named_submenu_ = false;
  std::string submenu_label_;
};

// « Cibler » : seulement sous le mode Ciblage — éteint, l'entrée ne ferait rien
// et le HUD ne montrerait rien.
void AddTarget(Composer& menu, const EntityMenuFacts& facts) {
  if (facts.targeting) {
    menu.Add(Tr("Cibler"), EntityVerb::kTarget);
  }
}

// LE GROUPE : l'invitation, grisée avec sa raison, puis les deux gestes du chef
// sur un membre. Hors groupe, rien : on n'invite que dans un groupe qu'on a.
void AddParty(Composer& menu, const EntityMenuFacts& facts) {
  if (!facts.in_party) {
    return;
  }
  EntityMenuItem& invite = menu.Add(Tr("Inviter dans le groupe"), EntityVerb::kPartyInvite);
  // Trois refus, trois phrases : « il est déjà avec moi » n'appelle pas la même
  // réaction que « il faudra qu'il quitte son groupe ».
  if (facts.target_in_party) {
    Composer::Disable(invite, Tr("Déjà membre de votre groupe."));
  } else if (facts.target_other_party) {
    Composer::Disable(invite, Tr("Ce joueur appartient déjà à un groupe : le serveur refuse "
                                 "l'invitation tant qu'il ne l'a pas quitté."));
  } else if (!facts.party_leader) {
    Composer::Disable(invite, Tr("Seul le chef du groupe peut inviter."));
  }
  if (facts.target_in_party && facts.party_leader) {
    menu.Add(Tr("Nommer chef de groupe"), EntityVerb::kPartyLeader).question =
        EntityMenuQuestion::kPartyLeader;
    menu.Add(Tr("Expulser du groupe"), EntityVerb::kPartyExpel).question =
        EntityMenuQuestion::kPartyExpel;
  }
}

// « AMI DU COMPAGNON » : une case par compagnon présent. Un ami est reconnu à
// son nom : sans nom connu, la case se grise.
void AddCompanionFriends(Composer& menu, const EntityMenuFacts& facts) {
  const bool both = facts.companion_friends.size() > 1;
  bool first = true;
  for (const CompanionFriendFacts& companion : facts.companion_friends) {
    if (first) {
      menu.Group();
      first = false;
    }
    const bool homunculus = companion.companion == ro::sim::AiCompanion::kHomunculus;
    const char* label = both ? (homunculus ? Tr("Ami de l'homoncule") : Tr("Ami du mercenaire"))
                             : Tr("Ami du compagnon");
    EntityMenuItem& item = menu.Add(
        label, homunculus ? EntityVerb::kHomunculusFriend : EntityVerb::kMercenaryFriend,
        Tr("Coché, le compagnon défend ce joueur comme son maître, et ses règles peuvent le "
           "viser comme « un ami »."));
    item.toggle = true;
    item.checked = companion.befriended;
    if (facts.name.empty()) {
      Composer::Disable(item, Tr("Le nom de ce joueur n'est pas encore connu."));
    } else if (companion.full && !companion.befriended) {
      Composer::Disable(item, Tr("La liste d'amis du compagnon est pleine."));
    }
  }
}

// LA CONDUITE D'UN COMPAGNON envers ce joueur : les rangs, puis les
// déclencheurs, en cases ; puis la retirer, et l'onglet « Joueurs » de la
// fenêtre de l'IA. Reconnue au nom : sans nom connu, tout se grise.
void AddCompanionConduct(Composer& menu, const CompanionConductFacts& conduct, bool both,
                         bool named) {
  const bool homunculus = conduct.companion == ro::sim::AiCompanion::kHomunculus;
  menu.EnterSubmenu(
      both ? (homunculus ? Tr("Conduite de l'homoncule") : Tr("Conduite du mercenaire"))
           : Tr("Conduite du compagnon"));
  const char* unnamed = Tr("Le nom de ce joueur n'est pas encore connu.");
  for (std::size_t rank = 0; rank < ro::sim::kTacticRankCount; ++rank) {
    EntityMenuItem& item =
        menu.Add(TacticRankLabel(static_cast<ro::sim::TacticRank>(rank)),
                 static_cast<EntityVerb>(ConductVerb(
                     conduct.companion,
                     static_cast<int>(TacticMenuChoice::kRankFirst) + static_cast<int>(rank))));
    item.toggle = true;
    item.checked = conduct.line.has_value() && static_cast<std::size_t>(conduct.line->rank) == rank;
    if (!named) {
      Composer::Disable(item, unnamed);
    }
  }
  menu.Group();
  for (std::size_t trigger = 0; trigger < ro::sim::kTacticTriggerCount; ++trigger) {
    EntityMenuItem& item =
        menu.Add(TacticTriggerLabel(static_cast<ro::sim::TacticTrigger>(trigger)),
                 static_cast<EntityVerb>(ConductVerb(
                     conduct.companion, static_cast<int>(TacticMenuChoice::kTriggerFirst) +
                                            static_cast<int>(trigger))));
    item.toggle = true;
    item.checked =
        conduct.line.has_value() && static_cast<std::size_t>(conduct.line->trigger) == trigger;
    if (!named) {
      Composer::Disable(item, unnamed);
    }
  }
  menu.Group();
  EntityMenuItem& remove =
      menu.Add(Tr("Retirer la conduite"),
               static_cast<EntityVerb>(
                   ConductVerb(conduct.companion, static_cast<int>(TacticMenuChoice::kRemove))));
  if (!conduct.line.has_value()) {
    Composer::Disable(remove, Tr("Aucune conduite n'est posée pour ce joueur."));
  }
  menu.Add(Tr("Avancé…"),
           static_cast<EntityVerb>(
               ConductVerb(conduct.companion, static_cast<int>(TacticMenuChoice::kAdvanced))),
           Tr("Ouvre l'onglet « Joueurs » de la fenêtre « IA du compagnon ». Une conduite ne "
              "sert qu'en PvP et en WoE."));
  menu.LeaveSubmenu();
}

void AddPlayer(Composer& menu, const EntityMenuFacts& facts) {
  AddTarget(menu, facts);
  menu.Add(Tr("Voir l'équipement"), EntityVerb::kViewEquip);
  AddParty(menu, facts);
  const bool in_guild = facts.own_guild_id != 0;
  if (in_guild) {
    EntityMenuItem& invite = menu.Add(Tr("Inviter dans la guilde"), EntityVerb::kGuildInvite);
    // Le serveur refuse l'invitation d'un rang qui n'en a pas le droit, et un
    // invité qui a déjà une guilde.
    if (facts.guild_invite_forbidden) {
      Composer::Disable(invite, Tr("Votre rang ne peut pas inviter dans la guilde."));
    } else if (facts.target_guild_id != 0) {
      Composer::Disable(invite, Tr("Ce joueur a déjà une guilde."));
    }
  }
  if (in_guild && facts.guild_master) {
    // Les deux visent la GUILDE de la cible : sans guilde, il n'y a rien à
    // allier ni à déclarer ennemi, et sur la nôtre ça n'a aucun sens.
    const char* issue = facts.target_guild_id == 0 ? Tr("Ce joueur n'a pas de guilde.")
                        : facts.target_guild_id == facts.own_guild_id
                            ? Tr("C'est votre propre guilde.")
                            : nullptr;
    menu.Group();
    EntityMenuItem& ally = menu.Add(Tr("Proposer une alliance"), EntityVerb::kGuildAlliance);
    if (issue != nullptr) {
      Composer::Disable(ally, issue);
    }
    EntityMenuItem& foe = menu.Add(Tr("Déclarer la guilde ennemie"), EntityVerb::kGuildOpposition);
    if (issue != nullptr) {
      Composer::Disable(foe, issue);
    }
  }
  // Chuchoter PRÉPARE l'envoi : le nom va dans la case du destinataire, et on
  // écrit.
  menu.Group();
  menu.Add(Tr("Chuchoter"), EntityVerb::kWhisper);
  // LE CARNET DE CHASSE MVP : la même entrée, le même grisage et les mêmes
  // infobulles que dans les deux autres menus d'un joueur. Éteint, il n'en a
  // aucune.
  if (!facts.mvp_invite_withdrawn) {
    EntityMenuItem& hunt =
        menu.Add(Tr("Inviter dans mon carnet MVP"), EntityVerb::kMvpInvite,
                 Tr("Guilde et liste d'amis uniquement.\nInviter quelqu'un, c'est inviter tous "
                    "ses personnages et tous ses comptes de jeu."));
    if (!facts.mvp_invite_refusal.empty()) {
      Composer::Disable(hunt, facts.mvp_invite_refusal.c_str());
    }
  }
  menu.Add(Tr("Chuchoter dans une fenêtre…"), EntityVerb::kWhisperWindow);
  // PROPOSER UN ÉCHANGE : trop loin, le personnage s'approche d'abord.
  EntityMenuItem& deal = menu.Add(Tr("Proposer un échange"), EntityVerb::kDealPropose);
  if (facts.dealing) {
    Composer::Disable(deal, Tr("Un échange est déjà en cours."));
  }
  // LE SUIVI : une ligne OU l'autre, selon qu'on le suit déjà.
  if (facts.followed) {
    menu.Add(Tr("Ne plus le suivre"), EntityVerb::kUnfollow);
  } else {
    menu.Add(Tr("Suivre"), EntityVerb::kFollow,
             Tr("Votre personnage marche derrière lui. Un clic gauche arrête le suivi."));
  }
  // Une ligne OU l'autre : « ajouter » grisé à côté de « retirer » n'apprendrait
  // rien de plus.
  if (facts.target_is_friend) {
    menu.Add(Tr("Retirer de mes amis"), EntityVerb::kFriendRemove).question =
        EntityMenuQuestion::kFriendRemove;
  } else {
    EntityMenuItem& befriend = menu.Add(Tr("Ajouter en ami"), EntityVerb::kFriendAdd);
    if (facts.friends_full) {
      Composer::Disable(befriend, Tr("Votre liste d'amis est pleine."));
    }
  }
  // LE COURRIER S'ADRESSE PAR LE NOM : sans nom connu, ou quand l'écriture est
  // refusée d'avance, la ligne se grise.
  EntityMenuItem& mail = menu.Add(Tr("Envoyer un courrier"), EntityVerb::kMailSend);
  if (facts.name.empty()) {
    Composer::Disable(mail, Tr("Le nom de ce joueur n'est pas encore connu."));
  } else if (!facts.mail_refusal.empty()) {
    Composer::Disable(mail, facts.mail_refusal.c_str());
  }
  // L'ADOPTION, SEULEMENT QUAND CE QUE LE CLIENT SAIT LA PERMET : le serveur
  // refuse presque tout le reste en silence, et une ligne qui ne mène à rien
  // n'apprendrait rien au joueur.
  if (facts.adoption_possible) {
    menu.Add(Tr("Adopter"), EntityVerb::kAdopt,
             Tr("Avec votre conjoint, qui doit être dans ce groupe et porter son alliance.\nS'il "
                "accepte, ce joueur devient pour toujours un bébé de sa classe."));
  }
  // UNE BASCULE, PAS DEUX LIGNES : le blocage est un état que le serveur nous
  // dit, et proposer les deux offrirait toujours une ligne qui ne fait rien.
  const char* whisper_tip =
      Tr("Ses chuchotements ne vous parviennent plus, jusqu'à la "
         "déconnexion. Le chat public, le groupe et la guilde ne sont pas "
         "filtrés.");
  if (facts.whisper_blocked) {
    menu.Add(Tr("Autoriser les chuchotements"), EntityVerb::kWhisperAllow, whisper_tip);
  } else {
    menu.Add(Tr("Bloquer les chuchotements"), EntityVerb::kWhisperBlock, whisper_tip);
  }
  menu.Group();
  menu.Add(Tr("Copier le nom"), EntityVerb::kCopyName);
  AddCompanionFriends(menu, facts);
  const bool both = facts.companion_conducts.size() > 1;
  for (const CompanionConductFacts& conduct : facts.companion_conducts) {
    AddCompanionConduct(menu, conduct, both, !facts.name.empty());
  }
}

// LA TACTIQUE D'UN COMPAGNON envers ce monstre : les rangs, puis les
// déclencheurs, en cases ; puis la retirer, et les réglages avancés.
void AddCompanionTactic(Composer& menu, const CompanionTacticFacts& tactic, bool both) {
  const bool homunculus = tactic.companion == ro::sim::AiCompanion::kHomunculus;
  menu.EnterSubmenu(
      both ? (homunculus ? Tr("Tactique de l'homoncule") : Tr("Tactique du mercenaire"))
           : Tr("Tactique du compagnon"));
  for (std::size_t rank = 0; rank < ro::sim::kTacticRankCount; ++rank) {
    EntityMenuItem& item =
        menu.Add(TacticRankLabel(static_cast<ro::sim::TacticRank>(rank)),
                 static_cast<EntityVerb>(TacticVerb(
                     tactic.companion,
                     static_cast<int>(TacticMenuChoice::kRankFirst) + static_cast<int>(rank))));
    item.toggle = true;
    item.checked = tactic.line.has_value() && static_cast<std::size_t>(tactic.line->rank) == rank;
  }
  menu.Group();
  for (std::size_t trigger = 0; trigger < ro::sim::kTacticTriggerCount; ++trigger) {
    EntityMenuItem& item =
        menu.Add(TacticTriggerLabel(static_cast<ro::sim::TacticTrigger>(trigger)),
                 static_cast<EntityVerb>(TacticVerb(
                     tactic.companion, static_cast<int>(TacticMenuChoice::kTriggerFirst) +
                                           static_cast<int>(trigger))));
    item.toggle = true;
    item.checked =
        tactic.line.has_value() && static_cast<std::size_t>(tactic.line->trigger) == trigger;
  }
  menu.Group();
  EntityMenuItem& remove =
      menu.Add(Tr("Retirer la tactique"),
               static_cast<EntityVerb>(
                   TacticVerb(tactic.companion, static_cast<int>(TacticMenuChoice::kRemove))));
  if (!tactic.line.has_value()) {
    Composer::Disable(remove, Tr("Aucune tactique n'est posée pour ce monstre."));
  }
  menu.Add(Tr("Avancé…"), static_cast<EntityVerb>(TacticVerb(
                              tactic.companion, static_cast<int>(TacticMenuChoice::kAdvanced))));
  menu.LeaveSubmenu();
}

void AddMonster(Composer& menu, const EntityMenuFacts& facts) {
  menu.Add(Tr("Attaquer"), EntityVerb::kAttack);
  AddTarget(menu, facts);
  menu.Group();
  menu.Add(Tr("Fiche du monstre"), EntityVerb::kMonsterSheet);
  menu.Add(Tr("Linker ce monstre"), EntityVerb::kMonsterLink);
  menu.Add(Tr("Copier le nom"), EntityVerb::kCopyName);
  const bool both = facts.companion_tactics.size() > 1;
  for (const CompanionTacticFacts& tactic : facts.companion_tactics) {
    AddCompanionTactic(menu, tactic, both);
  }
}

void AddNpc(Composer& menu, const EntityMenuFacts& facts, const EntityMenuSettings& settings) {
  // Pas d'« Interagir » sur un PORTAIL : un passage n'a pas de dialogue.
  // ⚠ L'ENTRÉE RESTE sur un PNJ bloqué, et c'est voulu : le blocage vise le clic
  // accidentel, pas la volonté de parler.
  if (facts.job != ro::sim::kPortalJob) {
    menu.Add(Tr("Interagir"), EntityVerb::kTalkToNpc);
  }
  menu.Group();
  // LE LIEN EST UNE RECHERCHE PAR LE NOM : ni sur un portail, qui n'en a pas
  // d'utile, ni sur un nom que la balise ne peut pas porter — anonyme, réduit
  // à sa partie cachée, ou à chevron —, qui n'ouvrirait la navigation sur rien.
  if (facts.job != ro::sim::kPortalJob && !NpcSearchTerm(facts.name).empty()) {
    menu.Add(Tr("Linker ce NPC"), EntityVerb::kNpcLink);
  }
  menu.Add(Tr("Copier le nom"), EntityVerb::kCopyName);
  // LA CASE, seulement sur un PNJ épinglé : les autres changent d'identifiant à
  // chaque démarrage du serveur, et une case cochée sur eux désignerait
  // n'importe qui le lendemain.
  if (settings.npc_block && IsFixedIdNpc(facts.id)) {
    menu.Group();
    EntityMenuItem& block = menu.Add(
        Tr("Bloquer le clic gauche"), EntityVerb::kNpcClickBlock,
        Tr("Coché, le clic gauche sur ce NPC ne fait plus rien du tout : ni dialogue, ni "
           "déplacement. Le menu du clic droit reste disponible, « Interagir » compris, et "
           "viser une zone avec un sort continue de marcher par-dessus lui.\n"
           "Réglage du client, gardé sur cette machine : il ne suit pas le personnage, et un "
           "script déclenché en MARCHANT dessus s'exécutera toujours."));
    block.toggle = true;
    block.checked = facts.npc_blocked;
  }
}

// 🔴 LES LIGNES QUI RÉVÈLENT L'IDENTITÉ D'UN OBJET SE GRISENT TANT QU'ELLE N'EST
// PAS CONNUE : un équipement tombé d'un monstre arrive non identifié, sa plaque
// montre le nom générique, et le menu n'a pas à trahir ce que le jeu cache.
void AddGroundItem(Composer& menu, const EntityMenuFacts& facts) {
  menu.Add(Tr("Ramasser"), EntityVerb::kPickup,
           Tr("Comme le clic gauche : le personnage s'approche et ramasse. Le serveur revalide "
              "la distance et le droit de butin."));
  const bool known = facts.item_id != 0 && facts.item_identified;
  const char* why_hidden = facts.item_id == 0
                               ? Tr("Item illisible : il vient sans doute d'être ramassé.")
                               : Tr("Item non identifié : le jeu ne dit pas encore ce que c'est.");
  const auto unless_known = [known, why_hidden](EntityMenuItem& item) {
    if (!known) {
      Composer::Disable(item, why_hidden);
    }
  };
  unless_known(menu.Add(Tr("Description"), EntityVerb::kItemDescription));
  unless_known(menu.Add(Tr("Linker cet item"), EntityVerb::kItemLink));
  // LE RAMASSAGE AUTOMATIQUE : la liste du compte, celle de la description. Une
  // ligne OU l'autre, selon la liste que le serveur a dite ; avant qu'il l'ait
  // dite, aucune. ⚠ SANS SENS SUR UN OBJET INCONNU : dire qu'il est déjà listé
  // trahirait ce qu'il est.
  if (facts.item_autolooted.has_value()) {
    menu.Group();
    const bool listed = known && *facts.item_autolooted;
    const char* label = !known   ? Tr("Ramassage automatique")
                        : listed ? Tr("Retirer du ramassage automatique")
                                 : Tr("Ajouter au ramassage automatique");
    unless_known(menu.Add(label, listed ? EntityVerb::kAutolootRemove : EntityVerb::kAutolootAdd,
                          Tr("La liste @alootid du compte, la même que dans la description : ces "
                             "items vont droit dans l'inventory quand ils tombent pour vous.")));
  }
  // Par le chemin COMPLET d'une ligne tapée : leur réponse revient dans la
  // conversation.
  menu.Group();
  unless_known(menu.Add("@iteminfo", EntityVerb::kItemInfo));
  unless_known(menu.Add("@whodrops", EntityVerb::kWhoDrops));
}

void AddStaff(Composer& menu, const EntityMenuFacts& facts) {
  menu.EnterStaff();
  // L'INSPECTEUR EN PREMIER, sur toute famille : c'est lui qu'on ouvre quand on
  // ne sait pas encore ce qu'on regarde, et les cibles inattendues sont celles
  // qui le méritent.
  menu.Add(Tr("Propriétés…"), EntityVerb::kInspect,
           Tr("Ce que le serveur sait de cette entité : d'où vient un NPC, quel spawn a posé un "
              "monstre, à qui un item au sol est réservé, le compte et l'état d'un joueur.\n"
              "Le serveur revérifie le droit du staff avant de répondre."));
  // L'identifiant seul, puis l'identité brute : ce qu'on va chercher quand une
  // entité se comporte mal. Un identifiant nu ne dit rien à un joueur : ces
  // lignes vivent ici, et nulle part ailleurs.
  const char* copy_id = facts.family == EntityFamily::kSelf     ? Tr("Copier mon AID")
                        : facts.family == EntityFamily::kPlayer ? Tr("Copier l'AID")
                                                                : Tr("Copier le GID");
  menu.Add(copy_id, EntityVerb::kCopyId);
  menu.Add(Tr("Copier l'identité de pick"), EntityVerb::kCopyPickIdentity);

  if (facts.family == EntityFamily::kPlayer) {
    // LE CONFORT : rien de puni ici — c'est ce qu'on fait pendant un event.
    menu.Group();
    menu.Add(Tr("Le faire venir ici"), EntityVerb::kComeHere,
             Tr("Il MARCHE jusqu'à vous — il n'est pas téléporté : le trajet reste visible pour "
                "lui comme pour les autres.\n"
                "Sur votre carte seulement, et seulement s'il existe un chemin. Le serveur le "
                "relève d'abord s'il est assis."));
    menu.Add(Tr("L'asseoir ou le relever"), EntityVerb::kSitStand,
             Tr("Une bascule : le serveur regarde dans quelle position il est et pose l'autre."));
    menu.Add(Tr("Points d'event…"), EntityVerb::kEventPoints,
             Tr("Donne ou retire des points d'event (la monnaie kafra), au plus 50 à la fois."))
        .question = EntityMenuQuestion::kEventPoints;

    // 🔴 LA PAROLE EN DEUX LIGNES, PAS UNE BASCULE : le client ignore si la
    // cible est déjà muette, et une bascule rendrait la parole à qui vient d'être
    // puni par quelqu'un d'autre.
    menu.Group();
    menu.Add(Tr("Le rendre muet (60 min)"), EntityVerb::kMute,
             Tr("Ajoute 60 minutes de silence. Appliqué deux fois, il prolonge — ce n'est pas un "
                "interrupteur."));
    menu.Add(Tr("Lui rendre la parole"), EntityVerb::kUnmute);
    menu.Add(Tr("Journal des blocages de chat"), EntityVerb::kChatBlockLog,
             Tr("Les @mute et @unmute qui ont visé ce personnage, datés, avec leur auteur et "
                "leur durée, tels que le journal des commandes du serveur les a gardés.\n"
                "Le serveur revérifie le droit du staff avant de répondre."));

    // CE QUI PUNIT, en rouge et en dernier : on ne clique pas là par erreur en
    // visant la ligne du dessus.
    menu.Group();
    menu.Add(Tr("Le foudroyer"), EntityVerb::kNuke,
             Tr("@nuke : une explosion sur sa case. Il meurt, avec la pénalité de mort "
                "habituelle."))
        .danger = true;
    menu.Add(Tr("Expulser (kick)"), EntityVerb::kKick,
             Tr("Le serveur revalide la permission avant d'agir."))
        .danger = true;
    menu.Add(Tr("L'emprisonner ou le libérer"), EntityVerb::kJail,
             Tr("Une bascule : le serveur regarde s'il est déjà en prison et fait l'inverse.\n"
                "Un joueur emprisonné vit sur sec_pri — hors de cette carte, cette entrée ne "
                "peut que l'y envoyer. Sans durée : pour une peine bornée, @jailfor."))
        .danger = true;
    EntityMenuItem& block =
        menu.Add(Tr("Bannir le compte…"), EntityVerb::kBlockAccount,
                 Tr("@block : bannissement DÉFINITIF du compte, pas du personnage. Tous ses "
                    "personnages tombent avec lui."));
    block.danger = true;
    block.question = EntityMenuQuestion::kAccountBlock;
  }

  // 🔴 NIVEAU 99, PAS 80 : tout ce qui précède AFFICHE ou vise un joueur, ces
  // trois-là MODIFIENT le monde pour tous les joueurs connectés. Le serveur
  // refait le test — la ligne absente n'est pas une garde.
  if (facts.family == EntityFamily::kNpc && facts.admin) {
    // Le rechargement en premier : c'est le geste courant, et le seul des trois
    // qui ne retire rien.
    menu.Group();
    menu.Add(Tr("Recharger son fichier de script"), EntityVerb::kNpcReloadFile,
             Tr("Recharge le FICHIER d'où vient ce NPC, pas seulement lui : tous les NPC déclarés "
                "dans ce fichier sont déchargés puis relus, et leurs événements OnInit rejoués.\n"
                "Les mapflags et les monstres posés directement par le script ne sont PAS "
                "retirés — ils s'ajouteront à ceux déjà en place.\n"
                "Le serveur dit dans le chat ce qu'il a fait."));
    menu.Add(Tr("Déplacer ici"), EntityVerb::kNpcMoveHere,
             Tr("Pose le NPC sur votre case. Rien n'est enregistré : il retrouvera sa position "
                "d'origine au prochain rechargement de son fichier ou du serveur."));
    EntityMenuItem& unload = menu.Add(
        Tr("Décharger ce NPC…"), EntityVerb::kNpcUnload,
        Tr("Retire ce NPC du monde pour TOUS les joueurs connectés, avec ses duplicates. Il "
           "faudra recharger son fichier pour le faire revenir."));
    unload.danger = true;
    unload.question = EntityMenuQuestion::kNpcUnload;
  }
}

// NOTRE PET : sa fiche, puis les quatre ordres du client, dans son ordre.
// « Remettre dans l'œuf » est en rouge : il range le pet pour de bon.
void AddOwnPet(Composer& menu, const EntityMenuFacts& facts) {
  menu.Add(Tr("Statut du pet"), EntityVerb::kPetStatus);
  // ⚠ CE QUI EMPÊCHE DE NOURRIR SE DIT AVANT LA QUESTION, comme dans le jeu :
  // la ligne se grise plutôt que de confirmer un repas qui sera refusé.
  EntityMenuItem& feed = menu.Add(Tr("Nourrir"), EntityVerb::kPetFeed);
  feed.question = EntityMenuQuestion::kPetFeed;
  if (!facts.pet_feed_refusal.empty()) {
    Composer::Disable(feed, facts.pet_feed_refusal.c_str());
  }
  menu.Add(Tr("Spectacle"), EntityVerb::kPetPerform);
  EntityMenuItem& unequip = menu.Add(Tr("Retirer l'accessoire"), EntityVerb::kPetUnequip);
  if (!facts.pet_has_accessory) {
    Composer::Disable(unequip, Tr("Ce pet ne porte aucun accessoire."));
  }
  menu.Add(Tr("Remettre dans l'œuf"), EntityVerb::kPetToEgg).danger = true;
}

// « EN ATTENTE » : une case, cochée tant que le compagnon colle au maître sans
// riposter. C'est un ordre à l'IA du compagnon, que le serveur ne voit pas.
void AddStandby(Composer& menu, const EntityMenuFacts& facts, EntityVerb verb) {
  EntityMenuItem& standby = menu.Add(Tr("En attente"), verb);
  standby.toggle = true;
  standby.checked = facts.companion_standby;
}

// NOTRE HOMONCULE.
void AddOwnHomunculus(Composer& menu, const EntityMenuFacts& facts) {
  menu.Add(Tr("Statut de l'homoncule"), EntityVerb::kHomunculusStatus);
  EntityMenuItem& feed = menu.Add(Tr("Nourrir"), EntityVerb::kHomunculusFeed);
  feed.question = EntityMenuQuestion::kHomunculusFeed;
  if (facts.homunculus_resting) {
    Composer::Disable(feed, Tr("L'homoncule est au repos : le serveur refuse de le nourrir."));
  }
  AddStandby(menu, facts, EntityVerb::kHomunculusStandby);
}

}  // namespace

const char* EntityFamilyLabel(EntityFamily family) {
  switch (family) {
    case EntityFamily::kSelf:
      return Tr("Moi");
    case EntityFamily::kPlayer:
      return Tr("Joueur");
    case EntityFamily::kMonster:
      return Tr("Monstre");
    case EntityFamily::kNpc:
      return "NPC";
    case EntityFamily::kSkillUnit:
      return Tr("Unité");
    case EntityFamily::kGroundItem:
      return Tr("Item au sol");
    case EntityFamily::kOwnPet:
      return "Pet";
    case EntityFamily::kOwnHomunculus:
      return Tr("Homoncule");
    case EntityFamily::kOwnMercenary:
      return Tr("Mercenaire");
    case EntityFamily::kNone:
    case EntityFamily::kOther:
      break;
  }
  return Tr("Entité");
}

EntityFamily ClassifyEntity(const ro::sim::Actor& actor, std::uint32_t own_id,
                            const OwnCompanionIds& companions) {
  if (actor.id == 0) {
    return EntityFamily::kNone;
  }
  switch (actor.familiar) {
    case ro::sim::Familiar::kPet:
      if (actor.id == companions.pet_id) {
        return EntityFamily::kOwnPet;
      }
      break;
    case ro::sim::Familiar::kHomunculus:
      if (actor.id == companions.homunculus_id) {
        return EntityFamily::kOwnHomunculus;
      }
      break;
    case ro::sim::Familiar::kMercenary:
      if (actor.id == companions.mercenary_id) {
        return EntityFamily::kOwnMercenary;
      }
      break;
    case ro::sim::Familiar::kNone:
    case ro::sim::Familiar::kElemental:
      break;
  }
  switch (actor.kind) {
    case ro::sim::ActorKind::kSkillUnit:
      return EntityFamily::kSkillUnit;
    case ro::sim::ActorKind::kItem:
      return EntityFamily::kGroundItem;
    case ro::sim::ActorKind::kPlayer:
      return actor.id == own_id ? EntityFamily::kSelf : EntityFamily::kPlayer;
    case ro::sim::ActorKind::kMonster:
      // 🔴 UN GID ÉPINGLÉ NE PEUT ÊTRE QU'UN PNJ, quel que soit ce qu'il montre :
      // un PNJ déguisé garde un corps de monstre, et se verrait sinon proposer
      // « Attaquer » sans sa case de blocage.
      return IsFixedIdNpc(actor.id) ? EntityFamily::kNpc : EntityFamily::kMonster;
    case ro::sim::ActorKind::kNonPlayer:
      // 🔴 UN FAMILIER N'EST PAS UN PNJ, bien que le monde les range ensemble :
      // le sien a son propre menu, que ce client n'a pas encore, et celui d'autrui
      // n'a aucune action de jeu. Il ne lui reste que son identité.
      return actor.familiar == ro::sim::Familiar::kNone ? EntityFamily::kNpc : EntityFamily::kOther;
    case ro::sim::ActorKind::kUnknown:
      break;
  }
  return EntityFamily::kOther;
}

bool WorldClickOpensMenu(EntityFamily family, std::uint32_t id, const WorldMenuGesture& gesture,
                         bool storage_open, const EntityMenuSettings& settings, bool staff) {
  // Les gardes du client d'origine : sans elles, le menu s'ouvrirait pendant un
  // glisser, par-dessus une fenêtre, ou coffre ouvert.
  if (gesture.left_held || gesture.shift_held || gesture.over_interface || storage_open ||
      family == EntityFamily::kNone) {
    return false;
  }
  // Les entités que le client d'origine n'a JAMAIS servies restent derrière
  // leur réglage.
  const bool native_served = family == EntityFamily::kPlayer || family == EntityFamily::kOwnPet ||
                             family == EntityFamily::kOwnHomunculus ||
                             family == EntityFamily::kOwnMercenary;
  // 🔴 UN PNJ ÉPINGLÉ S'OUVRE MÊME SANS « TOUTES LES ENTITÉS » : ce menu est le
  // SEUL endroit d'où l'on décoche son blocage. Derrière un autre réglage, un PNJ
  // bloqué le resterait sans aucun moyen visible d'en revenir depuis le monde.
  const bool blockable_npc = family == EntityFamily::kNpc && settings.npc_block && IsFixedIdNpc(id);
  // 🔴 LE STAFF OUTILLÉ OUVRE SUR TOUT : un inspecteur qu'il faut armer d'un
  // second réglage — de joueur, en plus — est un inspecteur qu'on ne trouve pas.
  const bool staff_tools = settings.staff_extras && staff;
  if (!native_served && !settings.all_entities && !blockable_npc && !staff_tools) {
    return false;
  }
  // Sur soi-même, décoché : le clic droit repart au jeu intact.
  if (family == EntityFamily::kSelf && !settings.self_menu) {
    return false;
  }
  // 🔴 UNE UNITÉ AU SOL ET CE QU'ON NE CLASSE PAS N'ONT AUCUNE ACTION DE JEU : il
  // n'en reste que l'identité brute, un outil de débogage. Pour un joueur, ce
  // menu ne ferait qu'avaler son clic droit sans rien lui offrir.
  const bool diagnostic_only = family == EntityFamily::kSkillUnit || family == EntityFamily::kOther;
  if (diagnostic_only && !staff_tools) {
    return false;
  }
  // Alt + clic droit sur un MONSTRE est un ordre au familier : on ne s'en mêle
  // pas.
  return !(gesture.alt_held && family == EntityFamily::kMonster);
}

bool FrameClickOpensMenu(EntityFamily family, const EntityMenuSettings& settings) {
  if (family == EntityFamily::kNone) {
    return false;
  }
  return family != EntityFamily::kSelf || settings.self_menu;
}

ro::ui::EntityMenuView BuildEntityMenu(const EntityMenuFacts& facts,
                                       const EntityMenuSettings& settings) {
  ro::ui::EntityMenuView view;
  view.subject = facts.id;
  view.name = facts.name;
  view.family = EntityFamilyLabel(facts.family);
  Composer menu(view);

  switch (facts.family) {
    case EntityFamily::kPlayer:
      AddPlayer(menu, facts);
      break;
    case EntityFamily::kSelf:
      menu.Add(Tr("Copier mon nom"), EntityVerb::kCopyName);
      break;
    case EntityFamily::kMonster:
      AddMonster(menu, facts);
      break;
    case EntityFamily::kNpc:
      AddNpc(menu, facts, settings);
      break;
    case EntityFamily::kGroundItem:
      AddGroundItem(menu, facts);
      break;
    case EntityFamily::kOwnPet:
      AddOwnPet(menu, facts);
      break;
    case EntityFamily::kOwnHomunculus:
      AddOwnHomunculus(menu, facts);
      break;
    case EntityFamily::kOwnMercenary:
      menu.Add(Tr("Statut du mercenaire"), EntityVerb::kMercenaryStatus);
      AddStandby(menu, facts, EntityVerb::kMercenaryStandby);
      break;
    case EntityFamily::kSkillUnit:
    case EntityFamily::kOther:
      // Ces deux-là ne s'ouvrent que pour le staff : le reste du menu est sa
      // section.
      menu.Add(Tr("Copier le nom"), EntityVerb::kCopyName);
      break;
    case EntityFamily::kNone:
      return view;
  }

  // « PAS CELLE-CI » : proposé UNIQUEMENT quand la fenêtre montre cette entité.
  // Une entrée « masquer » sur une fenêtre absente ferait douter de ce qui est
  // ouvert.
  if (facts.target_frame_shown) {
    menu.Group();
    menu.Add(Tr("Masquer la fenêtre de cible"), EntityVerb::kHideTargetFrame,
             Tr("Pour cette cible seulement : elle revient dès que tu en désignes une autre."));
  }

  if (facts.staff && settings.staff_extras) {
    AddStaff(menu, facts);
  }
  return view;
}

}  // namespace moonclient
