# 2. Application Target
qt_add_library(lar-application STATIC
    src/application/captured_packet.h
    src/application/session_limits.h
    src/application/recording_operation_state.h
    src/application/ip_access_policy.h
    src/application/ip_access_policy.cpp
    src/application/ip_access_policy_service.h
    src/application/ip_access_policy_service.cpp
    src/application/ports/datagram_source.h
    src/application/ports/ip_access_policy_repository.h
    src/application/ports/packet_decoder.h
    src/application/ports/mapping_repository.h
    src/application/ports/session_snapshot.h
    src/application/ports/recording_transaction.h
    src/application/ports/session_persistence.h
    src/application/ports/session_reader.h
    src/application/ports/recording_clock.h
    src/application/ports/playback_clock.h
    src/application/ports/application_runtime.h
    src/application/ports/runtime_messages.h
    src/application/ports/runtime_messages.cpp
    src/application/ports/runtime_event_context.h
    src/application/mode_coordinator.h
    src/application/mode_coordinator.cpp
    src/application/source_lifecycle_coordinator.h
    src/application/source_lifecycle_coordinator.cpp
    src/application/request_result_gate.h
    src/application/session_timestamp.h
    src/application/application_state.h
    src/application/application_view_model.h
    src/application/application_view_model.cpp
    src/application/metrics_service.h
    src/application/metrics_service.cpp
    src/application/online_capture_service.h
    src/application/online_capture_service.cpp
    src/application/recording_service.h
    src/application/recording_service.cpp
    src/application/recording_pipeline_coordinator.h
    src/application/recording_pipeline_coordinator.cpp
    src/application/playback_service.h
    src/application/playback_service.cpp
    src/application/direct_application_runtime.h
    src/application/direct_application_runtime.cpp
    src/application/application_facade.h
    src/application/application_facade.cpp
)
target_include_directories(lar-application PUBLIC src)
target_link_libraries(lar-application PUBLIC lar-domain Qt6::Core)

