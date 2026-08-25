
#include "infrastructure/session/qt_session_persistence.h"

#include <QSaveFile>

bool QtSessionPersistence::save(const SessionSnapshot &snapshot, const QString &targetPath,
                                QString *error) {
    if (!snapshot || targetPath.isEmpty()) {
        if (error)
            *error = QStringLiteral("Session persistence request is invalid");
        return false;
    }

    QSaveFile destination(targetPath);
    if (!destination.open(QIODevice::WriteOnly)) {
        if (error)
            *error = destination.errorString();
        return false;
    }

    if (!snapshot->writeTo(destination, error)) {
        destination.cancelWriting();
        return false;
    }

    if (!destination.commit()) {
        if (error)
            *error = destination.errorString();
        return false;
    }
    return true;
}
