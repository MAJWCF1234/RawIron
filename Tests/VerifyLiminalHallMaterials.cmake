if (NOT DEFINED RAWIRON_WORKSPACE)
  message(FATAL_ERROR "RAWIRON_WORKSPACE must point at the RawIron workspace root.")
endif()

set(structural_csv "${RAWIRON_WORKSPACE}/Games/LiminalHall/levels/assembly.structural.csv")
set(primitives_csv "${RAWIRON_WORKSPACE}/Games/LiminalHall/levels/assembly.primitives.csv")

file(READ "${structural_csv}" structural_text)
if (structural_text MATCHES "ri_psx_official_")
  message(FATAL_ERROR "Liminal Hall structural assembly still references legacy ri_psx_official textures.")
endif()

file(READ "${primitives_csv}" primitives_text)
if (primitives_text MATCHES "(SouthCorridorFluorescentGlow_import1|SouthCorridorBackGlow_import1|NorthApseDoorGlow_import1|OculusCoreGlow_import1|OculusSpokeNorth_import1|OculusSpokeSouth_import1|OculusSpokeWest_import1|OculusSpokeEast_import1|TowerWindowCardW1_import1|TowerWindowCardW2_import1|TowerWindowCardW3_import1|TowerWindowCardE1_import1|TowerWindowCardE2_import1|TowerWindowCardE3_import1|OverhangWindowCardA_import1|OverhangWindowCardB_import1|MidTowerWindowA_import1|MidTowerWindowB_import1|MidTowerWindowC_import1)[^\n]*ri_prototype_white\\.png")
  message(FATAL_ERROR "Liminal Hall visible primitive cards still reference placeholder white prototype textures.")
endif()
if (primitives_text MATCHES "[A-Za-z]:/")
  message(FATAL_ERROR "Liminal Hall primitives CSV still embeds machine-absolute texture paths (drive-letter absolute).")
endif()
if (primitives_text MATCHES "[A-Za-z]:\\\\")
  message(FATAL_ERROR "Liminal Hall primitives CSV still embeds machine-absolute texture paths (Windows absolute).")
endif()

message(STATUS "VerifyLiminalHallMaterials: structural and primitive texture audit passed.")
