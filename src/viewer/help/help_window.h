#pragma once

/**
 * @file help_window.h
 * @brief Searchable non-modal access to the application's bundled user documentation.
 */

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextBrowser;
class QUrl;

/** Presents resource-embedded documentation without interrupting application workflows. */
class HelpWindow final : public QDialog {
    Q_OBJECT

  public:
    explicit HelpWindow(QWidget *parent = nullptr);

    /** Supplies the primary-topic ID associated with the main window's current screen. */
    void setCurrentTopicResolver(std::function<QString()> resolver);
    /** Selects a topic by stable ID when it exists. */
    void showTopic(const QString &topicId);
    /** Returns the stable ID of the topic currently shown. */
    QString currentTopicId() const;

  private:
    struct Topic final {
        QString id;
        QString title;
        QString markdown;
    };

    static QString topicId(const QString &title);
    static QString readResource(const QString &path);
    void appendSectionTopics(const QString &resourcePath, const QString &idPrefix,
                             const QString &titlePrefix = {});
    void appendReferenceTopic(const QString &resourcePath, const QString &id, const QString &title);
    void applySearch(const QString &query);
    void navigateToRow(int row, bool addToHistory);
    void renderRow(int row);
    void navigateHistory(int offset);
    int rowForTopic(const QString &topicId) const;
    void updateNavigationButtons();
    void handleLink(const QUrl &url);

    QVector<Topic> m_topics;
    QStringList m_history;
    int m_historyPosition = -1;
    bool m_internalSelection = false;
    std::function<QString()> m_currentTopicResolver;
    QLineEdit *m_search = nullptr;
    QListWidget *m_topicList = nullptr;
    QTextBrowser *m_browser = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_back = nullptr;
    QPushButton *m_forward = nullptr;
};
