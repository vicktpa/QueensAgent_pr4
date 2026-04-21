#ifndef AGENTWINDOW_H
#define AGENTWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QTimer>
#include <QGroupBox>
#include <QComboBox>
#include <QSlider>
#include "AgentClient.h"

class AgentWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AgentWindow(int agentId, QWidget* parent = nullptr);
    ~AgentWindow();

    void setBrokerAddress(const QString& host, int port);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onStartSearchClicked();
    void onStopSearchClicked();
    void onStateChanged(AgentClient::State state);
    void onPositionChanged(int row);
    void onSolutionFound();
    void onConfigReceived(int boardSize, bool isFixed, int priority);
    void onLogMessage(const QString& message);
    void onError(const QString& message);
    void updateUI();
    void onSetPositionClicked();
    void clearLog();
    void onFixPositionClicked();
    void onUnfixPositionClicked();
    void onFixRequestAccepted(int row);
    void onFixRequestRejected(const QString& reason);
    void onPriorityChanged(int newPriority);  // НОВЫЙ СЛОТ
    void onSetPriorityClicked();              // НОВЫЙ СЛОТ

private:
    void setupUI();
    void appendLog(const QString& text);
    QString getStateString(AgentClient::State state) const;
    QString getPriorityDescription(int priority) const;

private:
    int m_agentId;
    AgentClient* m_client;

    QString m_defaultHost;
    int m_defaultPort;

    QLabel* m_titleLabel;
    QLabel* m_stateLabel;
    QLabel* m_positionLabel;
    QLabel* m_fixedLabel;
    QLabel* m_priorityLabel;
    QLabel* m_priorityValueLabel;

    QLineEdit* m_hostEdit;
    QSpinBox* m_portSpin;

    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QPushButton* m_startSearchButton;
    QPushButton* m_stopSearchButton;

    QSpinBox* m_rowSpin;
    QPushButton* m_setPositionButton;

    QGroupBox* m_fixGroup;
    QSpinBox* m_fixRowSpin;
    QPushButton* m_fixButton;
    QPushButton* m_unfixButton;
    QLabel* m_fixStatusLabel;

    // НОВЫЕ ЭЛЕМЕНТЫ ДЛЯ ПРИОРИТЕТОВ
    QGroupBox* m_priorityGroup;
    QComboBox* m_priorityCombo;
    QSlider* m_prioritySlider;
    QPushButton* m_setPriorityButton;
    QLabel* m_priorityDescLabel;

    QTextEdit* m_logEdit;
    QPushButton* m_clearLogButton;

    QWidget* m_positionIndicator;
};

#endif // AGENTWINDOW_H