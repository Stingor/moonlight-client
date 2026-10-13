#pragma once

#include <cstddef>

namespace moonclient {

// LES SECTIONS DU PANNEAU DE RÉGLAGES, et rien d'autre.
//
// 🔴 À PART DU PANNEAU, ET C'EST L'ÉDITEUR DE LIENS QUI LE DIT. Dans
// `game_options.h`, qui traîne derrière lui l'audio, le monde et les fenêtres,
// cette énumération empêcherait la table des destinations d'un lien de
// réglage — qui ne veut qu'un rang de section — de s'essayer sans monter tout
// le jeu. Ici, elle ne coûte rien à personne.

// LES SECTIONS DU PANNEAU, PAR LEUR RANG. ⚠ SANS COMPTE DANS CE TITRE : un
// nombre écrit ici périme à la première section ajoutée, et personne ne le voit.
//
// 🔴 UNE SEULE SOURCE, et c'est tout l'objet de cette énumération. La
// composition les range dans cet ordre, et la puce d'une fenêtre y mène par ce
// nom : deux listes finiraient par diverger d'un cran, et la puce ouvrirait la
// section d'à côté — celle qui ne parle pas de la fenêtre d'où l'on vient.
//
// ⚠ L'ORDRE DIT LES FAMILLES, PAS L'ORDRE DE LECTURE : `kDisplay` à `kCapture`
// forment « Core », `kInterface` à `kQuestTracker` « UI », `kCommands`
// « Server ». La navigation range chaque famille par ordre alphabétique de ses
// titres, dans la langue du joueur — sauf `kInterface` et `kSkin`, posées en
// tête de « UI » quelle que soit la langue. Une section sortie de son bloc
// changerait de famille.
enum class OptionSection : std::size_t {
  // ── CORE : ce que le programme fait ──
  kDisplay = 0,
  // CE QUE LE MOTEUR FAIT DE L'IMAGE. ⚠ À PART DE « DISPLAY », QUI RÈGLE LA
  // FENÊTRE : l'une dit où l'image s'affiche, l'autre comment elle se peint.
  kGraphics,
  kSound,
  kGameplay,
  // L'ÉCRAN DE VEILLE. ⚠ DANS « CORE » : il ne règle aucune fenêtre — il prend
  // la CAMÉRA, éteint tout le reste et teinte l'image.
  kAfkScreen,
  // LES DÉGÂTS FLOTTANTS. ⚠ DANS « CORE » : les nombres sont posés SUR LE MONDE
  // par le renderer, pas par une fenêtre.
  kDamage,
  // LES ENTITÉS : LEURS BARRES DE VIE ET DE MAGIE, LEURS NOMS ET LEURS BARRES
  // DE CAST, dans une seule page — ils se posent sur la même entité, et on les
  // règle en la regardant. ⚠ DANS « CORE », POUR LA RAISON DES DÉGÂTS : elles
  // sont posées SUR LE MONDE, au-dessus des têtes, et non dans une fenêtre
  // qu'on range.
  kEntityBars,
  // LA CAPTURE D'ÉCRAN. ⚠ DANS « CORE » ET NON DANS L'INTERFACE : c'est le
  // PROGRAMME qui relit son image, et ce que la capture montre est tout ce que
  // l'écran porte — le monde comme les fenêtres.
  kCapture,
  // ── UI : ce que chaque fenêtre fait ──
  //
  // ⚠ « INTERFACE » PORTE AUSSI LES FENÊTRES QUI N'ONT QUE DEUX OU TROIS
  // RÉGLAGES — la fiche du personnage, celle du pet —, chacune sous son
  // intertitre. Une page pour une case se cherche plus longtemps qu'elle ne se
  // lit.
  kInterface,
  // LE SKIN : le dossier porté et son habillage.
  kSkin,
  // LA MINIMAP. Ses réglages vivent dans le profil de disposition, et la partie
  // en confie le dessin — voir `MinimapPanelOptions`.
  kMinimap,
  kMenuIcons,
  kShortcutBars,
  // LES SIX JAUGES LIBRES.
  kStatBars,
  kStatuses,
  // LE COMPTEUR DE DÉGÂTS. ⚠ DANS L'INTERFACE ET NON AVEC « DÉGÂTS » : il ne
  // peint rien sur le monde, c'est une fenêtre qu'on range.
  kDamageMeter,
  // LE MODE CIBLAGE ET SON HUD.
  kTarget,
  kMonsterSheet,
  kMvpTracker,
  // LE MENU DU CLIC DROIT SUR UNE ENTITÉ.
  kContextMenu,
  // LA GRILLE DE GROUPE ET LA FENÊTRE GROUPE / AMIS.
  kPartyGrid,
  kPartyFriend,
  kChat,
  // LES BULLES DE CHAT, au-dessus des têtes.
  kBubbles,
  // LES BULLES D'ANNONCE, en haut de l'écran. ⚠ À PART DES BULLES DE CHAT : ni
  // la même place, ni la même vie, ni les mêmes réglages.
  kBroadcastBalloons,
  // LE BANDEAU « objet obtenu ».
  kItemObtain,
  kInventory,
  kCart,
  kStorage,
  // L'ATLAS DES RECETTES.
  kCraftAtlas,
  // LA FABRICATION ET LE REFINE D'ARME.
  kMakeItem,
  kWeaponRefine,
  kCardAlbum,
  // LES DESCRIPTIONS D'ITEM ET DE SKILL.
  kDescriptions,
  // LE JOURNAL DES QUÊTES ET SON PANNEAU DE SUIVI. Ses réglages vivent dans
  // `suivi.yml` — voir `QuestTrackerOptions`.
  kQuestTracker,
  // ── SERVER ──
  //
  // LES RÉGLAGES QUE LE SERVEUR TIENT POUR LE PERSONNAGE — `@showexp`, `@noks`,
  // `@autoloot` et les leurs. ⚠ UNE FAMILLE À PART : ils ne règlent ni le
  // programme ni une fenêtre, et vivent dans les registres du personnage.
  // C'est l'en-tête « Commands Settings ».
  kCommands,
  kCount,
};

[[nodiscard]] constexpr std::size_t Rank(OptionSection section) noexcept {
  return static_cast<std::size_t>(section);
}

// La page d'une section, telle que `SettingsFile::PageScope` la range.
[[nodiscard]] constexpr int SettingsPage(OptionSection section) noexcept {
  return static_cast<int>(section);
}

// LA PAGE DES SONS DES MONSTRES ET DES PETS : montrée sous « Sound », mais
// rangée à part dans le fichier. ⚠ RÉTABLIR « SOUND » EN JEU LA REPREND ; les
// réglages du login, qui ne la montrent pas, la laissent intacte.
inline constexpr int kSpriteSoundsSettingsPage = SettingsPage(OptionSection::kCount);

}  // namespace moonclient
