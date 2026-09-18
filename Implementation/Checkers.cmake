set(Checkers_NAME Checkers)

file(GLOB Checkers_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
file(GLOB Checkers_INCS    "${CMAKE_CURRENT_LIST_DIR}/src/*.h")

#Application icon
set(Checkers_PLIST ${CMAKE_CURRENT_LIST_DIR}/res/appIcon/AppIcon.plist)
if(WIN32)
	set(Checkers_WINAPP_ICON ${CMAKE_CURRENT_LIST_DIR}/res/appIcon/winAppIcon.rc)
else()
	set(Checkers_WINAPP_ICON ${CMAKE_CURRENT_LIST_DIR}/res/appIcon/winAppIcon.cpp)
endif()

add_executable(${Checkers_NAME} ${Checkers_INCS} ${Checkers_SOURCES} ${Checkers_WINAPP_ICON})

source_group("inc" FILES ${Checkers_INCS})
source_group("src" FILES ${Checkers_SOURCES})

target_link_libraries(${Checkers_NAME} debug ${MU_LIB_DEBUG} debug ${NATGUI_LIB_DEBUG}
									optimized ${MU_LIB_RELEASE} optimized ${NATGUI_LIB_RELEASE})

setTargetPropertiesForGUIApp(${Checkers_NAME} ${Checkers_PLIST})

setAppIcon(${Checkers_NAME} ${CMAKE_CURRENT_LIST_DIR})

setIDEPropertiesForGUIExecutable(${Checkers_NAME} ${CMAKE_CURRENT_LIST_DIR})

setPlatformDLLPath(${Checkers_NAME})
