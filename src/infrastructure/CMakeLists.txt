# 3. Infrastructure Target
qt_add_library(lar-infrastructure-qt STATIC
    src/infrastructure/network/qt_udp_datagram_source.h
    src/infrastructure/network/qt_udp_datagram_source.cpp
    src/infrastructure/network/qt_ip_address_normalizer.h
    src/infrastructure/network/qt_ip_address_normalizer.cpp
    src/infrastructure/network/qt_ip_access_policy_repository.h
    src/infrastructure/network/qt_ip_access_policy_repository.cpp
    src/infrastructure/mapping/json_mapping_repository.h
    src/infrastructure/mapping/json_mapping_repository.cpp
    src/infrastructure/mapping/mapped_packet_decoder.h
    src/infrastructure/mapping/mapped_packet_decoder.cpp
    src/infrastructure/session/lar_session_writer.h
    src/infrastructure/session/lar_session_writer.cpp
    src/infrastructure/session/lar_session_reader.h
    src/infrastructure/session/lar_session_reader.cpp
    src/infrastructure/session/file_session_snapshot.h
    src/infrastructure/session/file_session_snapshot.cpp
    src/infrastructure/session/qt_session_persistence.h
    src/infrastructure/session/qt_session_persistence.cpp
    src/infrastructure/timing/qt_recording_clock.h
    src/infrastructure/timing/qt_recording_clock.cpp
    src/infrastructure/timing/qt_playback_clock.h
    src/infrastructure/timing/qt_playback_clock.cpp
    src/infrastructure/runtime/network_runtime_worker.h
    src/infrastructure/runtime/network_runtime_worker.cpp
    src/infrastructure/runtime/recording_runtime_worker.h
    src/infrastructure/runtime/recording_runtime_worker.cpp
    src/infrastructure/runtime/playback_metrics.h
    src/infrastructure/runtime/playback_metrics.cpp
    src/infrastructure/runtime/playback_runtime_worker.h
    src/infrastructure/runtime/playback_runtime_worker.cpp
    src/infrastructure/runtime/persistence_runtime_worker.h
    src/infrastructure/runtime/persistence_runtime_worker.cpp
    src/infrastructure/runtime/threaded_application_runtime.h
    src/infrastructure/runtime/threaded_application_runtime.cpp
)
target_include_directories(lar-infrastructure-qt PUBLIC src)
target_link_libraries(lar-infrastructure-qt PUBLIC lar-application lar-domain Qt6::Core Qt6::Network)
