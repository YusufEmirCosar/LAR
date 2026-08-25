# 1. Domain Target
qt_add_library(lar-domain STATIC
    src/domain/decoded_state.h
    src/domain/state.h
    src/domain/statefield.h
    src/domain/statefield.cpp
    src/domain/statevalidator.h
    src/domain/statevalidator.cpp
    src/domain/dlz/dlz_types.h
    src/domain/dlz/dlz_units.h
    src/domain/dlz/dlz_geometry.h
    src/domain/dlz/dlz_geometry.cpp
    src/domain/dlz/dlz_solver.h
    src/domain/dlz/dlz_solver.cpp
    src/domain/dlz/dlz_scenario_adapter.h
    src/domain/dlz/dlz_scenario_adapter.cpp
    src/domain/packet_mapping.h
    src/domain/packet_mapping.cpp
)
target_include_directories(lar-domain PUBLIC src)
target_link_libraries(lar-domain PUBLIC Qt6::Core)

