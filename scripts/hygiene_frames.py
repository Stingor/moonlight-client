"""Un cas de famille réseau que l'aiguillage n'envoie jamais.

`libs/net/src/world_update.cpp` aiguille chaque trame : certaines familles sont
consultées par la chaîne de `world_relay.cpp`, d'autres ne sont atteintes que
par une LISTE de `case` écrite dans l'aiguillage. Pour ces dernières, un `case`
ajouté dans la famille sans être ajouté à la liste compile, passe l'essai qui
appelle la famille directement — et ne sert jamais : ZC_COMBODELAY est resté
ainsi sans effet.
"""

from __future__ import annotations

import re
from pathlib import Path

CASE = re.compile(r"case proto::(\w+)::kOpcode")
LISTED_CALL = re.compile(r"return (Apply\w+Frame)\(frame")
RELAYED_CALL = re.compile(r"(Apply\w+Frame)\(frame")
FAMILY_END = "\n}\n"


def unrouted_family_cases(dispatch: str, relay: str,
                          families: dict[str, str]) -> list[str]:
    """Les `case` d'une famille à liste que la liste de l'aiguillage omet.

    `families` associe un nom de fichier à son texte.
    """
    routed = set(CASE.findall(dispatch))
    listed_only = set(LISTED_CALL.findall(dispatch)) - set(RELAYED_CALL.findall(relay))
    problems = []
    for name, text in sorted(families.items()):
        for function in sorted(listed_only):
            start = text.find(f"FrameOutcome {function}(")
            if start < 0:
                continue
            end = text.find(FAMILY_END, start)
            for opcode in sorted(set(CASE.findall(text[start:end]))):
                if opcode not in routed:
                    problems.append(
                        f"{name} : {function} traite {opcode}, que world_update.cpp "
                        "ne lui envoie jamais — ajouter le cas à sa liste")
    return problems


def rule_every_family_case_is_routed(repo_root: Path) -> list[str]:
    sources = repo_root / "libs" / "net" / "src"
    dispatch = (sources / "world_update.cpp").read_text(encoding="utf-8")
    relay = (sources / "world_relay.cpp").read_text(encoding="utf-8")
    families = {f"libs/net/src/{path.name}": path.read_text(encoding="utf-8")
                for path in sorted(sources.glob("*.cpp")) if path.name != "world_update.cpp"}
    return unrouted_family_cases(dispatch, relay, families)


def self_test_family_routing() -> list[str]:
    dispatch = ("    case proto::ZcA::kOpcode:\n"
                "      return ApplyWorldSkillFrame(frame, world, now_ms);\n")
    family = ("FrameOutcome ApplyWorldSkillFrame(const Frame& frame) {\n"
              "    case proto::ZcA::kOpcode:\n    case proto::ZcB::kOpcode:\n}\n")
    failures = []
    if not unrouted_family_cases(dispatch, "", {"famille.cpp": family}):
        failures.append("self-test : un cas de famille non aiguillé passe")
    routed = dispatch.replace("case proto::ZcA", "case proto::ZcB::kOpcode:\n    case proto::ZcA")
    if unrouted_family_cases(routed, "", {"famille.cpp": family}):
        failures.append("self-test : une famille entièrement aiguillée est rejetée")
    relayed = "ApplyWorldSkillFrame(frame, world)"
    if unrouted_family_cases(dispatch, relayed, {"famille.cpp": family}):
        failures.append("self-test : une famille consultée par la chaîne est rejetée")
    return failures
