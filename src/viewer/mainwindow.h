#pragma once

/**
 * @file mainwindow.h
 * @brief Top-level Qt Widgets composition and ApplicationFacade bindings.
 */

#include "application/application_facade.h"

#include <QMainWindow>
#include <QVector>

#include <memory>

class QButtonGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QStackedWidget;
class LarViewport;
class ViewportControls;
class IRecordingFileDialog;
class RecordingWorkflowController;
class ValuesPanel;
class MetricsPanel;
class OfflinePanel;
class OnlinePanel;
class IViewerFileDialog;
class OnlineWorkflowController;

namespace dlz::presentation {
class ControlPanel;
}

/**
 * @brief Builds the control panels and renders ApplicationViewModel changes.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    explicit MainWindow(ApplicationFacade &app, QWidget *parent = nullptr);
    MainWindow(ApplicationFacade &app, IRecordingFileDialog &recordingDialog,
               QWidget *parent = nullptr);
    ~MainWindow() override;

  public slots:
    Q_INVOKABLE void updateState(const Plane &plane, const Target &target,
                                 const QBitArray &availableFields);
    Q_INVOKABLE void updateState(const DecodedState &state);

  protected:
    void closeEvent(QCloseEvent *event) override;

  private slots:
    void updateStateFromViewModel();

  private:
    QWidget *buildSidebar();
    void connectFacade();

    void switchMode(int modeIndex);
    void showDiagnostic(const QString &message);
    void updateCameraControls();
    void chooseLarPath();
    void initialize(IRecordingFileDialog &recordingDialog);

    ApplicationFacade &m_app;
    std::unique_ptr<IRecordingFileDialog> m_ownedRecordingDialog;
    std::unique_ptr<IViewerFileDialog> m_ownedViewerFileDialog;
    RecordingWorkflowController *m_recordingWorkflow = nullptr;
    OnlineWorkflowController *m_onlineWorkflow = nullptr;
    LarViewport *m_viewport = nullptr;
    ViewportControls *m_viewportControls = nullptr;
    ValuesPanel *m_valuesPanel = nullptr;
    MetricsPanel *m_metricsPanel = nullptr;
    OfflinePanel *m_offlinePanel = nullptr;
    OnlinePanel *m_onlinePanel = nullptr;
    QStackedWidget *m_modeStack = nullptr;
    QPushButton *m_onlineButton = nullptr;
    QPushButton *m_offlineButton = nullptr;
};
