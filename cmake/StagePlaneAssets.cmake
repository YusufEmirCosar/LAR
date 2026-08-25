foreach(required_variable IN ITEMS
        LAR_PLANE_ASSET_SOURCE
        LAR_PLANE_ASSET_DESTINATION
        LAR_PLANE_ASSET_LOCK_ROOT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} must be provided")
    endif()
endforeach()

file(MAKE_DIRECTORY "${LAR_PLANE_ASSET_LOCK_ROOT}")
file(LOCK
    "${LAR_PLANE_ASSET_LOCK_ROOT}/stage.lock"
    GUARD PROCESS
    TIMEOUT 120
    RESULT_VARIABLE lock_result
)
if(NOT lock_result STREQUAL "0")
    message(FATAL_ERROR "Could not lock plane asset staging: ${lock_result}")
endif()

file(REMOVE_RECURSE
    "${LAR_PLANE_ASSET_DESTINATION}/skyboxes"
    "${LAR_PLANE_ASSET_DESTINATION}/cubemaps"
    "${LAR_PLANE_ASSET_DESTINATION}/models"
    "${LAR_PLANE_ASSET_DESTINATION}/water"
)
file(MAKE_DIRECTORY
    "${LAR_PLANE_ASSET_DESTINATION}/cubemaps"
    "${LAR_PLANE_ASSET_DESTINATION}/models"
    "${LAR_PLANE_ASSET_DESTINATION}/water"
)
file(COPY
    "${LAR_PLANE_ASSET_SOURCE}/models/f16_3.glb"
    DESTINATION "${LAR_PLANE_ASSET_DESTINATION}/models"
)
file(COPY
    "${LAR_PLANE_ASSET_SOURCE}/cubemaps/"
    DESTINATION "${LAR_PLANE_ASSET_DESTINATION}/cubemaps"
)
file(COPY
    "${LAR_PLANE_ASSET_SOURCE}/water/dted0_water_mask.bin"
    DESTINATION "${LAR_PLANE_ASSET_DESTINATION}/water"
)
