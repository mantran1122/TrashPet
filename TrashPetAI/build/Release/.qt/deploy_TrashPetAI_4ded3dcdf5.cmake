include("C:/Users/Administrator/Desktop/TrashPet AI Desktop Assistant/TrashPetAI/build/Release/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/TrashPetAI-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "C:/Users/Administrator/Desktop/TrashPet AI Desktop Assistant/TrashPetAI/build/Release/TrashPetAI.exe"
    GENERATE_QT_CONF
)
