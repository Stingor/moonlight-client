#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// LES EFFETS DE CHAPEAU, et ils vivent dans les DONNÉES de l'installation.
//
// Un chapeau, une monture, une aura d'événement : le serveur annonce un numéro
// d'effet de chapeau, et le client va chercher ce qu'il montre dans deux tables
// Lua de l'installation. C'est la seconde famille à vivre dans les données, après
// les effets NOMMÉS — voir ADR 0027.
//
// ⚠ ELLE NE DEMANDE AUCUNE MACHINERIE DE DESSIN NOUVELLE. Un exemple isolé
// (`HAT_EF_Digital_Space`, identifiant 1240, qui ne passe ni par un `.str` ni
// par un émetteur) le laisserait croire, mais la table entière dit autre
// chose : sur 259
// fiches, **149 nomment un `.str`** et **111 un identifiant d'effet** — les deux
// familles que nous peignons déjà. Une seule fiche porte les deux.
//
// CE QUE CETTE INSTALLATION MONTRERAIT, au 2026-09-14 : 138 des 149 fichiers y
// sont (11 manquent, et le client d'origine ne montre rien non plus pour
// ceux-là),
// et 23 des 101 identifiants distincts sont de ceux que le juge voit. Les 70
// autres sont hors du dénominateur du juge — ni cas du répartiteur, ni fichier
// — et ce qui les résout reste à lire.

// Les deux fichiers, dans l'ordre où ils se lisent : le second nomme ses clés
// par le premier, et se lit seul rendrait des clés nulles.
inline constexpr std::string_view kHatEffectIdsPath =
    "data\\luafiles514\\lua files\\hateffectinfo\\hateffectids.lub";
inline constexpr std::string_view kHatEffectInfoPath =
    "data\\luafiles514\\lua files\\hateffectinfo\\hateffectinfo.lub";
// La globale sous laquelle la seconde range ses fiches, indexées par le NUMÉRO
// d'effet de chapeau — celui que `HatEFID` nomme et que le serveur envoie.
inline constexpr std::string_view kHatEffectTableGlobal = "hatEffectTable";

// LE TROISIÈME RÉSOLVEUR, et il vit dans les données.
//
// Le client ne résout un identifiant d'effet par sa table de fichiers que SOUS
// ce nombre. Au-delà, il ne compare rien : il appelle le global Lua
// `GetHatEfResName(identifiant - 2442)` et laisse la table de données répondre.
// `ro::render::EffectFile` ne couvre donc que ce qui est en deçà.
//
// Chaque fiche est d'ailleurs inscrite sous la clé ORDINAL + 2442, si bien que
// les deux bouts du calcul se répondent.
inline constexpr std::uint32_t kHatEffectIdBase = 2442;

// Ce qu'une fiche décrit.
struct HatEffect {
  std::uint16_t hat_effect_id = 0;

  // resourceFileName, relatif au dossier des effets. Vide quand la fiche
  // désigne son effet par un identifiant.
  std::string file;
  // hatEffectID : un identifiant d'effet ordinaire, celui des deux tables de
  // fichiers du client et du répartiteur procédural, ou une classe du
  // gestionnaire d'effets. ZÉRO quand la fiche ne nomme qu'un fichier — aucun
  // effet ne porte le numéro zéro ; une fiche qui porte les deux dit par
  // `effect_pair` si l'identifiant se joue aussi.
  std::uint32_t effect_id = 0;

  // hatEffectPos : ce qui s'ajoute à la HAUTEUR DE L'ANCRE, en unités du monde,
  // vers le haut de l'écran. Quatorze valeurs sur les 259 fiches, toutes
  // entières, de -17 à 4 ; le client la lit en entier. Voir `hat_effect_anchor.h`.
  float offset = 0.0F;
  // hatEffectPosX : le décalage vers la DROITE DE L'ÉCRAN, en unités du monde,
  // lu en réel. Huit valeurs, de -1 à 6.
  float offset_x = 0.0F;

  // Les drapeaux, absents des fiches qui ne les portent pas. Le nombre de
  // fiches qui les portent, sur 259 : 148, 148, 61, 45, 2.
  bool adjust_position_when_shrunk = false;
  bool adjust_size_when_shrunk = false;
  // isRenderBeforeCharacter : le `.str` passe dans la file du sol, AVANT les
  // corps. Faux sur une fiche sans `hatEffectPos`, quoi qu'elle écrive : le
  // client ne consulte le drapeau que si le décalage est nommé.
  bool render_before_character = false;
  bool ignore_riding = false;
  bool attached_head = false;
  // isEffectPair : la fiche montre SES DEUX MOITIÉS, le fichier ET l'effet de
  // son identifiant. Sans lui, le fichier nommé passe seul.
  bool effect_pair = false;
};

// La table entière, triée par numéro d'effet de chapeau.
class HatEffectTable {
 public:
  // Lit les deux fichiers dans l'ordre, comme le client. L'échec dit lequel.
  [[nodiscard]] static core::Result<HatEffectTable> Load(vfs::Vfs& vfs);

  // La MEME lecture, à partir de globales déjà lues.
  //
  // Séparée de `Load` parce que c'est la seule moitié qui se met sous test : un
  // essai n'a pas d'installation, et aucun asset du jeu n'est versé au dépôt.
  // Il lui suffit de trois lignes de Lua en clair, que le lecteur accepte au
  // même titre que le bytecode.
  [[nodiscard]] static core::Result<HatEffectTable> Parse(const formats::LuaTable& globals);

  // Nul si ce numéro n'a pas de fiche. C'est un cas NORMAL : `HatEFID` nomme
  // 292 effets et la table n'en décrit que 259.
  [[nodiscard]] const HatEffect* Find(std::uint32_t hat_effect_id) const;

  // LA FICHE QUE LE CLIENT MONTRE pour un identifiant d'effet CONCRET, en
  // refaisant son calcul : la fiche d'ordinal `effect_id - 2442`. Nul en deçà
  // de ce seuil, où la table de fichiers du client répond seule.
  //
  // ⚠ CE N'EST PAS LA FICHE QUI PORTE CET IDENTIFIANT, et c'est un DÉFAUT DU
  // CLIENT que nous reproduisons délibérément. Il lance l'effet sous
  // l'identifiant CONCRET que la fiche porte — `GetHatEffectID(ordinal)` — et non la clé unifiée
  // `ordinal + 2442`. Les deux plages se recouvrant, la recherche aboutit quand
  // même, sur la MAUVAISE fiche.
  //
  // Une seule fiche de cette installation y tombe, et elle suffit à le prouver :
  // l'ordinal 277 (`HAT_EF_C_2025RosFesta`) porte l'identifiant 2443, donc
  // 2443 - 2442 = 1, et ce que le joueur voit est le `sakura.str` de l'ordinal
  // 1. Montrer « ce qu'il faudrait » afficherait autre chose que tous les autres
  // écrans. La PARITÉ est le but ; le défaut se reproduit et se dit.
  [[nodiscard]] const HatEffect* FindByConcreteId(std::uint32_t effect_id) const;

  [[nodiscard]] std::span<const HatEffect> entries() const noexcept { return entries_; }

 private:
  std::vector<HatEffect> entries_;
};

}  // namespace ro::render
