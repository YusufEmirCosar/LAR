#include "viewer/help/help_window.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QTextCursor>
#include <QUrl>
#include <QVBoxLayout>

#include <utility>

HelpWindow::HelpWindow(QWidget *parent) : QDialog(parent) {
    setObjectName(QStringLiteral("applicationHelpWindow"));
    setWindowTitle(QStringLiteral("LAR Packet Monitor Help"));
    setModal(false);
    resize(920, 650);
    setMinimumSize(640, 440);

    appendSectionTopics(QStringLiteral(":/help/USER_GUIDE.md"), QString());
    appendSectionTopics(QStringLiteral(":/help/PROTOCOL_UNITS.md"), QStringLiteral("protocol-"),
                        QStringLiteral("Data · "));
    appendReferenceTopic(QStringLiteral(":/help/DLZ_MODEL.md"), QStringLiteral("reference-dlz"),
                         QStringLiteral("Reference · DLZ model"));
    appendReferenceTopic(QStringLiteral(":/help/TERRAIN_AND_ASSETS.md"),
                         QStringLiteral("reference-terrain"),
                         QStringLiteral("Reference · Terrain and assets"));
    appendReferenceTopic(QStringLiteral(":/help/THREAT_MODEL.md"),
                         QStringLiteral("reference-security"),
                         QStringLiteral("Reference · Security and limits"));
    appendReferenceTopic(QStringLiteral(":/help/DEVELOPER_GUIDE.md"),
                         QStringLiteral("reference-development"),
                         QStringLiteral("Reference · Build and development"));
    appendReferenceTopic(QStringLiteral(":/help/TEST_SENDER.md"),
                         QStringLiteral("reference-test-sender"),
                         QStringLiteral("Reference · Test sender"));

    Topic about;
    about.id = QStringLiteral("about");
    about.title = QStringLiteral("About and shortcuts");
    about.markdown =
        QStringLiteral("# About and shortcuts\n\n**LAR Packet Monitor %1**\n\n"
                       "LAR Packet Monitor is an engineering visualization tool. It is not a "
                       "certified flight, navigation, fire-control, or weapon-employment "
                       "system.\n\n- **F1** opens or focuses this help window.\n"
                       "- Type in **Search help** to filter every bundled topic.\n"
                       "- **Help for current screen** jumps to Online, Offline, LAR, Plane, or "
                       "DLZ instructions.\n- **Escape** closes this window.\n")
            .arg(QCoreApplication::applicationVersion());
    m_topics.append(std::move(about));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 12);
    root->setSpacing(10);

    auto *navigation = new QHBoxLayout;
    m_back = new QPushButton(QStringLiteral("← Back"), this);
    m_forward = new QPushButton(QStringLiteral("Forward →"), this);
    m_back->setObjectName(QStringLiteral("helpBackButton"));
    m_forward->setObjectName(QStringLiteral("helpForwardButton"));
    m_back->setAccessibleName(QStringLiteral("Previous help topic"));
    m_forward->setAccessibleName(QStringLiteral("Next help topic"));
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("helpSearchInput"));
    m_search->setPlaceholderText(QStringLiteral("Search help"));
    m_search->setClearButtonEnabled(true);
    m_search->setAccessibleName(QStringLiteral("Search help"));
    navigation->addWidget(m_back);
    navigation->addWidget(m_forward);
    navigation->addSpacing(8);
    navigation->addWidget(m_search, 1);
    root->addLayout(navigation);

    auto *content = new QHBoxLayout;
    content->setSpacing(12);
    m_topicList = new QListWidget(this);
    m_topicList->setObjectName(QStringLiteral("helpTopicList"));
    m_topicList->setAccessibleName(QStringLiteral("Help topics"));
    m_topicList->setMinimumWidth(220);
    m_topicList->setMaximumWidth(285);
    for (const Topic &topic : std::as_const(m_topics)) {
        m_topicList->addItem(topic.title);
    }
    m_browser = new QTextBrowser(this);
    m_browser->setObjectName(QStringLiteral("helpContentBrowser"));
    m_browser->setAccessibleName(QStringLiteral("Help content"));
    m_browser->setOpenLinks(false);
    m_browser->setOpenExternalLinks(false);
    content->addWidget(m_topicList);
    content->addWidget(m_browser, 1);
    root->addLayout(content, 1);

    auto *footer = new QHBoxLayout;
    auto *currentScreen = new QPushButton(QStringLiteral("Help for current screen"), this);
    currentScreen->setObjectName(QStringLiteral("helpCurrentScreenButton"));
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("helpStatusLabel"));
    m_status->setWordWrap(true);
    auto *closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    closeButtons->setObjectName(QStringLiteral("helpCloseButtons"));
    footer->addWidget(currentScreen);
    footer->addWidget(m_status, 1);
    footer->addWidget(closeButtons);
    root->addLayout(footer);

    connect(m_search, &QLineEdit::textChanged, this, &HelpWindow::applySearch);
    connect(m_search, &QLineEdit::returnPressed, this, [this] {
        if (m_topicList->currentRow() >= 0) {
            m_browser->setFocus();
        }
    });
    connect(m_topicList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!m_internalSelection) {
            navigateToRow(row, true);
        }
    });
    connect(m_back, &QPushButton::clicked, this, [this] { navigateHistory(-1); });
    connect(m_forward, &QPushButton::clicked, this, [this] { navigateHistory(1); });
    connect(currentScreen, &QPushButton::clicked, this, [this] {
        if (m_currentTopicResolver) {
            showTopic(m_currentTopicResolver());
        }
    });
    connect(closeButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_browser, &QTextBrowser::anchorClicked, this, &HelpWindow::handleLink);

    const int initialRow = rowForTopic(QStringLiteral("what-the-application-does"));
    navigateToRow(initialRow >= 0 ? initialRow : 0, true);
}

void HelpWindow::setCurrentTopicResolver(std::function<QString()> resolver) {
    m_currentTopicResolver = std::move(resolver);
}

void HelpWindow::showTopic(const QString &requestedTopicId) {
    const int row = rowForTopic(requestedTopicId);
    if (row >= 0) {
        navigateToRow(row, true);
    }
}

QString HelpWindow::currentTopicId() const {
    const int row = m_topicList != nullptr ? m_topicList->currentRow() : -1;
    return row >= 0 && row < m_topics.size() ? m_topics.at(row).id : QString();
}

QString HelpWindow::topicId(const QString &title) {
    QString result = title.toLower();
    result.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    while (result.startsWith(QLatin1Char('-'))) {
        result.remove(0, 1);
    }
    while (result.endsWith(QLatin1Char('-'))) {
        result.chop(1);
    }
    return result;
}

QString HelpWindow::readResource(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(file.readAll())
                                                            : QString();
}

void HelpWindow::appendSectionTopics(const QString &resourcePath, const QString &idPrefix,
                                     const QString &titlePrefix) {
    const QString document = readResource(resourcePath);
    if (document.isEmpty()) {
        return;
    }
    Topic current;
    const QStringList lines = document.split(QLatin1Char('\n'));
    const auto commit = [this, &current] {
        if (!current.id.isEmpty()) {
            m_topics.append(current);
        }
    };
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("## "))) {
            commit();
            const QString heading = line.mid(3).trimmed();
            current = {};
            current.id = idPrefix + topicId(heading);
            current.title = titlePrefix + heading;
            current.markdown = QStringLiteral("# %1\n").arg(heading);
        } else if (!current.id.isEmpty()) {
            current.markdown += line + QLatin1Char('\n');
        }
    }
    commit();
}

void HelpWindow::appendReferenceTopic(const QString &resourcePath, const QString &id,
                                      const QString &title) {
    const QString document = readResource(resourcePath);
    if (document.isEmpty()) {
        return;
    }
    m_topics.append({id, title, document});
}

void HelpWindow::applySearch(const QString &query) {
    const QString needle = query.trimmed();
    int firstVisible = -1;
    int visibleCount = 0;
    for (int row = 0; row < m_topicList->count(); ++row) {
        const Topic &topic = m_topics.at(row);
        const bool matches = needle.isEmpty() ||
                             topic.title.contains(needle, Qt::CaseInsensitive) ||
                             topic.markdown.contains(needle, Qt::CaseInsensitive);
        m_topicList->item(row)->setHidden(!matches);
        if (matches) {
            if (firstVisible < 0) {
                firstVisible = row;
            }
            ++visibleCount;
        }
    }
    m_status->setText(visibleCount == 0 ? QStringLiteral("No help topics match “%1”.").arg(needle)
                                        : QString());
    const int current = m_topicList->currentRow();
    if (firstVisible >= 0 && (current < 0 || m_topicList->item(current)->isHidden())) {
        navigateToRow(firstVisible, true);
    }
    if (!needle.isEmpty() && visibleCount > 0) {
        m_browser->moveCursor(QTextCursor::Start);
        m_browser->find(needle);
    }
}

void HelpWindow::navigateToRow(int row, bool addToHistory) {
    if (row < 0 || row >= m_topics.size()) {
        return;
    }
    if (addToHistory) {
        const QString id = m_topics.at(row).id;
        if (m_historyPosition < 0 || m_history.value(m_historyPosition) != id) {
            while (m_history.size() > m_historyPosition + 1) {
                m_history.removeLast();
            }
            m_history.append(id);
            m_historyPosition = m_history.size() - 1;
        }
    }
    m_internalSelection = true;
    m_topicList->setCurrentRow(row);
    m_topicList->scrollToItem(m_topicList->item(row));
    m_internalSelection = false;
    renderRow(row);
    updateNavigationButtons();
}

void HelpWindow::renderRow(int row) {
    if (row < 0 || row >= m_topics.size()) {
        return;
    }
    m_browser->setMarkdown(m_topics.at(row).markdown);
    m_browser->moveCursor(QTextCursor::Start);
    const QString needle = m_search->text().trimmed();
    if (!needle.isEmpty()) {
        m_browser->find(needle);
    }
}

void HelpWindow::navigateHistory(int offset) {
    const int next = m_historyPosition + offset;
    if (next < 0 || next >= m_history.size()) {
        return;
    }
    const int row = rowForTopic(m_history.at(next));
    if (row < 0) {
        return;
    }
    m_historyPosition = next;
    navigateToRow(row, false);
}

int HelpWindow::rowForTopic(const QString &requestedTopicId) const {
    for (int row = 0; row < m_topics.size(); ++row) {
        if (m_topics.at(row).id == requestedTopicId) {
            return row;
        }
    }
    return -1;
}

void HelpWindow::updateNavigationButtons() {
    m_back->setEnabled(m_historyPosition > 0);
    m_forward->setEnabled(m_historyPosition >= 0 && m_historyPosition + 1 < m_history.size());
}

void HelpWindow::handleLink(const QUrl &url) {
    const QString targetFile = QFileInfo(url.path()).fileName();
    QString id;
    if (targetFile.compare(QStringLiteral("USER_GUIDE.md"), Qt::CaseInsensitive) == 0) {
        id = topicId(url.fragment());
    } else if (targetFile.compare(QStringLiteral("PROTOCOL_UNITS.md"), Qt::CaseInsensitive) == 0) {
        id = QStringLiteral("protocol-") + topicId(url.fragment());
    } else if (targetFile.compare(QStringLiteral("DLZ_MODEL.md"), Qt::CaseInsensitive) == 0) {
        id = QStringLiteral("reference-dlz");
    } else if (targetFile.compare(QStringLiteral("TERRAIN_AND_ASSETS.md"), Qt::CaseInsensitive) ==
               0) {
        id = QStringLiteral("reference-terrain");
    } else if (targetFile.compare(QStringLiteral("THREAT_MODEL.md"), Qt::CaseInsensitive) == 0) {
        id = QStringLiteral("reference-security");
    } else if (targetFile.compare(QStringLiteral("DEVELOPER_GUIDE.md"), Qt::CaseInsensitive) == 0) {
        id = QStringLiteral("reference-development");
    } else if (targetFile.compare(QStringLiteral("README.md"), Qt::CaseInsensitive) == 0 &&
               url.path().contains(QStringLiteral("testsender"), Qt::CaseInsensitive)) {
        id = QStringLiteral("reference-test-sender");
    }
    const int row = rowForTopic(id);
    if (row >= 0) {
        navigateToRow(row, true);
        if (!url.fragment().isEmpty()) {
            m_browser->scrollToAnchor(url.fragment());
        }
        m_status->clear();
        return;
    }
    m_status->setText(QStringLiteral("That link belongs to the source-tree documentation and is "
                                     "not a separate in-app topic."));
}
