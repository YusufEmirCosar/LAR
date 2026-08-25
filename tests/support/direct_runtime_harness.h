#pragma once

#include "application/direct_application_runtime.h"
#include "infrastructure/mapping/json_mapping_repository.h"
#include "infrastructure/mapping/mapped_packet_decoder.h"
#include "infrastructure/session/lar_session_reader.h"
#include "infrastructure/session/lar_session_writer.h"
#include "infrastructure/session/qt_session_persistence.h"
#include "infrastructure/timing/qt_playback_clock.h"
#include "infrastructure/timing/qt_recording_clock.h"
#include "tests/support/fake_datagram_source.h"

/** Owns every direct-runtime dependency for shared contract tests. */
struct DirectRuntimeHarness final {
    FakeDatagramSource datagramSource;
    MappedPacketDecoder decoder;
    MetricsService metrics;
    JsonMappingRepository mappingRepository;
    LarSessionWriter recordingTransaction;
    QtSessionPersistence persistence;
    QtRecordingClock recordingClock;
    LarSessionReader sessionReader;
    QtPlaybackClock playbackClock;

    OnlineCaptureService capture{datagramSource, decoder, metrics};
    RecordingService recording{recordingTransaction, recordingClock};
    PlaybackService playback{sessionReader, playbackClock};
    DirectApplicationRuntime runtime{capture, recording,         playback,
                                     metrics, mappingRepository, persistence};
};
