#pragma once

/**
 * @file application_state.h
 * @brief Internal lifecycle state owned by ApplicationFacade.
 */

#include <QByteArray>
#include <QtGlobal>

/**
 * Authoritative workflow state owned by ApplicationFacade.
 *
 * Presentation data (the latest Plane/Target and formatted metrics) remains in
 * ApplicationViewModel. Lifecycle and command-eligibility facts live here so
 * they cannot drift across unrelated facade booleans.
 */
struct ApplicationState final {
    QByteArray mappingJson;
    int mappedFieldCount = 0;
    int minimumPacketSize = 0;
    bool listening = false;
    quint16 listeningPort = 0;
    bool hasRecordingSession = false;
    bool recordingPaused = false;
    bool sessionLoaded = false;
    qint64 loadedRecordCount = 0;
    bool shuttingDown = false;

    bool isRecording() const noexcept {
        return hasRecordingSession && !recordingPaused;
    }

    bool isRecordingPaused() const noexcept {
        return hasRecordingSession && recordingPaused;
    }
};
