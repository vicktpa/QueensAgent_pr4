#ifndef BROKERWINDOW_H
#define BROKERWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QTableWidget>
#include <QProgressBar>
#include <QTabWidget>
#include <QSplitter>
#include <QCheckBox>
#include <QProcess>
#include <QList>
#include <QTimer>  // ДОБАВИТЬ

#include "Broker.h"
#include "ChessBoardWidget.h"
#include "KnowledgeBase.h"
#include "PriorityManager.h"

class BrokerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit BrokerWindow(QWidget* parent = nullptr);
    ~BrokerWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onStartServerClicked();
    void onStopServerClicked();
    void onStartSearchClicked();
    void onStopSearchClicked();
    void onOpenKnowledgeBaseDialog();
    void onLaunchAllAgentsClicked();
    void onAgentConnected(int agentId);
    void onAgentDisconnected(int agentId);
    void onAgentPositionChanged(int agentId, int row);
    void onSolutionFound(const std::vector<int>& solution);
    void onSearchFinished(int totalSolutions);
    void onLogMessage(const QString& message);
    void onError(const QString& message);
    void onProgressUpdated(int solutionsFound, int backtracks);
    void updateAgentsTable();
    void clearLog();

    // Новые слоты для навигации по решениям
    void onFirstSolutionClicked();
    void onPreviousSolutionClicked();
    void onNextSolutionClicked();
    void onShowAllSolutionsClicked();
    void onStopAnimationClicked();

private:
    void setupUI();
    void appendLog(const QString& text);
    QString formatSolution(const std::vector<int>& solution) const;
    void initKnowledgeBase();
    void initPriorityManager();
    QString findAgentExecutable();
    void displaySolution(int index);
    void animateSolutions(int index);
    void appendToBoardVisualization(const std::vector<int>& solution);
    void updateNavigationButtons();
    void stopAnimation();  // ДОБАВИТЬ ЭТУ СТРОКУ

private:
    Broker* m_broker;
    ChessBoardWidget* m_boardWidget;
    KnowledgeBase* m_knowledgeBase;
    PriorityManager* m_priorityManager;

    QTabWidget* m_tabWidget;
    QWidget* m_mainTab;

    QLabel* m_serverStatusLabel;
    QSpinBox* m_portSpin;
    QSpinBox* m_boardSizeSpin;
    QCheckBox* m_autoLaunchAgentsCheckBox;
    QPushButton* m_startServerButton;
    QPushButton* m_stopServerButton;
    QPushButton* m_knowledgeBaseButton;
    QPushButton* m_launchAllAgentsButton;

    QTableWidget* m_agentsTable;

    QPushButton* m_startSearchButton;
    QPushButton* m_stopSearchButton;

    QLabel* m_solutionsCountLabel;
    QLabel* m_backtracksLabel;
    QProgressBar* m_progressBar;

    QTextEdit* m_solutionsEdit;
    QTextEdit* m_logEdit;

    QPushButton* m_clearLogButton;

    // Новые кнопки навигации
    QPushButton* m_firstSolutionButton;
    QPushButton* m_prevSolutionButton;
    QPushButton* m_nextSolutionButton;
    QPushButton* m_showAllButton;
    QPushButton* m_stopAnimationButton;

    QList<QProcess*> m_localAgentProcesses;

    // Для навигации по решениям
    std::vector<std::vector<int>> m_allSolutions;
    int m_currentSolutionIndex;
    bool m_animationRunning;
    int m_animationIndex;
};

#endif // BROKERWINDOW_H