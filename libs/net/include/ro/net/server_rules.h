#pragma once

#include <cstdint>

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LES RÈGLES DU SERVEUR : `ZC_MOONLIGHT_SERVER_RULES 0x0F38`.
//
//     0f38 <len>.W { <clé>.W <valeur>.L }*
//
// Des paires clé / valeur, rangées dans `sim::SessionSettings`. Le serveur les
// envoie dès que le client annonce savoir les lire (`kUiCapServerRules`), donc à
// chaque connexion à un serveur de carte, puis de nouveau à tous les clients qui
// l'ont annoncé quand le staff recharge sa configuration de combat
// (`@reloadbattleconf`).
//
// ⚠ UNE CLÉ INCONNUE EST IGNORÉE, et c'est ce qui laisse le serveur en ajouter
// sans casser ce client. Une longueur qui ne tombe pas sur un nombre entier de
// paires, ou qui contredit la trame, rend la trame indécodable : rien n'en est
// retenu. Une valeur hors de son domaine (un rayon nul, un niveau nul, un délai
// ou un seuil négatif, une taxe hors de 0 à 10 000 dix-millièmes) est ignorée,
// et le défaut ou la valeur précédente reste.
enum class ServerRuleKey : std::uint16_t {
  kViewCells = 1,
  kHomunculusMaxLevel = 2,
  kHomunculusSMaxLevel = 3,
  kHomunculusTeleportHoldMs = 4,
  kVendingTaxPermyriad = 5,
  kVendingTaxMinZeny = 6,
  // La portée du JEU, `area_size` : la clé 1 est celle de la VUE de ce joueur,
  // plus grande pour moonclient ; celle-ci borne ce que le serveur tolère hors
  // de la vue (laisse d'un compagnon, distance d'un PNJ ou d'une échoppe).
  kGameAreaCells = 7,
  // Le plus long chemin que le serveur accepte pour une marche, en pas.
  kMaxWalkPathSteps = 8,
  // LE PLAFOND DE LA BANQUE, un `int64` qu'une valeur de quatre octets ne porte
  // pas : deux clés, chacune les trente-deux bits de sa moitié en motif brut.
  // ⚠ ELLES NE VALENT QU'ENSEMBLE, DANS LA MÊME TRAME : une moitié seule, ou un
  // plafond qui ne serait pas strictement positif, est ignoré.
  kBankZenyMaxLow = 9,
  kBankZenyMaxHigh = 10,
  // LES COULEURS DU PERSONNAGE. La cadence des calques est la preuve que le
  // serveur sait recevoir leurs teintures.
  kStyleLayersCadenceMs = 11,
  kStyleBodyVariantsMax = 12,
  kStyleLayersMax = 13,
  kHairStyleMin = 14,
  kHairStyleMax = 15,
  kHairColorMin = 16,
  kHairColorMax = 17,
  kClothColorMin = 18,
  kClothColorMax = 19,
  kStyleLayersBytesPerMinute = 20,
  kStyleAtelierCells = 21,
  // LES TENUES. La cadence est la preuve que le serveur sait les recevoir.
  kStyleOutfitCadenceMs = 22,
  kStyleWardrobeSlots = 23,
  // LE PROFIL MIXTE : les retouches qu'un calque porte au plus. Sa présence est
  // la preuve que le serveur accepte le reste et les retouches d'une entrée.
  kStyleLayerRetouchesMost = 24,
  // LE DERNIER NUMÉRO DE CALQUE que le serveur accepte. Sa présence, à 6 au
  // moins, est la preuve qu'il range la peau du corps.
  kStyleLayerLast = 25,
  // LES STYLES HORS DE VUE. La cadence est la preuve que le serveur sert leur
  // demande.
  kStyleAbsentCadenceMs = 26,
  kStyleAbsentIdsMost = 27,
};

[[nodiscard]] FrameOutcome ApplyServerRulesFrame(const Frame& frame, sim::World& world);

}  // namespace ro::net
