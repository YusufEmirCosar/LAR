
#include "viewer/dialogs/qt_viewer_file_dialog.h"

#include <QFileDialog>

QString QtViewerFileDialog::chooseMappingPath() {
    return QFileDialog::getOpenFileName(m_parent, QStringLiteral("Open UDP field mapping"),
                                        QString(), QStringLiteral("JSON mapping (*.json)"));
}

QString QtViewerFileDialog::chooseWhitelistPath() {
    return QFileDialog::getOpenFileName(m_parent, QStringLiteral("Open IP whitelist"), QString(),
                                        QStringLiteral("Text files (*.txt *.list);;All files (*)"));
}

QString QtViewerFileDialog::chooseSessionPath() {
    return QFileDialog::getOpenFileName(m_parent, QStringLiteral("Open LAR session"), QString(),
                                        QStringLiteral("LAR session (*.lar)"));
}
