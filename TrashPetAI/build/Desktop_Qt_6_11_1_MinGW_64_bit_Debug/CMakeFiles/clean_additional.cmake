# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\TrashPetAI_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\TrashPetAI_autogen.dir\\ParseCache.txt"
  "TrashPetAI_autogen"
  )
endif()
