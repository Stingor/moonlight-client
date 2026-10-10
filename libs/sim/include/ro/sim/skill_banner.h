#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace ro::sim {

// LA BANNIÈRE DU NOM D'UNE COMPÉTENCE, au-dessus de qui la lance.
//
// 🔴 CE N'EST PAS UNE INCRUSTATION À PART : le client l'envoie à l'acteur comme
// une PAROLE. Le répartiteur de commandes du mode de jeu (cas 86) compose
// « <nom> !!  » puis envoie à l'acteur le message **7**, celui d'une phrase de
// chat — et c'est pourquoi la bannière prend l'aspect, la place et la durée
// d'une bulle de dialogue.
//
// Le nom vient du Lua, c'est-à-dire du libellé
// affiché de `skillinfolist.lub` : voir `SkillNode::display_name`.

// Ce que le client ajoute derrière le nom d'un partenaire : espace, deux points
// d'exclamation, deux espaces.
//
// ⚠ IL Y A UNE VARIANTE CORÉENNE, ajoutée AVANT celle-ci pour le service
// coréen. Ce service-là n'est pas le nôtre.
inline constexpr std::string_view kSkillBannerSuffix = " !!  ";

// Derrière le nom d'une COMPÉTENCE, le suffixe est plus court : sans les deux
// espaces de queue.
inline constexpr std::string_view kSkillNameBannerSuffix = " !!";

// LES COMPÉTENCES QU'UN JOUEUR ANNONCE SANS BANNIÈRE.
//
// ⚠ UN MONSTRE OU UN PNJ LA MONTRE QUAND MÊME : le client ne consulte ce
// drapeau que pour un joueur — un métier de monstre ou de PNJ le force à vrai.
// Écarter ces treize pour tout le monde priverait les monstres
// d'une annonce qu'ils font.
inline constexpr std::array<std::uint16_t, 13> kSkillsWithoutPlayerBanner = {
    40, 41, 51, 135, 389, 2033, 2247, 2290, 2342, 2535, 3001, 5382, 5383};

// CELLES QUE PERSONNE N'ANNONCE, joueur comme monstre.
inline constexpr std::array<std::uint16_t, 3> kSkillsWithoutAnyBanner = {318, 326, 492};

// CELLES QU'UN MONSTRE OU UN PNJ TAIT, ici et non chez le client d'origine :
// l'invocation de suivants (196) et leur rappel (209), qu'une Bloody Branch
// lâche en nombre, couvriraient l'écran de bulles « … !! ». Un joueur qui les
// lance garde sa bannière.
inline constexpr std::array<std::uint16_t, 2> kSkillsWithoutCreatureBanner = {196, 209};

// ⚠ LA 336 MONTRE UN NOM PORTÉ PAR L'ACTEUR, pas celui de la compétence. Nous
// la traitons comme les autres faute de savoir ce que ce nom est — le noter
// vaut mieux que l'inventer.
inline constexpr std::uint16_t kSkillBannerShowsActorName = 336;

// LES COMPÉTENCES DE COUPLE NOMMENT LEUR PARTENAIRE : la bannière dit « <nom de
// la cible> !!  » puis une phrase à elle (`LoveSkillText`), et non le nom de
// la compétence.
inline constexpr std::array<std::uint16_t, 2> kSkillsBannerNamingTarget = {334, 335};

[[nodiscard]] constexpr bool SkillBannerNamesTarget(std::uint16_t skill_id) {
  return std::ranges::find(kSkillsBannerNamingTarget, skill_id) != kSkillsBannerNamingTarget.end();
}

// Si cette compétence, lancée par cet acteur, montre sa bannière.
[[nodiscard]] constexpr bool SkillBannerShows(std::uint16_t skill_id, bool monster_or_npc) {
  if (skill_id == 0) {
    return false;
  }
  if (std::ranges::find(kSkillsWithoutAnyBanner, skill_id) != kSkillsWithoutAnyBanner.end()) {
    return false;
  }
  if (monster_or_npc) {
    return std::ranges::find(kSkillsWithoutCreatureBanner, skill_id) ==
           kSkillsWithoutCreatureBanner.end();
  }
  return std::ranges::find(kSkillsWithoutPlayerBanner, skill_id) ==
         kSkillsWithoutPlayerBanner.end();
}

// UNE COMPÉTENCE POSÉE SUR UNE CASE N'ANNONCE PAS UNE CRÉATURE. Le client ne
// demande la bannière, à ce paquet, que sur le chemin d'un lanceur dont le
// métier AFFICHÉ n'est pas celui d'une créature ou d'un PNJ — de 1001 à 3998,
// et de 20021 à 32042, les bornes de `ro::render::IsMonsterOrNpcClass`.
inline constexpr std::uint32_t kFirstCreatureClass = 1001;
inline constexpr std::uint32_t kLastCreatureClass = 3998;
inline constexpr std::uint32_t kFirstExtendedCreatureClass = 20021;
inline constexpr std::uint32_t kLastExtendedCreatureClass = 32042;

[[nodiscard]] constexpr bool GroundSkillAnnouncedBy(std::uint32_t shown_job) {
  const bool creature =
      (shown_job >= kFirstCreatureClass && shown_job <= kLastCreatureClass) ||
      (shown_job >= kFirstExtendedCreatureClass && shown_job <= kLastExtendedCreatureClass);
  return !creature;
}

// L'INCANTATION N'ANNONCE PAS UNE CRÉATURE NON PLUS, et le serveur l'envoie pour
// toute compétence, même instantanée : c'est le seul paquet que produisent
// Follower Summon, Emotion ou Recall Slaves, qui restent donc muettes au-dessus
// d'un monstre. Un paquet « jetable » et ces deux compétences-ci se taisent
// aussi, quel que soit le lanceur.
inline constexpr std::array<std::uint16_t, 2> kSkillsWithoutCastBanner = {57, 62};

[[nodiscard]] constexpr bool CastAnnouncedBy(std::uint16_t skill_id, std::uint32_t shown_job,
                                             bool disposable) {
  return !disposable && GroundSkillAnnouncedBy(shown_job) &&
         std::ranges::find(kSkillsWithoutCastBanner, skill_id) == kSkillsWithoutCastBanner.end();
}

class SkillTree;

// LE TEXTE D'UNE BANNIÈRE, tel que le client le compose : le
// libellé affiché de la compétence, puis le suffixe.
//
// Le numéro s'ajoute entre parenthèses pour le STAFF — un maître de jeu qui
// essaie les compétences a besoin de l'identifiant pour les nommer au serveur,
// et le lire dans le journal oblige à quitter le jeu des yeux.
//
// ⚠ SANS TABLES DU CLIENT, ON DIT LE NUMÉRO plutôt que de se taire : une
// bannière vide ressemblerait à une compétence qui n'annonce rien, et les deux
// se corrigent à des endroits opposés.
[[nodiscard]] std::string SkillBannerText(const SkillTree* skills, std::uint16_t skill_id,
                                          bool staff);

}  // namespace ro::sim
