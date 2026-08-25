
#include "viewer/dialogs/qt_recording_file_dialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QWidget>

QString QtRecordingFileDialog::chooseSavePath(RecordingSaveKind kind) {
    const bool snapshot = kind == RecordingSaveKind::Snapshot;
    const QString title = snapshot ? QStringLiteral("Save session snapshot")
                                   : QStringLiteral("Stop and save LAR session");
    const QString defaultName =
        snapshot ? QStringLiteral("lar-snapshot.lar") : QStringLiteral("lar-session.lar");
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString path =
        QFileDialog::getSaveFileName(m_parent, title, directory + QLatin1Char('/') + defaultName,
                                     QStringLiteral("LAR session (*.lar)"));
    if (!path.isEmpty() && !path.endsWith(QStringLiteral(".lar"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".lar");
    }
    return path;
}

bool QtRecordingFileDialog::confirmReset() {
    return QMessageBox::question(
               m_parent, QStringLiteral("Reset session"),
               QStringLiteral("Erase all packages in the current in-memory session?"),
               QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
}

bool QtRecordingFileDialog::confirmDiscardOnStop() {
    return QMessageBox::question(
               m_parent, QStringLiteral("Stop listener"),
               QStringLiteral("Stopping the online listener will erase the saved packages in the "
                              "current in-memory session. Do you want to continue?"),
               QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
}

bool QtRecordingFileDialog::confirmDiscardOnClose() {
    return QMessageBox::question(
               m_parent, QStringLiteral("Discard session"),
               QStringLiteral("Close the app and discard the current in-memory session?"),
               QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
}
