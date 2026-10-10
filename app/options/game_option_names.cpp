// LES RANGÉES DES ÉTIQUETTES D'ENTITÉ, un onglet par espèce.
//
// ⚠ À PART DU CATALOGUE, pour la raison des barres d'entité : ceci est une
// BOUCLE, pas une liste de littéraux qu'on copie six fois. Les douze réglages
// s'écrivent une fois, et chaque onglet ne garde que ceux qui ont un sens sur
// son espèce — voir `entity_name_fields.h`, qui dit lesquels et pourquoi.
//
// 🔴 CE QUI RESTE AU-DESSUS DES ONGLETS N'EST PAS « CE QUI SE RESSEMBLE », C'EST
// « CE QUI NE PEUT PAS DIFFÉRER » : le maître, la taille de la police — l'atlas
// est GRAVÉ à une taille, six en voudraient six — et la composition du client,
// qui est une lettre à suivre ou non, pas un goût par espèce.
//
// 🔴 ET CE QUE LE SERVEUR NE DIT PAS SE DIT DANS L'INFOBULLE. Un joueur qui
// coche « Clan et guilde » sur les monstres doit apprendre ICI ce qui remplit
// ces places-là, sinon il cherchera la panne dans son écran.

#include <algorithm>
#include <cstddef>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/render/entity_names.h"

#include "options/entity_name_fields.h"
#include "options/game_options.h"
#include "options/option_row_kinds.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les formats des curseurs. ⚠ L'UNITÉ EST DANS LE FORMAT et non dans le
// libellé : « Écart » suivi de « 6 » ne dit pas si ce sont des pixels.
constexpr const char* kPixelFormat = "%.0f px";
constexpr const char* kPercentFormat = "%.0f %%";

// Les bornes d'un pour cent réglé au curseur, en centièmes dans le réglage.
constexpr float kPercentScale = 100.0F;

}  // namespace

// CE QUI VAUT POUR LES SIX ESPÈCES, puis les onglets.
void GameOptions::BuildEntityNameShared() {
  entity_name_shared_rows_.clear();
  entity_name_shared_rows_.push_back(
      Toggle(GameOption::kEntityNamesShown, Tr("Montrer les noms"),
             Tr("Les étiquettes posées sous les pieds de ce qui peuple la carte.\n\n"
                "Le client d'origine n'en écrit qu'UNE à la fois — celle de ce que la souris "
                "désigne. Ici, chaque espèce décide de la sienne."),
             nullptr, entity_names_.shown));
  entity_name_shared_rows_.push_back(OptionRowView{
      .id = static_cast<int>(GameOption::kEntityNamesFont),
      .label = Tr("Taille du texte"),
      .help = Tr("Elle vaut pour les six espèces, et ce n'est pas un choix : la police des "
                 "noms est GRAVÉE une fois à cette taille-là, et six tailles voudraient six "
                 "gravures.\n\n"
                 "Douze pixels est la taille du client d'origine."),
      .kind = OptionKind::kSlider,
      .value = entity_names_.font_pixels,
      .lowest = ro::render::kSmallestNamePlateFont,
      .highest = ro::render::kLargestNamePlateFont,
      .format = kPixelFormat,
      .disabled = !entity_names_.shown});
  entity_name_shared_rows_.push_back(
      Toggle(GameOption::kEntityNamesEmblem, Tr("Emblème de guilde"),
             Tr("L'emblème de la guilde à gauche de l'étiquette, comme « /emblem » dans le client "
                "d'origine. Il passe devant l'insigne de boss.\n\n"
                "Il ne paraît qu'une fois téléchargé : la première étiquette d'une guilde peut "
                "s'écrire sans lui, le temps qu'il arrive."),
             nullptr, entity_names_.guild_emblem));
  entity_name_shared_rows_.back().disabled = !entity_names_.shown;
  // LES ONGLETS. ⚠ REFAITS À CHAQUE IMAGE : ces noms se traduisent, et un nom
  // mis en cache resterait dans la langue d'avant.
  entity_name_tabs_.clear();
  entity_name_tabs_.reserve(ro::render::kNamePlateKindCount);
  for (std::size_t kind = 0; kind < ro::render::kNamePlateKindCount; ++kind) {
    entity_name_tabs_.push_back(
        ro::render::EntityNameKindName(static_cast<ro::render::NamePlateKind>(kind)));
  }
}

// CE QUE LE SERVEUR MET DANS L'ÉTIQUETTE DE CETTE ESPÈCE-LÀ — l'infobulle qui
// évite de chercher une panne qui n'existe pas.
//
// 🔴 PAR ESPÈCE, PARCE QUE LA RÉPONSE L'EST. Un texte commun dirait « selon les
// cas », ce qui n'aide personne à comprendre pourquoi une case cochée ne change
// rien. C'est aussi ici que se dit ce que l'onglet N'A PAS : une rangée retirée
// ne peut plus expliquer son absence.
const char* GameOptions::EntityNameSourceHelp(std::size_t kind) {
  switch (static_cast<ro::render::NamePlateKind>(kind)) {
    case ro::render::NamePlateKind::kOwn:
      return Tr(
          "Le client d'origine n'écrit pas ton nom sous tes pieds — tu sais où tu es, et "
          "l'étiquette ne ferait que couvrir le sol sur lequel tu marches.");
    case ro::render::NamePlateKind::kPlayer:
      return Tr(
          "Un nom de joueur se coupe au croisillon : ce qui suit un « # » ne s'écrit pas, "
          "et c'est le client d'origine qui fait ainsi.\n\n"
          "Attention : sa carte de nom — groupe, guilde, titre — n'arrive QUE si le serveur nous "
          "l'envoie, et il ne le fait qu'aux instants où elle change. Un joueur déjà posé "
          "quand on arrive n'en enverra jamais : nous la demandons alors nous-mêmes, pour "
          "ce que la souris désigne.");
    case ro::render::NamePlateKind::kNonPlayer:
      return Tr(
          "Un PNJ reçoit son titre EN TOUTES LETTRES, là où un joueur reçoit un numéro "
          "que la table de l'installation traduit.\n\n"
          "Son paquet de nom ne porte QUE ces deux choses : ni parenthèse ni seconde "
          "ligne n'existent pour lui, et c'est pourquoi cet onglet ne les propose pas.\n\n"
          "Attention : un portail est un PNJ sans nom : son étiquette reste vide quoi qu'on règle "
          "ici.");
    case ro::render::NamePlateKind::kMonster:
    case ro::render::NamePlateKind::kBoss:
      return Tr(
          "Ce que ce serveur-ci écrit entre parenthèses sur un monstre, c'est son NIVEAU "
          "et sa VIE en pour cent — le réglage show_mob_info du serveur, que le client "
          "d'origine jette avec sa parenthèse.\n\n"
          "Sa seconde ligne porte sa RACE, et entre crochets son ÉLÉMENT : ces places-là "
          "ne sont donc pas vides sur un monstre, elles ne portent simplement pas ce que "
          "leur nom de protocole annonce.");
    case ro::render::NamePlateKind::kGroundItem:
      return Tr(
          "Le serveur ne nomme JAMAIS un item au sol : clif_name ne répond pas sur un "
          "butin. C'est nous qui composons son nom depuis la table de l'installation, et "
          "la quantité ramassable prend la place de la parenthèse.\n\n"
          "Il n'a donc ni titre ni seconde ligne — rien ne les remplirait —, et cet "
          "onglet ne les propose pas.\n\n"
          "Attention : sans le catalogue d'items, un butin n'a pas de nom du tout.");
  }
  return nullptr;
}

// LES RÉGLAGES D'UNE ESPÈCE — les douze, moins ceux qui n'ont pas de sens ici.
//
// 🔴 CE QUI NE S'APPLIQUE PAS MAINTENANT EST GRISÉ, CE QUI NE S'APPLIQUE JAMAIS
// EST ABSENT. Une ligne qui disparaît quand on éteint son maître fait sauter la
// mise en page sous la main du joueur, et il cherche ce qu'il vient de perdre :
// grisée, elle dit « pas maintenant », et OÙ elle est. Une ligne qui ne pourra
// rien faire sur cet onglet-là n'a pas cette excuse — changer d'onglet est déjà
// changer de page, rien ne saute, et la garder promettrait une propriété que
// l'espèce n'a pas. Voir `entity_name_fields.h`.
void GameOptions::BuildEntityNameRows(int shown_kind) {
  BuildEntityNameShared();

  const bool names_off = !entity_names_.shown;
  entity_name_rows_.clear();
  // UN ONGLET SANS ESPÈCE DE NOM — le groupe — n'a que les réglages communs.
  if (shown_kind < 0) {
    return;
  }
  const auto kind = static_cast<std::size_t>(
      std::clamp(shown_kind, 0, static_cast<int>(ro::render::kNamePlateKindCount) - 1));
  ro::render::EntityNameLook& look = entity_names_.kinds[kind];

  // 🔴 NI UN `static`, NI UN LOCAL : une rangée ne COPIE pas ses choix, elle en
  // garde un `span`. Un `static` les capture une fois et le catalogue qui les
  // porte est remplacé au premier changement de langue ; un tableau local meurt
  // avant le dessin. Les deux mènent au même écran noir.
  entity_name_when_names_.clear();
  entity_name_when_names_.push_back(Tr("Toujours"));
  entity_name_when_names_.push_back(Tr("Au survol"));
  entity_name_when_names_.push_back(Tr("La cible"));
  entity_name_when_names_.push_back(Tr("Survol ou cible"));

  const bool off = names_off || !look.shown;

  // 🔴 TOUTE RANGÉE PASSE PAR ICI, ET C'EST CE QUI REND LA RÈGLE INÉVITABLE. Un
  // `if` écrit à côté de chaque ligne serait celui qu'on oublie le jour où un
  // treizième réglage s'ajoute ; ici, un champ qui n'appartient pas à l'espèce
  // ne PEUT pas atteindre la liste. C'est aussi ce qui écrit l'identifiant, donc
  // il ne peut plus nommer un autre champ que celui de la ligne.
  const char* heading_to_place = nullptr;
  const auto push = [this, kind, &heading_to_place](EntityNameField field, OptionRowView row) {
    // ⚠ L'INTERTITRE SUIT LA PREMIÈRE RANGÉE QUI RESTE : porté par une rangée
    // retirée, il partirait avec elle et la section s'ouvrirait sans titre.
    if (row.heading != nullptr) {
      heading_to_place = row.heading;
      row.heading = nullptr;
    }
    if (!EntityNameFieldApplies(static_cast<ro::render::NamePlateKind>(kind), field)) {
      return;
    }
    row.id = EntityNameRow(kind, field);
    row.heading = heading_to_place;
    heading_to_place = nullptr;
    entity_name_rows_.push_back(row);
  };

  push(EntityNameField::kShown, OptionRowView{.label = Tr("Montrer pour cette espèce"),
                                              .help = EntityNameSourceHelp(kind),
                                              .kind = OptionKind::kToggle,
                                              .on = look.shown,
                                              .disabled = names_off});
  push(EntityNameField::kWhen,
       OptionRowView{
           .label = Tr("Quand la montrer"),
           .help = Tr("« Au survol » est ce que fait le client d'origine : un seul nom s'écrit à "
                      "la fois, celui de ce que la souris désigne, et c'est ce qui garde une "
                      "carte peuplée lisible.\n\n"
                      "« Toujours » écrit toutes les étiquettes de cette espèce, ce qui se "
                      "défend sur les PNJ et sur les items au sol, beaucoup moins sur trente "
                      "monstres."),
           .kind = OptionKind::kChoice,
           .chosen = static_cast<int>(look.when),
           .choices = entity_name_when_names_,
           .disabled = off});

  // ── CE QUE L'ÉTIQUETTE ÉCRIT ──────────────────────────────────────────────
  push(EntityNameField::kTitle,
       OptionRowView{.label = Tr("Le titre"),
                     .help = Tr("Il se met DEVANT le nom, entre crochets — « [Master] Stingor »."),
                     .kind = OptionKind::kToggle,
                     .on = look.title,
                     .heading = Tr("Ce qu'elle écrit"),
                     .disabled = off});
  push(EntityNameField::kAside,
       OptionRowView{
           .label = Tr("Champs de party"),
           .help = Tr("La parenthèse à la suite du nom. Sur un monstre, ce serveur-ci y met son "
                      "niveau et sa vie ; sur un item au sol, la quantité ramassable ; sur un "
                      "joueur, ce que sa carte de nom porte."),
           .kind = OptionKind::kToggle,
           .on = look.aside,
           .disabled = off});
  push(EntityNameField::kBanner,
       OptionRowView{
           .label = Tr("La seconde ligne"),
           .help = Tr("Le groupe, la guilde ou le clan, et son insigne entre crochets.\n\n"
                      "Attention : une bannière vide emporte l'insigne : un membre de clan, dont "
                      "le serveur ne remplit QUE l'insigne, ne montre donc rien de son clan. "
                      "C'est le client d'origine qui est ainsi."),
           .kind = OptionKind::kToggle,
           .on = look.banner,
           .disabled = off});
  push(EntityNameField::kBossBadge,
       OptionRowView{
           .label = Tr("L'insigne de boss"),
           .help = Tr("La tête de mort posée à gauche du nom : ARGENT pour un mini-boss, DORÉE "
                      "pour un MVP. Le rang vient du paquet d'apparition, et c'est le seul "
                      "signe qui dise vraiment « boss ».\n\n"
                      "Attention : le nom ROUGE, lui, n'en est pas un : il dit que la cible te "
                      "dépasse de plus de dix niveaux. Voir « La couleur d'écart de niveau »."),
           .kind = OptionKind::kToggle,
           .on = look.boss_badge,
           .disabled = off});

  // ── LES COULEURS ──────────────────────────────────────────────────────────
  push(EntityNameField::kColor,
       OptionRowView{.label = Tr("Couleur du nom"),
                     .help = Tr("Les défauts sont ceux du client d'origine : blanc pour un "
                                "joueur, rose clair pour un monstre."),
                     .kind = OptionKind::kColor,
                     .color = look.color,
                     .heading = Tr("Les couleurs"),
                     .disabled = off});
  push(EntityNameField::kLevelGap,
       OptionRowView{
           .label = Tr("La couleur d'écart de niveau"),
           .help = Tr("Elle ÉCRASE la couleur ci-dessus : rouge quand la cible te dépasse de "
                      "plus de dix niveaux, gris quand tu la dépasses de plus de quinze. C'est "
                      "une surcharge du client d'origine, et c'est elle qu'on prend pour la "
                      "marque d'un boss.\n\n"
                      "Elle ne se règle que sur un monstre, et elle n'agit que là : le client "
                      "d'origine la garde derrière un « ce n'est pas un job de joueur », et "
                      "le niveau des autres joueurs ne s'écrit pas par ce chemin."),
           .kind = OptionKind::kToggle,
           .on = look.level_gap,
           .indented = true,
           .disabled = off});
  push(EntityNameField::kShadow,
       OptionRowView{
           .label = Tr("L'ombre portée"),
           .help = Tr("Ce qui rend un nom lisible sur un sol clair. Le client d'origine en pose "
                      "une sous chacun des siens, en réécrivant le texte quatre fois autour de "
                      "lui."),
           .kind = OptionKind::kToggle,
           .on = look.shadow,
           .disabled = off});
  push(EntityNameField::kShadowColor, OptionRowView{.label = Tr("Couleur de l'ombre"),
                                                    .kind = OptionKind::kColor,
                                                    .color = look.shadow_color,
                                                    .indented = true,
                                                    .disabled = off || !look.shadow});

  // ── LA PLACE ──────────────────────────────────────────────────────────────
  push(EntityNameField::kOffset,
       OptionRowView{
           .label = Tr("Écart sous les pieds"),
           .help =
               Tr("De combien l'étiquette descend sous les pieds. Négatif, elle remonte vers "
                  "le sprite.\n\n"
                  "Attention : une barre de vie posée du même côté la pousse encore : les deux ne "
                  "se chevauchent jamais, et c'est la barre qui gagne."),
           .kind = OptionKind::kSlider,
           .value = look.offset_pixels,
           .lowest = ro::render::kSmallestNamePlateOffset,
           .highest = ro::render::kLargestNamePlateOffset,
           .format = kPixelFormat,
           .heading = Tr("La place"),
           .disabled = off});
  push(EntityNameField::kOpacity,
       OptionRowView{.label = Tr("Opacité"),
                     .help = Tr("Elle s'applique à tout — le texte, son ombre et l'insigne."),
                     .kind = OptionKind::kSlider,
                     .value = look.opacity * kPercentScale,
                     .lowest = 0.0F,
                     .highest = kPercentScale,
                     .format = kPercentFormat,
                     .disabled = off});
}

// CE QU'UNE LIGNE D'ÉTIQUETTE CHANGE.
//
// ⚠ ELLE REND FAUX SUR CE QUI NE LA CONCERNE PAS, et c'est ce qui laisse les
// trois aiguillages se suivre sans se marcher dessus.
bool GameOptions::ChangeEntityName(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kEntityNameRowBase) {
    return false;
  }
  const int offset = asked.changed - kEntityNameRowBase;
  const auto kind = static_cast<std::size_t>(offset / kEntityNameFieldsPerKind);
  if (kind >= ro::render::kNamePlateKindCount) {
    return false;
  }
  ro::render::EntityNameLook& look = entity_names_.kinds[kind];
  switch (static_cast<EntityNameField>(offset % kEntityNameFieldsPerKind)) {
    case EntityNameField::kShown:
      look.shown = asked.on;
      break;
    case EntityNameField::kWhen:
      look.when = ro::render::NamePlateWhenOf(asked.chosen);
      break;
    case EntityNameField::kTitle:
      look.title = asked.on;
      break;
    case EntityNameField::kAside:
      look.aside = asked.on;
      break;
    case EntityNameField::kBanner:
      look.banner = asked.on;
      break;
    case EntityNameField::kBossBadge:
      look.boss_badge = asked.on;
      break;
    case EntityNameField::kColor:
      look.color = asked.color;
      break;
    case EntityNameField::kLevelGap:
      look.level_gap = asked.on;
      break;
    case EntityNameField::kShadow:
      look.shadow = asked.on;
      break;
    case EntityNameField::kShadowColor:
      look.shadow_color = asked.color;
      break;
    case EntityNameField::kOffset:
      look.offset_pixels = asked.value;
      break;
    case EntityNameField::kOpacity:
      // ⚠ LE CURSEUR COMPTE EN POUR CENT, LE RÉGLAGE EN PART : diviser ici et
      // multiplier à l'affichage, jamais l'inverse.
      look.opacity = asked.value / kPercentScale;
      break;
    case EntityNameField::kCount:
      return false;
  }
  return true;
}

}  // namespace moonclient
