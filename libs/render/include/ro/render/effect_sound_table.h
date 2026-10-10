#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/render/skill_sound_table.h"

namespace ro::render {

// LE BRUIT D'UN EFFET.
//
// ⚠ Le bruit d'une compétence n'est pas rangé sous la compétence. Le client
// l'attache à l'EFFET, dans la fonction de mise à jour propre à chaque
// identifiant d'effet. Deux compétences qui partagent un
// effet partagent donc son bruit, et une compétence sans effet n'en fait aucun.
//
// C'est ce qui explique qu'une table rangée par COMPÉTENCE reste muette sur les
// compétences classiques : leur bruit n'y a jamais été.

// Une entrée de la table des sons d'effet.
struct EffectSoundEntry {
  std::uint16_t effect_id = 0;
  // Le chemin du fichier, relatif à kSoundFolder, écrit comme dans l'archive.
  std::string_view sound;
  // À QUEL TIC DE L'INSTANCE LE CLIENT LE JOUE, zéro pour sa naissance.
  //
  // 🔴 UN BRUIT SUR CINQ N'EST PAS JOUÉ AU DÉPART : le bâtisseur le garde
  // derrière `âge == N` — trente-sept pour l'onde de Bash, soixante pour
  // Impositio Manus, douze pour la flèche de glace — et un effet peut en jouer
  // plusieurs à des tics différents, Lex Divina cinq fois. Sans ce tic, tout
  // sonnerait à la naissance.
  std::uint16_t delay_ticks = 0;
  // TOUS LES COMBIEN DE TICS LE CLIENT LE REJOUE, zéro pour un bruit unique.
  //
  // 🔴 UN BRUIT PEUT ÊTRE UN BOURDON, et `delay_ticks` seul ne sait pas le
  // dire. Le portail prêt joue `EF_ReadyPortal.wav` sous `!(âge % 14)` : il
  // carillonne toutes les deux cent vingt-quatre millisecondes tant qu'il tient
  // sa case. Sans la période, il ne sonnerait qu'une fois par relance d'unité —
  // treize fois trop lentement.
  //
  // ⚠ ET LA PÉRIODE NE DIT PAS À ELLE SEULE QU'ON DOIT REJOUER : un bruit dont
  // le SEMIS se charge déjà — Soul Strike, une boule un coup — porte sa période
  // sans que le lecteur ne la suive. Voir `EffectSoundPlaysAtSowing`.
  std::uint16_t period_ticks = 0;
  // LE NOMBRE DE FICHIERS ENTRE LESQUELS LE CLIENT TIRE, zéro pour un nom fixe.
  // Le nom porte alors `%d` à la place du numéro, que chaque appel tire à neuf :
  // les quatre bruits des pétales ne sonnent pas forcément pareil. Voir
  // `EffectSoundFile`.
  std::uint8_t variants = 0;
  // 🔴 SUR L'AUDITEUR ET NON SUR L'EFFET : le client passe la place du bruit
  // RELATIVE à l'auditeur, et certains bâtisseurs y passent trois zéros. Le
  // bruit sonne alors plein et au milieu, où que soit l'effet — la boule de feu
  // de l'effet 891 est de ceux-là, celle d'EF_FIREBALL non.
  bool at_listener = false;
  // LA HAUTEUR AU-DESSUS DE L'AUDITEUR, en unités du monde. Le client la compte
  // dans la distance, qu'il pose le bruit sur l'auditeur ou sur l'effet : au-delà
  // de la proximité, le bruit s'atténue comme s'il venait d'aussi loin.
  std::uint8_t listener_height = 0;
  // ⚠ EN QUEUE, les deux : la table les pose par liste positionnelle.
  //
  // LE BRUIT NE SONNE QU'UNE FOIS SUR AUTANT, tiré à chaque lancement ; zéro
  // ou un pour toujours.
  std::uint8_t one_in = 0;
  // LE BRUIT NE SONNE QUE SUR UN PORTEUR ORDINAIRE : un acteur présent, sans
  // état du corps (pétrifié, gelé…) ni option (caché, dissimulé…).
  bool needs_plain_bearer = false;
};

// Le fichier qu'un bruit joue : son nom tel quel, ou, pour un nom à variantes,
// le numéro tiré de `draw` — de un au nombre de variantes. Deux tirages
// voisins, un par bruit d'un même effet, rendent des numéros indépendants.
[[nodiscard]] std::string EffectSoundFile(const EffectSoundEntry& entry, std::uint32_t draw);

// Le fichier de la variante `variant`, comptée à partir de un ; le nom tel quel
// pour un nom fixe.
[[nodiscard]] std::string EffectSoundVariantFile(const EffectSoundEntry& entry,
                                                 std::uint32_t variant);

// La table entière, triée par identifiant d'effet.
[[nodiscard]] std::span<const EffectSoundEntry> EffectSoundTable();

// Le son d'un effet, ou une chaîne VIDE pour un effet muet. Le PREMIER, quand
// l'effet en joue plusieurs : voir `EffectSounds`.
[[nodiscard]] std::string_view EffectSound(std::uint32_t effect_id);

// TOUS LES SONS D'UN EFFET, dans l'ordre où le client les joue — vide pour un
// effet muet.
//
// 🔴 UN BÂTISSEUR PEUT EN JOUER DEUX D'UN COUP. L'attaque empoisonnée joue deux
// sons de suite à son tic zéro, `assasin_enchantpoison.wav` puis
// `EF_PoisonAttack.wav`, sans branche entre les deux. La table porte alors deux
// rangées du même identifiant, à la suite ; n'en jouer qu'une laisserait un
// bruit de côté.
[[nodiscard]] std::span<const EffectSoundEntry> EffectSounds(std::uint32_t effect_id);

// LES BRUITS D'UNE COMPÉTENCE, chacun avec son tic.
//
// 🔴 CE N'EST PAS `SkillSound`. Cette table-là ne connaît que la poignée de
// compétences dont le client déclenche le son dans son propre cas de switch ;
// pour toutes les autres — la quasi-totalité —, le bruit est celui de l'EFFET
// qu'elles montrent. Les deux sources se consultent dans cet ordre, et la
// famille procédurale, qui n'a pas d'effet à fichier, fournit la troisième.
//
// ⚠ ET LE TIC FAIT PARTIE DU SON : un effet en joue plusieurs à des instants
// différents — Provoke deux fois, aux tics 5 et 30 —, si bien qu'une liste de
// NOMS ne suffit pas à le rejouer. Voir `EffectSoundEntry::delay_ticks`.
//
// ⚠ UN EFFET DONT LA TROUPE SONNE NE REND RIEN ICI : son bâtisseur joue le
// bruit dans la garde du SEMIS, une fois par pièce, et l'ajouter au départ
// ferait entendre un coup de trop — voir `EffectSoundPlaysAtSowing`.
[[nodiscard]] std::vector<EffectSoundEntry> SkillSounds(std::uint32_t skill_id);

// VRAI QUAND CE CHEMIN FAIT NAÎTRE UN EFFET pour la compétence : le coup
// (`on_hit`) ou l'annonce sans dégâts — sa table d'effets, ou une étape d'effet
// de sa suite à ce moment.
[[nodiscard]] bool SkillPathShowsEffect(std::uint16_t skill_id, bool on_hit);

// LES BRUITS D'UNE COMPÉTENCE SUR UN CHEMIN : ceux de `SkillSounds`, mais
// SANS REPLI sur l'effet quand ce chemin n'en fait naître aucun.
//
// 🔴 LE BRUIT D'UN EFFET VIENT DE SON INSTANCE : un chemin qui ne sème rien ne
// sonne pas. Hiding n'a d'effet que sur le chemin des dégâts, qu'il ne prend
// jamais ; son annonce, à l'entrée comme à la sortie, est muette.
// Resurrection n'a d'effet qu'à l'annonce ; le coup qu'elle porte à un
// mort-vivant ne fait entendre que l'impact.
//
// Le son propre de la compétence, lui, reste : il ne passe par aucun effet.
[[nodiscard]] std::vector<EffectSoundEntry> SkillSoundsOnPath(std::uint16_t skill_id, bool on_hit);

}  // namespace ro::render
