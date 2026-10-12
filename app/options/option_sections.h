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
// ⚠ L'ORDRE EST CELUI DE LA LECTURE, familles comprises : `kDisplay` à
// `kCapture` forment « Core », le reste « UI ». Une section déplacée hors de
// son bloc emporterait le titre de famille avec elle.
enum class OptionSection : std::size_t {
  kDisplay = 0,
  // CE QUE LE MOTEUR FAIT DE L'IMAGE. ⚠ CONTRE « DISPLAY », QUI RÈGLE LA
  // FENÊTRE : l'une dit où l'image s'affiche, l'autre comment elle se peint.
  kGraphics,
  kSound,
  kGameplay,
  // L'ÉCRAN DE VEILLE. ⚠ DANS « CORE » ET CONTRE « GAMEPLAY » : il ne règle
  // aucune fenêtre — il prend la CAMÉRA, éteint tout le reste et teinte
  // l'image. Il vit sous la même navigation que les réglages de jeu.
  kAfkScreen,
  // LES DÉGÂTS FLOTTANTS. ⚠ DANS « CORE », CONTRE « GAMEPLAY » : les nombres
  // sont posés SUR LE MONDE par le renderer, pas par une fenêtre.
  kDamage,
  // LES ENTITÉS : LEURS BARRES DE VIE ET DE MAGIE, LEURS NOMS ET LEURS BARRES
  // DE CAST, dans une seule page — ils se posent sur la même entité, et on les
  // règle en la regardant.
  //
  // LES BARRES DE VIE ET DE MAGIE DES ENTITÉS. ⚠ DANS « CORE », CONTRE LES
  // DÉGÂTS ET POUR LA MÊME RAISON : elles sont posées SUR LE MONDE, au-dessus
  // des têtes, et non dans une fenêtre qu'on range. Les deux sections se règlent
  // d'ailleurs l'une en regardant l'autre — on lit une barre et un nombre au
  // même instant, sur la même cible.
  kEntityBars,
  // LA CAPTURE D'ÉCRAN. ⚠ DANS LA FAMILLE « CORE » ET NON DANS L'INTERFACE :
  // c'est le PROGRAMME qui relit son image, et ce que la capture montre est
  // tout ce que l'écran porte — le monde comme les fenêtres.
  kCapture,
  kInterface,
  // LE SKIN : le dossier porté et son habillage. ⚠ SOUS L'INTERFACE, en tête :
  // il habille toutes les fenêtres qui suivent.
  kSkin,
  // LA MINIMAP. ⚠ SOUS L'INTERFACE, en tête de ses fenêtres : c'est le premier
  // repère qu'on pose à l'écran. Ses réglages vivent dans le profil de
  // disposition, et la partie en confie le dessin — voir `MinimapPanelOptions`.
  kMinimap,
  // LES ICÔNES DU MENU. ⚠ CONTRE L'INTERFACE : c'est le premier HUD qu'on pose,
  // et la porte de toutes les autres fenêtres.
  kMenuIcons,
  kShortcutBars,
  // LES SIX JAUGES LIBRES, contre les barres de raccourcis :
  // c'est le même genre de surface posée sur le monde.
  kStatBars,
  // ⚠ CONTRE LES BARRES DE RACCOURCIS, et non ailleurs : c'est l'autre rangée
  // posée sur le monde, et les deux se règlent l'une en regardant l'autre.
  kStatuses,
  // LE COMPTEUR DE DÉGÂTS. ⚠ DANS L'INTERFACE ET NON CONTRE « DÉGÂTS » : il ne
  // peint rien sur le monde, c'est une fenêtre qu'on range — et l'instrument du
  // combat, avant la fenêtre de cible.
  kDamageMeter,
  // LE MODE CIBLAGE ET SON HUD, après les autres surfaces posées sur le monde.
  kTarget,
  // LA FICHE D'UN MONSTRE. ⚠ CONTRE LA FENÊTRE DE CIBLE : c'est le monstre
  // qu'on vise qu'on vient lire.
  kMonsterSheet,
  // LE CARNET DE CHASSE MVP. ⚠ CONTRE LA FICHE DE MONSTRE : c'est d'un monstre
  // qu'il tient le retour.
  kMvpTracker,
  // LE MENU DU CLIC DROIT SUR UNE ENTITÉ. ⚠ CONTRE LA FENÊTRE DE CIBLE : le
  // clic droit d'un de ses cadres ouvre ce menu.
  kContextMenu,
  // LA GRILLE DE GROUPE ET LA FENÊTRE GROUPE / AMIS. ⚠ CONTRE LE MENU
  // CONTEXTUEL : le clic droit d'une tuile ou d'une ligne l'ouvre.
  kPartyGrid,
  kPartyFriend,
  kChat,
  kBubbles,
  // LE BANDEAU « objet obtenu ». ⚠ CONTRE L'INVENTAIRE : c'est en regardant
  // son sac se remplir qu'on le règle, et ce qu'il annonce y atterrit.
  kItemObtain,
  kInventory,
  kCart,
  kStorage,
  // L'ATLAS DES RECETTES. ⚠ APRÈS LES TROIS CONTENANTS : c'est de ce qu'on y
  // range qu'on vient lui demander « et ça, ça sert à quoi ? ».
  kCraftAtlas,
  // LA FABRICATION ET LE REFINE D'ARME. ⚠ CONTRE L'ATLAS : ce sont les deux
  // ateliers dont il décrit les recettes et les chances.
  kMakeItem,
  kWeaponRefine,
  // L'ALBUM DE CARTES. ⚠ CONTRE LE COFFRE : c'est l'autre conteneur que le sac
  // remplit.
  kCardAlbum,
  // LES DESCRIPTIONS D'ITEM ET DE SKILL. ⚠ APRÈS LES CONTENANTS ET L'ALBUM :
  // c'est de ce qu'on y range qu'on ouvre la description.
  kDescriptions,
  kCharacter,
  // LA FICHE DU PET. ⚠ CONTRE LE PERSONNAGE : ses deux autres compagnons,
  // l'homoncule et le mercenaire, sont des onglets de sa fiche.
  kPet,
  // LE JOURNAL DES QUÊTES ET SON PANNEAU DE SUIVI. ⚠ EN FIN D'INTERFACE : le
  // panneau sert aussi les succès, et n'appartient à aucune fenêtre d'au-dessus.
  // Ses réglages vivent dans `suivi.yml` — voir `QuestTrackerOptions`.
  kQuestTracker,
  // LES RÉGLAGES QUE LE SERVEUR TIENT POUR LE PERSONNAGE — `@showexp`, `@noks`,
  // `@autoloot` et les leurs. ⚠ UNE FAMILLE À PART, « Server » : ils ne règlent
  // ni le programme ni une fenêtre, et vivent dans les registres du personnage.
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
