cmake_minimum_required(VERSION 3.19)
project(TrashPetAI LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

qt_add_executable(TrashPetAI
    WIN32 MACOSX_BUNDLE
    main.cpp
    characterwidget.cpp    characterwidget.h
    chatwidget.cpp         chatwidget.h
    configmanager.cpp      configmanager.h
    geminiclient.cpp       geminiclient.h
    openaiclient.cpp       openaiclient.h
    claudeclient.cpp       claudeclient.h
    aiclient.h
    petstate.cpp           petstate.h
    hungermanager.cpp      hungermanager.h
    recyclebinmanager.cpp  recyclebinmanager.h
    eatdialog.cpp          eatdialog.h
    cleanupplan.cpp        cleanupplan.h
    setupdialog.cpp        setupdialog.h
    systemmonitor.cpp      systemmonitor.h
    healthdialog.cpp       healthdialog.h
    characterstate.h
    hungerconfig.h
    healthconfig.h
    costume.h
)

# windres không xử lý tốt -I có khoảng trắng (đường dẫn project có dấu cách) —
# gọi windres trực tiếp, không qua CMake RC language support (tự thêm -I gây lỗi).
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resource_rc.o
    COMMAND ${CMAKE_RC_COMPILER} -O coff -o ${CMAKE_CURRENT_BINARY_DIR}/resource_rc.o resource.rc
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resource.rc ${CMAKE_CURRENT_SOURCE_DIR}/imgs/logo.ico
    COMMENT "Building Windows icon resource"
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/resource_rc.o PROPERTIES
    EXTERNAL_OBJECT TRUE
    GENERATED TRUE
)
target_sources(TrashPetAI PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/resource_rc.o)

# windres không xử lý tốt include path có khoảng trắng (đường dẫn project có dấu cách) —
# resource.rc không cần include dir của C++/Qt nên xoá hết để tránh lỗi preprocessing.
set_source_files_properties(resource.rc PROPERTIES INCLUDE_DIRECTORIES "")

target_link_libraries(TrashPetAI PRIVATE
    Qt6::Widgets
    Qt6::Network
    Shell32
    Ole32
    OleAut32
)

install(TARGETS TrashPetAI
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET TrashPetAI
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
