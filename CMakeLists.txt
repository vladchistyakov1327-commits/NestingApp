cmake_minimum_required(VERSION 3.16)
project(ProductionManager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS
    Core
    Gui
    Widgets
    Sql
    REQUIRED
)

set(PROJECT_SOURCES
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    
    src/Material.h
    src/Material.cpp
    src/Norm.h
    src/Norm.cpp
    src/Receipt.h
    src/Receipt.cpp
    src/Movement.h
    src/Movement.cpp
    
    src/DatabaseManager.h
    src/DatabaseManager.cpp
    src/InventoryManager.h
    src/InventoryManager.cpp
    
    src/MaterialDelegate.h
    src/MaterialDelegate.cpp
    src/MaterialsForm.h
    src/MaterialsForm.cpp
    src/ReceiptForm.h
    src/ReceiptForm.cpp
    src/ShipmentForm.h
    src/ShipmentForm.cpp
    src/MovementForm.h
    src/MovementForm.cpp
    
    src/EmployeesForm.h
    src/EmployeesForm.cpp
    src/SalaryForm.h
    src/SalaryForm.cpp
    src/PerformedForm.h
    src/PerformedForm.cpp
    
    src/NotificationSystem.h
    src/NotificationSystem.cpp
    src/EmployeeRating.h
    src/EmployeeRating.cpp
)

add_executable(ProductionManager ${PROJECT_SOURCES})

target_link_libraries(ProductionManager
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
)
