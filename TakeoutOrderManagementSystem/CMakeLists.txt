cmake_minimum_required(VERSION 3.21)

project(TakeoutOrderManagementSystem VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Qt6 6.11.2 REQUIRED COMPONENTS Core Gui Widgets Network)
include(CTest)

add_library(takeout_core STATIC
    core/enums.h core/entities.h core/result.h core/requests.h
    data/repository.h data/jsonrepository.h data/jsonrepository.cpp
    data/datastore.h data/datastore.cpp data/apppaths.h data/apppaths.cpp
    services/sessioncontext.h services/sessioncontext.cpp
    services/servicebase.h services/authservice.h services/catalogservice.h
    services/orderservice.h services/orderqueryservice.h
    services/adminservice.h services/statisticsservice.h
)
target_include_directories(takeout_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(takeout_core PUBLIC Qt6::Core PRIVATE Qt6::Network)

add_library(takeout_models STATIC
    models/ordertablemodel.h models/ordertablemodel.cpp
    models/orderfilterproxymodel.h models/orderfilterproxymodel.cpp
)
target_link_libraries(takeout_models PUBLIC takeout_core Qt6::Gui)

add_library(takeout_ui STATIC
    mainwindow.h mainwindow.cpp mainwindow.ui
    app/appcontext.h app/appcontext.cpp
    delegates/moneydelegate.h delegates/moneydelegate.cpp
    delegates/orderstatusdelegate.h delegates/orderstatusdelegate.cpp
)
target_link_libraries(takeout_ui PUBLIC takeout_models Qt6::Widgets)

qt_add_executable(TakeoutOrderManagementSystem WIN32 main.cpp resources/resources.qrc)
target_link_libraries(TakeoutOrderManagementSystem PRIVATE takeout_ui)

if(MSVC)
    add_compile_options(/utf-8)
endif()
if(BUILD_TESTING)
    find_package(Qt6 6.11.2 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
install(TARGETS TakeoutOrderManagementSystem RUNTIME DESTINATION .)
