#pragma once

#include <span>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA NAVIGATION LATÉRALE : la liste des sections à gauche, le contenu de celle
// qu'on a choisie à droite.
//
// ⚠ C'EST LA FORME QU'UN PANNEAU DE RÉGLAGES PREND QUAND IL GRANDIT. Des onglets
// tiennent jusqu'à cinq ou six ; au-delà ils se chevauchent, défilent, et l'on
// ne voit plus ce qui existe. Une liste verticale montre tout, se cherche à l'œil
// et accepte d'être longue — c'est ce que fait le panneau qu'on porte, qui en a
// vingt-sept.
//
// S'emploie comme un Begin/End, même règle que partout : `EndNavPanel` s'appelle
// TOUJOURS, même si l'appelant ne dessine rien entre les deux.
//
//   BeginNavPanel("reglages", labels, &section, art);
//   DessinerLaSection(section);
//   EndNavPanel(art);
//
// ⚠ ELLE NE POSSÈDE PAS LA SECTION COURANTE : c'est un état de l'appelant, et
// non une statique locale. Sans cela, rien d'extérieur — une commande, un
// raccourci — ne pourrait ouvrir le panneau SUR une section.

// UNE ENTRÉE DE LA LISTE, et la FAMILLE qu'elle ouvre s'il y a lieu.
//
// 🔴 LE TITRE DE FAMILLE EST PORTÉ PAR SON PREMIER MEMBRE, et non par une entrée
// à lui : une famille qui occuperait un rang décalerait tout ce qui suit.
//
// 🔴 ET L'ENTRÉE DIT LA SECTION QU'ELLE DÉSIGNE, plutôt que son rang dans la
// liste. La liste se trie et se filtre ; la section, elle, est ce que retiennent
// l'état de l'appelant, les liens et les puces des fenêtres. Un rang de liste
// changerait de sens à chaque lettre tapée.
struct NavEntry {
  // ⚠ DÉJÀ TRADUIT par l'appelant, comme partout dans cette couche.
  const char* label = "";
  // Le titre de la famille qui s'ouvre AVANT cette entrée, ou nul.
  const char* group = nullptr;
  // La section que l'entrée désigne ; moins un, son rang dans la liste.
  int target = -1;
};

// LE CHAMP DE FILTRE posé au-dessus de la liste, quand l'appelant en offre un.
//
// ⚠ LA NAV NE FILTRE RIEN ELLE-MÊME : elle montre le champ et les entrées
// qu'on lui donne. C'est l'appelant qui relit le texte et ne passe que les
// entrées qui y répondent.
struct NavFilter {
  // Le tampon du texte tapé, terminé par un zéro.
  std::span<char> typed;
  // Ce que montre le champ vide. ⚠ DÉJÀ TRADUIT.
  const char* hint = "";
  // Le compte des résultats, posé sous le champ, ou nul quand rien n'est
  // cherché. ⚠ DÉJÀ TRADUIT ET FORMATÉ.
  const char* found = nullptr;
  // Le champ prend le clavier à cette image.
  bool focus = false;
};

// Ouvre la nav. `*current` est l'indice de la section montrée, borné à la liste.
//
// `linked`, non nul, OFFRE LE LIEN DANS LE CHAT : un Maj + clic sur une entrée y
// écrit son rang, et le survol l'annonce — voir `chat_link_offer.h`. Nul — à
// l'écran de connexion, sans chat —, le Maj + clic reste un clic ordinaire.
//
// 🔴 OFFERT, MAJ DÉSARME LA SÉLECTION : le geste pose le lien SANS changer de
// section sous les yeux de celui qui explique.
//
// `filter`, non nul, pose son champ au-dessus de la liste, à sa largeur.
void BeginNavPanel(const char* id, std::span<const NavEntry> entries, int* current,
                   const SkinArt& art, int* linked = nullptr, NavFilter* filter = nullptr);

// Referme le cadre du contenu, après y avoir peint son ascenseur.
void EndNavPanel(const SkinArt& art);

// La largeur que prend la liste : celle de son plus long libellé, bornée pour
// qu'elle ne mange pas le contenu sur une fenêtre étroite.
//
// ⚠ MESURÉE À CHAQUE IMAGE, ET SANS CACHE. Un cache paraît naturel — vingt-sept
// `CalcTextSize` par image pour un résultat qui ne bouge presque jamais — mais
// sa clé doit porter la POLICE **et la LANGUE**, et c'est l'oubli de la seconde
// qui a coûté au panneau qu'on porte : en passant au catalogue espagnol, plus
// verbeux, la liste gardait la mesure française et « Seguimiento de misiones »
// sortait coupé net. Rien ne le signalait, et changer de police « réparait » le
// symptôme le temps d'une remesure. Mesurer quelques libellés courts ne coûte
// rien au regard de ce qu'ImGui fait par widget ; la panne, elle, disparaît.
[[nodiscard]] [[nodiscard]] float NavPanelWidth(std::span<const NavEntry> entries);

}  // namespace ro::ui
