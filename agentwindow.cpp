#include "AgentWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCloseEvent>
#include <QMessageBox>
#include <QDateTime>

AgentWindow::AgentWindow(int agentId, QWidget* parent)
    : QMainWindow(parent)
    , m_agentId(agentId)
    , m_defaultHost("127.0.0.1")
    , m_defaultPort(12345)
{
    m_client = new AgentClient(agentId, this);

    setupUI();

    connect(m_client, &AgentClient::stateChanged, this, &AgentWindow::onStateChanged);
    connect(m_client, &AgentClient::positionChanged, this, &AgentWindow::onPositionChanged);
    connect(m_client, &AgentClient::solutionFound, this, &AgentWindow::onSolutionFound);
    connect(m_client, &AgentClient::configReceived, this, &AgentWindow::onConfigReceived);
    connect(m_client, &AgentClient::logMessage, this, &AgentWindow::onLogMessage);
    connect(m_client, &AgentClient::error, this, &AgentWindow::onError);
    connect(m_client, &AgentClient::connected, this, [this]() { updateUI(); });
    connect(m_client, &AgentClient::disconnected, this, [this]() { updateUI(); });
    connect(m_client, &AgentClient::fixRequestAccepted, this, &AgentWindow::onFixRequestAccepted);
    connect(m_client, &AgentClient::fixRequestRejected, this, &AgentWindow::onFixRequestRejected);
    connect(m_client, &AgentClient::priorityChanged, this, &AgentWindow::onPriorityChanged);

    setWindowTitle(QString("Агент %1 - Задача о ферзях").arg(QChar('A' + agentId)));
    resize(450, 750);

    updateUI();
    appendLog(QString("Агент %1 запущен").arg(QChar('A' + agentId)));
}

AgentWindow::~AgentWindow()
{
    m_client->disconnectFromBroker();
}

void AgentWindow::setBrokerAddress(const QString& host, int port)
{
    m_defaultHost = host;
    m_defaultPort = port;
    m_hostEdit->setText(host);
    m_portSpin->setValue(port);
}

void AgentWindow::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // Заголовок
    QHBoxLayout* titleLayout = new QHBoxLayout();
    m_titleLabel = new QLabel(QString("АГЕНТ %1").arg(QChar('A' + m_agentId)), this);
    m_titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #2c3e50;");
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    mainLayout->addLayout(titleLayout);

    // Индикатор позиции
    m_positionIndicator = new QWidget(this);
    m_positionIndicator->setMinimumHeight(80);
    m_positionIndicator->setStyleSheet("background-color: #34495e; border-radius: 10px;");

    QHBoxLayout* indicatorLayout = new QHBoxLayout(m_positionIndicator);
    m_positionLabel = new QLabel("—", this);
    m_positionLabel->setStyleSheet("font-size: 48px; font-weight: bold; color: white;");
    m_positionLabel->setAlignment(Qt::AlignCenter);
    indicatorLayout->addWidget(m_positionLabel);
    mainLayout->addWidget(m_positionIndicator);

    // Информация
    QGroupBox* infoGroup = new QGroupBox("Информация", this);
    QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);
    m_stateLabel = new QLabel("Состояние: Отключен", this);
    m_fixedLabel = new QLabel("Фиксация: Нет", this);

    QHBoxLayout* priorityInfoLayout = new QHBoxLayout();
    priorityInfoLayout->addWidget(new QLabel("Приоритет:", this));
    m_priorityValueLabel = new QLabel("0 (нет)", this);
    m_priorityValueLabel->setStyleSheet("font-weight: bold;");
    priorityInfoLayout->addWidget(m_priorityValueLabel);
    priorityInfoLayout->addStretch();

    infoLayout->addWidget(m_stateLabel);
    infoLayout->addWidget(m_fixedLabel);
    infoLayout->addLayout(priorityInfoLayout);
    mainLayout->addWidget(infoGroup);

    // Подключение к брокеру
    QGroupBox* connectionGroup = new QGroupBox("Подключение к брокеру", this);
    QVBoxLayout* connectionLayout = new QVBoxLayout(connectionGroup);

    QHBoxLayout* hostLayout = new QHBoxLayout();
    hostLayout->addWidget(new QLabel("Хост:", this));
    m_hostEdit = new QLineEdit(m_defaultHost, this);
    hostLayout->addWidget(m_hostEdit);
    connectionLayout->addLayout(hostLayout);

    QHBoxLayout* portLayout = new QHBoxLayout();
    portLayout->addWidget(new QLabel("Порт:", this));
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(m_defaultPort);
    portLayout->addWidget(m_portSpin);
    portLayout->addStretch();
    connectionLayout->addLayout(portLayout);

    QHBoxLayout* connButtonLayout = new QHBoxLayout();
    m_connectButton = new QPushButton("Подключиться", this);
    m_disconnectButton = new QPushButton("Отключиться", this);
    m_disconnectButton->setEnabled(false);
    connButtonLayout->addWidget(m_connectButton);
    connButtonLayout->addWidget(m_disconnectButton);
    connectionLayout->addLayout(connButtonLayout);

    connect(m_connectButton, &QPushButton::clicked, this, &AgentWindow::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &AgentWindow::onDisconnectClicked);

    mainLayout->addWidget(connectionGroup);

    // Группа приоритетов (НОВАЯ)
    m_priorityGroup = new QGroupBox("Настройка приоритета", this);
    QVBoxLayout* priorityLayout = new QVBoxLayout(m_priorityGroup);

    // Пояснение
    QLabel* priorityHint = new QLabel("Чем МЕНЬШЕ число, тем ВЫШЕ приоритет", this);
    priorityHint->setStyleSheet("color: #7f8c8d; font-size: 10px;");
    priorityLayout->addWidget(priorityHint);

    // Выбор приоритета через ComboBox
    QHBoxLayout* comboLayout = new QHBoxLayout();
    comboLayout->addWidget(new QLabel("Приоритет:", this));
    m_priorityCombo = new QComboBox(this);
    m_priorityCombo->addItem("0 - нет приоритета", 0);
    m_priorityCombo->addItem("1 - ★★★ НАИВЫСШИЙ", 1);
    m_priorityCombo->addItem("2 - ★★ Высокий", 2);
    m_priorityCombo->addItem("3 - ★ Выше среднего", 3);
    m_priorityCombo->addItem("4 - Средний", 4);
    m_priorityCombo->addItem("5 - Ниже среднего", 5);
    m_priorityCombo->addItem("6 - Низкий", 6);
    m_priorityCombo->addItem("7 - Очень низкий", 7);
    m_priorityCombo->addItem("8 - Минимальный", 8);
    m_priorityCombo->addItem("9 - Самый низкий", 9);
    m_priorityCombo->addItem("10 - ★ НИЗШИЙ", 10);
    comboLayout->addWidget(m_priorityCombo);
    comboLayout->addStretch();
    priorityLayout->addLayout(comboLayout);

    // Слайдер для выбора приоритета
    QHBoxLayout* sliderLayout = new QHBoxLayout();
    sliderLayout->addWidget(new QLabel("Высокий", this));
    m_prioritySlider = new QSlider(Qt::Horizontal, this);
    m_prioritySlider->setRange(1, 10);
    m_prioritySlider->setValue(5);
    sliderLayout->addWidget(m_prioritySlider);
    sliderLayout->addWidget(new QLabel("Низкий", this));
    priorityLayout->addLayout(sliderLayout);

    // Описание текущего приоритета
    m_priorityDescLabel = new QLabel("Текущий приоритет: 5 (средний)", this);
    m_priorityDescLabel->setStyleSheet("color: #3498db;");
    priorityLayout->addWidget(m_priorityDescLabel);

    // Кнопка установки приоритета
    m_setPriorityButton = new QPushButton("Установить приоритет", this);
    priorityLayout->addWidget(m_setPriorityButton);

    mainLayout->addWidget(m_priorityGroup);

    // Связываем слайдер и комбобокс
    connect(m_prioritySlider, &QSlider::valueChanged, [this](int value) {
        m_priorityCombo->setCurrentIndex(value);
        m_priorityDescLabel->setText(QString("Текущий приоритет: %1 (%2)")
                                         .arg(value)
                                         .arg(getPriorityDescription(value)));
    });

    connect(m_priorityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        int value = m_priorityCombo->itemData(index).toInt();
        m_prioritySlider->setValue(value);
    });

    connect(m_setPriorityButton, &QPushButton::clicked, this, &AgentWindow::onSetPriorityClicked);

    // Управление
    QGroupBox* controlGroup = new QGroupBox("Управление", this);
    QVBoxLayout* controlLayout = new QVBoxLayout(controlGroup);

    QHBoxLayout* searchButtonLayout = new QHBoxLayout();
    m_startSearchButton = new QPushButton("Начать поиск", this);
    m_stopSearchButton = new QPushButton("Остановить", this);
    m_startSearchButton->setEnabled(false);
    m_stopSearchButton->setEnabled(false);
    searchButtonLayout->addWidget(m_startSearchButton);
    searchButtonLayout->addWidget(m_stopSearchButton);
    controlLayout->addLayout(searchButtonLayout);

    connect(m_startSearchButton, &QPushButton::clicked, this, &AgentWindow::onStartSearchClicked);
    connect(m_stopSearchButton, &QPushButton::clicked, this, &AgentWindow::onStopSearchClicked);

    QHBoxLayout* positionLayout = new QHBoxLayout();
    positionLayout->addWidget(new QLabel("Строка (1-N):", this));
    m_rowSpin = new QSpinBox(this);
    m_rowSpin->setRange(1, 8);
    positionLayout->addWidget(m_rowSpin);
    m_setPositionButton = new QPushButton("Установить", this);
    m_setPositionButton->setEnabled(false);
    positionLayout->addWidget(m_setPositionButton);
    controlLayout->addLayout(positionLayout);

    connect(m_setPositionButton, &QPushButton::clicked, this, &AgentWindow::onSetPositionClicked);

    mainLayout->addWidget(controlGroup);

    // Фиксация позиции
    m_fixGroup = new QGroupBox("Фиксация позиции", this);
    QVBoxLayout* fixLayout = new QVBoxLayout(m_fixGroup);

    QHBoxLayout* fixRowLayout = new QHBoxLayout();
    fixRowLayout->addWidget(new QLabel("Зафиксировать строку:", this));
    m_fixRowSpin = new QSpinBox(this);
    m_fixRowSpin->setRange(1, 8);
    m_fixRowSpin->setValue(1);
    fixRowLayout->addWidget(m_fixRowSpin);
    fixRowLayout->addStretch();
    fixLayout->addLayout(fixRowLayout);

    QHBoxLayout* fixButtonLayout = new QHBoxLayout();
    m_fixButton = new QPushButton("🔒 Зафиксировать", this);
    m_unfixButton = new QPushButton("🔓 Снять фиксацию", this);
    m_unfixButton->setEnabled(false);
    fixButtonLayout->addWidget(m_fixButton);
    fixButtonLayout->addWidget(m_unfixButton);
    fixLayout->addLayout(fixButtonLayout);

    m_fixStatusLabel = new QLabel("Статус: не зафиксирован", this);
    m_fixStatusLabel->setStyleSheet("color: gray;");
    fixLayout->addWidget(m_fixStatusLabel);

    mainLayout->addWidget(m_fixGroup);

    // Лог
    QGroupBox* logGroup = new QGroupBox("Лог", this);
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);

    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(200);
    m_logEdit->setFont(QFont("Consolas", 8));
    logLayout->addWidget(m_logEdit);

    QHBoxLayout* logButtonLayout = new QHBoxLayout();
    m_clearLogButton = new QPushButton("Очистить лог", this);
    logButtonLayout->addStretch();
    logButtonLayout->addWidget(m_clearLogButton);
    logLayout->addLayout(logButtonLayout);

    connect(m_clearLogButton, &QPushButton::clicked, this, &AgentWindow::clearLog);

    mainLayout->addWidget(logGroup);

    // Подключение сигналов
    connect(m_fixButton, &QPushButton::clicked, this, &AgentWindow::onFixPositionClicked);
    connect(m_unfixButton, &QPushButton::clicked, this, &AgentWindow::onUnfixPositionClicked);
}

void AgentWindow::closeEvent(QCloseEvent* event)
{
    m_client->disconnectFromBroker();
    event->accept();
}

void AgentWindow::onConnectClicked()
{
    QString host = m_hostEdit->text();
    int port = m_portSpin->value();
    m_client->connectToBroker(host, port);
}

void AgentWindow::onDisconnectClicked()
{
    m_client->disconnectFromBroker();
}

void AgentWindow::onStartSearchClicked()
{
    m_client->startSearch();
}

void AgentWindow::onStopSearchClicked()
{
    m_client->stopSearch();
}

void AgentWindow::onSetPositionClicked()
{
    int row = m_rowSpin->value() - 1;
    m_client->setCurrentRow(row);
}

void AgentWindow::onFixPositionClicked()
{
    int row = m_fixRowSpin->value() - 1;
    m_client->requestFixPosition(row);
}

void AgentWindow::onUnfixPositionClicked()
{
    m_client->requestUnfixPosition();
}

void AgentWindow::onFixRequestAccepted(int row)
{
    if (row >= 0) {
        // Фиксация установлена
        m_fixStatusLabel->setText(QString("Статус: ЗАФИКСИРОВАН (строка %1)").arg(row + 1));
        m_fixStatusLabel->setStyleSheet("color: #27ae60; font-weight: bold;");
        m_fixButton->setEnabled(false);
        m_unfixButton->setEnabled(true);
        m_fixRowSpin->setEnabled(false);
        m_setPositionButton->setEnabled(false);
        m_rowSpin->setEnabled(false);
        m_startSearchButton->setEnabled(false);
        m_setPriorityButton->setEnabled(false);
        m_priorityCombo->setEnabled(false);
        m_prioritySlider->setEnabled(false);
        appendLog(QString("✅ Позиция зафиксирована на строке %1").arg(row + 1));
    } else {
        // Фиксация снята (row == -1)
        m_fixStatusLabel->setText("Статус: не зафиксирован");
        m_fixStatusLabel->setStyleSheet("color: gray;");
        m_fixButton->setEnabled(true);
        m_unfixButton->setEnabled(false);
        m_fixRowSpin->setEnabled(true);
        m_setPositionButton->setEnabled(true);
        m_rowSpin->setEnabled(true);
        m_startSearchButton->setEnabled(true);
        m_setPriorityButton->setEnabled(true);
        m_priorityCombo->setEnabled(true);
        m_prioritySlider->setEnabled(true);
        appendLog("🔓 Фиксация снята");
    }
}

void AgentWindow::onFixRequestRejected(const QString& reason)
{
    appendLog(QString("❌ Отказ в фиксации: %1").arg(reason));
    QMessageBox::warning(this, "Фиксация отклонена", reason);
}

void AgentWindow::onStateChanged(AgentClient::State state)
{
    updateUI();

    bool isRegistered = (state == AgentClient::REGISTERED);
    bool isFixed = m_client->isFixed();

    m_fixButton->setEnabled(isRegistered && !isFixed);
    m_unfixButton->setEnabled(isRegistered && isFixed);
    m_fixRowSpin->setEnabled(isRegistered && !isFixed);

    appendLog(QString("Состояние: %1").arg(getStateString(state)));
}

void AgentWindow::onPositionChanged(int row)
{
    if (row >= 0) {
        m_positionLabel->setText(QString::number(row + 1));
        m_rowSpin->setValue(row + 1);
        appendLog(QString("Позиция изменена: строка %1").arg(row + 1));
    } else {
        m_positionLabel->setText("—");
        appendLog("Позиция сброшена");
    }
}

void AgentWindow::onSolutionFound()
{
    appendLog("★★★ РЕШЕНИЕ НАЙДЕНО! ★★★");
    m_positionIndicator->setStyleSheet("background-color: #27ae60; border-radius: 10px;");
    QTimer::singleShot(1000, [this]() {
        m_positionIndicator->setStyleSheet("background-color: #34495e; border-radius: 10px;");
    });
}

void AgentWindow::onConfigReceived(int boardSize, bool isFixed, int priority)
{
    m_rowSpin->setRange(1, boardSize);
    m_fixRowSpin->setRange(1, boardSize);

    // Обновляем отображение приоритета
    m_priorityValueLabel->setText(QString("%1 (%2)")
                                      .arg(priority)
                                      .arg(getPriorityDescription(priority)));
    m_prioritySlider->setValue(priority == 0 ? 5 : priority);

    // Находим индекс в комбобоксе
    int comboIndex = m_priorityCombo->findData(priority);
    if (comboIndex >= 0) {
        m_priorityCombo->setCurrentIndex(comboIndex);
    }

    if (isFixed) {
        int fixedRow = m_client->getFixedRow().value_or(0);
        m_fixedLabel->setText(QString("Фиксация: ДА (строка %1)").arg(fixedRow + 1));
        m_fixStatusLabel->setText(QString("Статус: ЗАФИКСИРОВАН (строка %1)").arg(fixedRow + 1));
        m_fixStatusLabel->setStyleSheet("color: #27ae60; font-weight: bold;");
        m_fixButton->setEnabled(false);
        m_unfixButton->setEnabled(true);
        m_fixRowSpin->setEnabled(false);
        m_setPositionButton->setEnabled(false);
        m_rowSpin->setEnabled(false);
        m_startSearchButton->setEnabled(false);
        m_setPriorityButton->setEnabled(false);
        m_priorityCombo->setEnabled(false);
        m_prioritySlider->setEnabled(false);
    } else {
        m_fixedLabel->setText("Фиксация: Нет");
        m_fixStatusLabel->setText("Статус: не зафиксирован");
        m_fixStatusLabel->setStyleSheet("color: gray;");
        m_fixButton->setEnabled(true);
        m_unfixButton->setEnabled(false);
        m_fixRowSpin->setEnabled(true);
        m_setPositionButton->setEnabled(true);
        m_rowSpin->setEnabled(true);
        m_startSearchButton->setEnabled(true);
        m_setPriorityButton->setEnabled(true);
        m_priorityCombo->setEnabled(true);
        m_prioritySlider->setEnabled(true);
    }

    updateUI();
}

void AgentWindow::onLogMessage(const QString& message)
{
    appendLog(message);
}

void AgentWindow::onError(const QString& message)
{
    appendLog(QString("ОШИБКА: %1").arg(message));
    QMessageBox::warning(this, "Ошибка", message);
}

void AgentWindow::updateUI()
{
    bool isConnected = m_client->isConnected();
    AgentClient::State state = m_client->getState();

    m_connectButton->setEnabled(!isConnected);
    m_disconnectButton->setEnabled(isConnected);
    m_hostEdit->setEnabled(!isConnected);
    m_portSpin->setEnabled(!isConnected);

    bool canSearch = (state == AgentClient::REGISTERED) && !m_client->isFixed();
    bool isSearching = (state == AgentClient::SEARCHING || state == AgentClient::WAITING);

    m_startSearchButton->setEnabled(canSearch && !isSearching);
    m_stopSearchButton->setEnabled(isSearching);

    m_stateLabel->setText(QString("Состояние: %1").arg(getStateString(state)));

    switch (state) {
    case AgentClient::DISCONNECTED:
        m_stateLabel->setStyleSheet("color: #95a5a6;");
        break;
    case AgentClient::CONNECTING:
        m_stateLabel->setStyleSheet("color: #f39c12;");
        break;
    case AgentClient::REGISTERED:
        m_stateLabel->setStyleSheet("color: #27ae60;");
        break;
    case AgentClient::SEARCHING:
        m_stateLabel->setStyleSheet("color: #3498db; font-weight: bold;");
        break;
    case AgentClient::WAITING:
        m_stateLabel->setStyleSheet("color: #9b59b6;");
        break;
    case AgentClient::SOLUTION_FOUND:
        m_stateLabel->setStyleSheet("color: #27ae60; font-weight: bold;");
        break;
    case AgentClient::ERROR_STATE:
        m_stateLabel->setStyleSheet("color: #e74c3c; font-weight: bold;");
        break;
    }
}

void AgentWindow::appendLog(const QString& text)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_logEdit->append(QString("[%1] %2").arg(timestamp).arg(text));
}

void AgentWindow::clearLog()
{
    m_logEdit->clear();
    appendLog("Лог очищен");
}

QString AgentWindow::getStateString(AgentClient::State state) const
{
    switch (state) {
    case AgentClient::DISCONNECTED: return "Отключен";
    case AgentClient::CONNECTING: return "Подключение...";
    case AgentClient::REGISTERED: return "Зарегистрирован";
    case AgentClient::SEARCHING: return "Поиск...";
    case AgentClient::WAITING: return "Ожидание...";
    case AgentClient::SOLUTION_FOUND: return "Решение найдено!";
    case AgentClient::ERROR_STATE: return "Ошибка";
    default: return "Неизвестно";
    }
}

QString AgentWindow::getPriorityDescription(int priority) const
{
    switch(priority) {
    case 0: return "нет приоритета";
    case 1: return "НАИВЫСШИЙ ★★★";
    case 2: return "высокий ★★";
    case 3: return "выше среднего ★";
    case 4: return "средний";
    case 5: return "ниже среднего";
    case 6: return "низкий";
    case 7: return "очень низкий";
    case 8: return "минимальный";
    case 9: return "самый низкий";
    case 10: return "НИЗШИЙ ★";
    default: return "";
    }
}

void AgentWindow::onPriorityChanged(int newPriority)
{
    // Приоритет хранится в m_client, не нужно дублировать
    m_priorityValueLabel->setText(QString("%1 (%2)")
                                      .arg(newPriority)
                                      .arg(getPriorityDescription(newPriority)));

    // Цветовая индикация
    if (newPriority == 1) {
        m_priorityValueLabel->setStyleSheet("font-weight: bold; color: #e74c3c;");
        m_priorityValueLabel->setToolTip("Наивысший приоритет ★★★");
    } else if (newPriority == 2) {
        m_priorityValueLabel->setStyleSheet("font-weight: bold; color: #e67e22;");
        m_priorityValueLabel->setToolTip("Высокий приоритет ★★");
    } else if (newPriority == 3) {
        m_priorityValueLabel->setStyleSheet("font-weight: bold; color: #f39c12;");
        m_priorityValueLabel->setToolTip("Приоритет выше среднего ★");
    } else if (newPriority >= 4 && newPriority <= 7) {
        m_priorityValueLabel->setStyleSheet("color: #27ae60;");
        m_priorityValueLabel->setToolTip("Средний приоритет");
    } else if (newPriority >= 8) {
        m_priorityValueLabel->setStyleSheet("color: #95a5a6;");
        m_priorityValueLabel->setToolTip("Низкий приоритет");
    }

    // Обновляем слайдер и комбобокс
    m_prioritySlider->setValue(newPriority);
    int comboIndex = m_priorityCombo->findData(newPriority);
    if (comboIndex >= 0) {
        m_priorityCombo->setCurrentIndex(comboIndex);
    }

    appendLog(QString("Приоритет обновлен: %1 %2")
                  .arg(newPriority)
                  .arg(getPriorityDescription(newPriority)));
}

void AgentWindow::onSetPriorityClicked()
{
    int priority = m_prioritySlider->value();
    m_client->requestSetPriority(priority);
    appendLog(QString("Запрос на установку приоритета %1 отправлен...").arg(priority));
}


