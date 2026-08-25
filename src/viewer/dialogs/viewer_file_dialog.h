#pragma once

/**
 * @file viewer_file_dialog.h
 * @brief Presentation port for selecting viewer input files.
 */

#include <QString>

/** Selection-only boundary for viewer input files; empty always means cancel. */
class IViewerFileDialog {
  public:
    virtual ~IViewerFileDialog() = default;
    virtual QString chooseMappingPath() = 0;
    virtual QString chooseWhitelistPath() = 0;
    virtual QString chooseSessionPath() = 0;
};
