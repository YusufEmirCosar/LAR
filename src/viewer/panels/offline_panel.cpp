
#include "viewer/panels/offline_panel.h"

#include "application/application_facade.h"
#include "application/application_view_model.h"
#include "viewer/playback_timeline_mapper.h"
#include "viewer/playbacktimeformatter.h"

#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>

namespace {
QIcon folderIcon() {
    return QIcon(QStringLiteral(":/icons/folder.png"));
}

QIcon lightningIcon() {
    return QIcon(QStringLiteral(":/icons/lightning.png"));
}

QIcon resetIcon() {
    return QIcon(QStringLiteral(":/icons/reset.png"));
}
} // namespace

OfflinePanel::OfflinePanel(ApplicationFacade &application, QWidget *parent)
    : QWidget(parent), m_application(application) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *fileGroup = new QGroupBox(QStringLiteral("Offline Session"));
    auto *fileLayout = new QVBoxLayout(fileGroup);
    m_fileLabel = new QLabel(QStringLiteral("No .lar file selected"));
    m_fileLabel->setWordWrap(true);
    m_fileLabel->setStyleSheet(QStringLiteral("color: #65706a;"));
    auto *open = new QPushButton(folderIcon(), QStringLiteral("Select .lar File"));
    open->setIconSize(QSize(18, 18));
    connect(open, &QPushButton::clicked, this, &OfflinePanel::sessionSelectionRequested);
    fileLayout->addWidget(m_fileLabel);
    fileLayout->addWidget(open);
    layout->addWidget(fileGroup);

    auto *playbackGroup = new QGroupBox(QStringLiteral("Playback"));
    playbackGroup->setObjectName(QStringLiteral("playbackPanel"));
    auto *playbackLayout = new QVBoxLayout(playbackGroup);
    m_timeLabel = new QLabel(QStringLiteral("00:00.000 / 00:00.000"));
    m_timeline = new QSlider(Qt::Horizontal);
    m_timeline->setRange(0, 10000);
    m_timeline->setEnabled(false);
    connect(m_timeline, &QSlider::sliderReleased, this, [this] {
        if (m_duration == SessionTimestamp::zero())
            return;
        const SessionTimestamp position = PlaybackTimelineMapper::fromSlider(
            m_duration, m_timeline->value(), m_timeline->maximum());
        m_application.seek(position);
    });
    playbackLayout->addWidget(m_timeLabel);
    playbackLayout->addWidget(m_timeline);

    auto *controls = new QHBoxLayout;
    m_playPause = new QPushButton(style()->standardIcon(QStyle::SP_MediaPlay), QString());
    m_stop = new QPushButton(style()->standardIcon(QStyle::SP_MediaStop), QString());
    m_repeat = new QPushButton(resetIcon(), QString());
    m_repeat->setObjectName(QStringLiteral("repeatPlaybackButton"));
    m_repeat->setCheckable(true);
    m_repeat->setAccessibleName(QStringLiteral("Repeat playback"));
    m_playPause->setToolTip(QStringLiteral("Play"));
    m_stop->setToolTip(QStringLiteral("Stop"));
    m_repeat->setToolTip(QStringLiteral("Repeat playback"));
    m_playPause->setEnabled(false);
    m_stop->setEnabled(false);
    m_repeat->setEnabled(false);
    connect(m_playPause, &QPushButton::clicked, this, [this] {
        if (m_playing)
            m_application.pause();
        else
            m_application.play();
    });
    connect(m_stop, &QPushButton::clicked, &m_application, &ApplicationFacade::stop);
    connect(m_repeat, &QPushButton::clicked, this, [this](bool checked) {
        if (m_application.setPlaybackRepeat(checked))
            return;
        const QSignalBlocker blocker(m_repeat);
        m_repeat->setChecked(!checked);
    });

    m_rate = new QLineEdit(QStringLiteral("1"));
    m_rate->setObjectName(QStringLiteral("playbackRateInput"));
    m_rate->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_rate->setMinimumWidth(58);
    m_rate->setMaximumWidth(78);
    auto *rateUnit = new QLabel(QStringLiteral("x"));
    rateUnit->setObjectName(QStringLiteral("playbackRateUnit"));
    rateUnit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    connect(m_rate, &QLineEdit::editingFinished, this, &OfflinePanel::applyPlaybackRate);
    controls->addWidget(m_playPause);
    controls->addWidget(m_stop);
    controls->addWidget(m_repeat);
    controls->addStretch();
    controls->addWidget(m_rate);
    controls->addWidget(rateUnit);
    playbackLayout->addLayout(controls);
    layout->addWidget(playbackGroup);

    auto *burstGroup = new QGroupBox(QStringLiteral("Burst"));
    burstGroup->setObjectName(QStringLiteral("burstPanel"));
    auto *burstLayout = new QVBoxLayout(burstGroup);
    m_burst = new QPushButton(lightningIcon(), QStringLiteral("Burst"));
    m_burst->setObjectName(QStringLiteral("burstPlaybackButton"));
    m_burst->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_burst->setToolTip(QStringLiteral("Burst analysis is not available yet"));
    m_burst->setEnabled(false);
    burstLayout->addWidget(m_burst);
    layout->addWidget(burstGroup);
    layout->addStretch();

    connect(&m_application, &ApplicationFacade::sessionLoaded, this, &OfflinePanel::sessionLoaded);
    connect(&m_application.viewModel(), &ApplicationViewModel::modeChanged, this,
            &OfflinePanel::renderMode);
    connect(&m_application.viewModel(), &ApplicationViewModel::playbackStateChanged, this,
            [this] { renderPlayback(m_application.viewModel()); });
}

void OfflinePanel::sessionLoaded(const QString &path, qint64 recordCount) {
    m_fileLabel->setText(QFileInfo(path).fileName());
    m_duration = m_application.viewModel().playbackDuration();
    m_timeline->setEnabled(true);
    const bool hasRecords = recordCount > 0;
    m_playPause->setEnabled(hasRecords);
    m_stop->setEnabled(hasRecords);
    m_repeat->setEnabled(hasRecords);
    m_timeLabel->setText(PlaybackTimeFormatter::format({}) + QStringLiteral(" / ") +
                         PlaybackTimeFormatter::format(m_duration));
}

void OfflinePanel::renderMode(ApplicationMode mode) {
    m_playing = mode == ApplicationMode::Playing;
    m_playPause->setIcon(
        style()->standardIcon(m_playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    m_playPause->setToolTip(m_playing ? QStringLiteral("Pause") : QStringLiteral("Play"));
    if (m_application.isSessionLoaded())
        return;

    m_duration = {};
    m_timeline->setValue(0);
    m_timeline->setEnabled(false);
    m_playPause->setEnabled(false);
    m_stop->setEnabled(false);
    m_repeat->setEnabled(false);
    m_timeLabel->setText(QStringLiteral("00:00.000 / 00:00.000"));
}

void OfflinePanel::renderPlayback(const ApplicationViewModel &viewModel) {
    m_duration = viewModel.playbackDuration();
    const SessionTimestamp position = viewModel.playbackPosition();
    m_timeLabel->setText(PlaybackTimeFormatter::format(position) + QStringLiteral(" / ") +
                         PlaybackTimeFormatter::format(m_duration));
    if (!m_timeline->isSliderDown() && m_duration != SessionTimestamp::zero()) {
        m_timeline->setValue(
            PlaybackTimelineMapper::toSlider(position, m_duration, m_timeline->maximum()));
    }
}

void OfflinePanel::applyPlaybackRate() {
    QString text = m_rate->text().trimmed();
    if (text.endsWith(QLatin1Char('x'), Qt::CaseInsensitive))
        text.chop(1);
    bool valid = false;
    const double rate = text.toDouble(&valid);
    if (!valid || !std::isfinite(rate) || rate <= 0.0 || !m_application.setPlaybackRate(rate)) {
        m_rate->setText(QStringLiteral("1"));
        m_application.setPlaybackRate(1.0);
        return;
    }
    m_rate->setText(QString::number(rate));
}
