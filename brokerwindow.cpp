#include "BrokerWindow.h"
#include "KnowledgeBaseDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QCloseEvent>
#include <QMessageBox>
#include <QDateTime>
#include <QProcess>
#include <QThread>
#include <QFile>
#include <QCheckBox>
#include <QTimer>
#include <QApplication>
#include <QScrollBar>
#include <QDir>
#include <QFileInfo>

BrokerWindow::BrokerWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_broker(nullptr)
    , m_boardWidget(nullptr)
    , m_knowledgeBase(nullptr)
    , m_priorityManager(nullptr)
    , m_currentSolutionIndex(-1)
    , m_animationRunning(false)
    , m_animationIndex(0)
{
    initKnowledgeBase();
    initPriorityManager();

    m_broker = new Broker(this);
    m_broker->setKnowledgeBase(m_knowledgeBase);
    m_broker->setPriorityManager(m_priorityManager);

    setupUI();

    // Подключение сигналов брокера
    connect(m_broker, &Broker::agentConnected, this, &BrokerWindow::onAgentConnected);
    connect(m_broker, &Broker::agentDisconnected, this, &BrokerWindow::onAgentDisconnected);
    connect(m_broker, &Broker::agentPositionChanged, this, &BrokerWindow::onAgentPositionChanged);
    connect(m_broker, &Broker::solutionFound, this, &BrokerWindow::onSolutionFound);
    connect(m_broker, &Broker::searchFinished, this, &BrokerWindow::onSearchFinished);
    connect(m_broker, &Broker::logMessage, this, &BrokerWindow::onLogMessage);
    connect(m_broker, &Broker::error, this, &BrokerWindow::onError);
    connect(m_broker, &Broker::progressUpdated, this, &BrokerWindow::onProgressUpdated);

    if (m_knowledgeBase) {
        connect(m_knowledgeBase, &KnowledgeBase::knowledgeUpdated, [this]() {
            if (m_boardWidget) m_boardWidget->refreshDisplay();
        });
    }

    setWindowTitle("Брокер - Координатор распределенной системы");
    resize(1100, 800);

    appendLog("Брокер запущен. Настройте параметры и нажмите 'Запустить сервер'");
}

BrokerWindow::~BrokerWindow()
{
    for (QProcess* proc : m_localAgentProcesses) {
        if (proc->state() == QProcess::Running) {
            proc->terminate();
            proc->waitForFinished(3000);
        }
        proc->deleteLater();
    }
    m_localAgentProcesses.clear();
}

void BrokerWindow::initKnowledgeBase()
{
    m_knowledgeBase = new KnowledgeBase(this);
    m_knowledgeBase->initializeDatabase();
}

void BrokerWindow::initPriorityManager()
{
    m_priorityManager = new PriorityManager(this);
}

void BrokerWindow::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget);

    m_mainTab = new QWidget();
    QVBoxLayout* mainTabLayout = new QVBoxLayout(m_mainTab);

    QSplitter* splitter = new QSplitter(Qt::Horizontal);

    // ========================================================================
    // ЛЕВАЯ ПАНЕЛЬ - Шахматная доска
    // ========================================================================
    QWidget* leftPanel = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);

    m_boardWidget = new ChessBoardWidget(this);
    m_boardWidget->setMinimumSize(400, 400);
    m_boardWidget->setKnowledgeBase(m_knowledgeBase);
    m_boardWidget->setPriorityManager(m_priorityManager);
    leftLayout->addWidget(m_boardWidget);

    splitter->addWidget(leftPanel);

    // ========================================================================
    // ПРАВАЯ ПАНЕЛЬ - Управление
    // ========================================================================
    QWidget* rightPanel = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);

    // Группа настроек сервера
    QGroupBox* serverGroup = new QGroupBox("Настройки сервера", this);
    QVBoxLayout* serverMainLayout = new QVBoxLayout(serverGroup);

    QHBoxLayout* statusLayout = new QHBoxLayout();
    m_serverStatusLabel = new QLabel("Сервер: Остановлен", this);
    m_serverStatusLabel->setStyleSheet("color: #e74c3c; font-weight: bold;");
    statusLayout->addWidget(m_serverStatusLabel);
    statusLayout->addStretch();
    serverMainLayout->addLayout(statusLayout);

    QHBoxLayout* settingsLayout = new QHBoxLayout();
    settingsLayout->addWidget(new QLabel("Порт:", this));
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(12345);
    settingsLayout->addWidget(m_portSpin);

    settingsLayout->addWidget(new QLabel("N:", this));
    m_boardSizeSpin = new QSpinBox(this);
    m_boardSizeSpin->setRange(4, 10);
    m_boardSizeSpin->setValue(8);
    settingsLayout->addWidget(m_boardSizeSpin);
    settingsLayout->addStretch();
    serverMainLayout->addLayout(settingsLayout);

    m_autoLaunchAgentsCheckBox = new QCheckBox("Автоматически запускать агентов", this);
    m_autoLaunchAgentsCheckBox->setChecked(false);
    serverMainLayout->addWidget(m_autoLaunchAgentsCheckBox);

    QHBoxLayout* serverButtonLayout = new QHBoxLayout();
    m_startServerButton = new QPushButton("Запустить сервер", this);
    m_stopServerButton = new QPushButton("Остановить сервер", this);
    m_stopServerButton->setEnabled(false);
    serverButtonLayout->addWidget(m_startServerButton);
    serverButtonLayout->addWidget(m_stopServerButton);
    serverMainLayout->addLayout(serverButtonLayout);

    m_launchAllAgentsButton = new QPushButton("Запустить всех агентов локально", this);
    m_launchAllAgentsButton->setEnabled(false);
    serverMainLayout->addWidget(m_launchAllAgentsButton);

    rightLayout->addWidget(serverGroup);

    // Кнопка базы знаний
    m_knowledgeBaseButton = new QPushButton("Управление базой знаний", this);
    connect(m_knowledgeBaseButton, &QPushButton::clicked, this, &BrokerWindow::onOpenKnowledgeBaseDialog);
    rightLayout->addWidget(m_knowledgeBaseButton);

    // Таблица агентов
    QGroupBox* agentsGroup = new QGroupBox("Подключенные агенты", this);
    QVBoxLayout* agentsLayout = new QVBoxLayout(agentsGroup);

    m_agentsTable = new QTableWidget(0, 5, this);
    m_agentsTable->setHorizontalHeaderLabels({"ID", "Столбец", "Строка", "Фиксация", "Приоритет"});
    m_agentsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_agentsTable->setEditTriggers(QTableWidget::NoEditTriggers);
    agentsLayout->addWidget(m_agentsTable);

    rightLayout->addWidget(agentsGroup);

    // Группа поиска
    QGroupBox* searchGroup = new QGroupBox("Поиск решений", this);
    QVBoxLayout* searchLayout = new QVBoxLayout(searchGroup);

    QHBoxLayout* searchButtonLayout = new QHBoxLayout();
    m_startSearchButton = new QPushButton("Начать поиск", this);
    m_stopSearchButton = new QPushButton("Остановить", this);
    m_startSearchButton->setEnabled(false);
    m_stopSearchButton->setEnabled(false);
    searchButtonLayout->addWidget(m_startSearchButton);
    searchButtonLayout->addWidget(m_stopSearchButton);
    searchLayout->addLayout(searchButtonLayout);

    QHBoxLayout* countersLayout = new QHBoxLayout();
    m_solutionsCountLabel = new QLabel("Решений: 0", this);
    //m_backtracksLabel = new QLabel("Откатов: 0", this);
    countersLayout->addWidget(m_solutionsCountLabel);
    //countersLayout->addWidget(m_backtracksLabel);
    countersLayout->addStretch();
    searchLayout->addLayout(countersLayout);

    rightLayout->addWidget(searchGroup);

    // ========================================================================
    // ГРУППА НАВИГАЦИИ ПО РЕШЕНИЯМ
    // ========================================================================
    QGroupBox* navigationGroup = new QGroupBox("Навигация по решениям", this);
    QVBoxLayout* navigationLayout = new QVBoxLayout(navigationGroup);

    // Первая строка кнопок
    QHBoxLayout* navRow1 = new QHBoxLayout();
    m_firstSolutionButton = new QPushButton("Первое решение", this);
    m_prevSolutionButton = new QPushButton("Предыдущее", this);
    m_nextSolutionButton = new QPushButton("Следующее", this);

    m_firstSolutionButton->setEnabled(false);
    m_prevSolutionButton->setEnabled(false);
    m_nextSolutionButton->setEnabled(false);

    navRow1->addWidget(m_firstSolutionButton);
    navRow1->addWidget(m_prevSolutionButton);
    navRow1->addWidget(m_nextSolutionButton);
    navigationLayout->addLayout(navRow1);

    // Вторая строка кнопок
    QHBoxLayout* navRow2 = new QHBoxLayout();
    m_showAllButton = new QPushButton("Показать все", this);
    m_stopAnimationButton = new QPushButton("Остановить показ", this);
    m_showAllButton->setEnabled(false);
    m_stopAnimationButton->setEnabled(false);

    navRow2->addWidget(m_showAllButton);
    navRow2->addWidget(m_stopAnimationButton);
    navigationLayout->addLayout(navRow2);

    rightLayout->addWidget(navigationGroup);

    // Прогресс-бар
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    rightLayout->addWidget(m_progressBar);

    rightLayout->addStretch();

    splitter->addWidget(rightPanel);
    splitter->setSizes({500, 350});

    mainTabLayout->addWidget(splitter);

    // Группа с найденными решениями
    QGroupBox* solutionsGroup = new QGroupBox("Найденные решения", this);
    QVBoxLayout* solutionsLayout = new QVBoxLayout(solutionsGroup);
    m_solutionsEdit = new QTextEdit(this);
    m_solutionsEdit->setReadOnly(true);
    m_solutionsEdit->setMaximumHeight(120);
    m_solutionsEdit->setFont(QFont("Courier New", 9));
    solutionsLayout->addWidget(m_solutionsEdit);
    mainTabLayout->addWidget(solutionsGroup);

    m_tabWidget->addTab(m_mainTab, "Основное");

    // Вкладка с логом
    QWidget* logTab = new QWidget();
    QVBoxLayout* logTabLayout = new QVBoxLayout(logTab);

    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setFont(QFont("Monaco", 9));
    logTabLayout->addWidget(m_logEdit);

    QHBoxLayout* logButtonLayout = new QHBoxLayout();
    m_clearLogButton = new QPushButton("Очистить лог", this);
    connect(m_clearLogButton, &QPushButton::clicked, this, &BrokerWindow::clearLog);
    logButtonLayout->addStretch();
    logButtonLayout->addWidget(m_clearLogButton);
    logTabLayout->addLayout(logButtonLayout);

    m_tabWidget->addTab(logTab, "Лог");

    // ========================================================================
    // ПОДКЛЮЧЕНИЕ СИГНАЛОВ
    // ========================================================================
    connect(m_startServerButton, &QPushButton::clicked, this, &BrokerWindow::onStartServerClicked);
    connect(m_stopServerButton, &QPushButton::clicked, this, &BrokerWindow::onStopServerClicked);
    connect(m_startSearchButton, &QPushButton::clicked, this, &BrokerWindow::onStartSearchClicked);
    connect(m_stopSearchButton, &QPushButton::clicked, this, &BrokerWindow::onStopSearchClicked);
    connect(m_launchAllAgentsButton, &QPushButton::clicked, this, &BrokerWindow::onLaunchAllAgentsClicked);

    // Подключение кнопок навигации
    connect(m_firstSolutionButton, &QPushButton::clicked, this, &BrokerWindow::onFirstSolutionClicked);
    connect(m_prevSolutionButton, &QPushButton::clicked, this, &BrokerWindow::onPreviousSolutionClicked);
    connect(m_nextSolutionButton, &QPushButton::clicked, this, &BrokerWindow::onNextSolutionClicked);
    connect(m_showAllButton, &QPushButton::clicked, this, &BrokerWindow::onShowAllSolutionsClicked);
    connect(m_stopAnimationButton, &QPushButton::clicked, this, &BrokerWindow::onStopAnimationClicked);

    connect(m_boardSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int size) {
                if (m_boardWidget) m_boardWidget->setBoardSize(size);
                if (m_broker) m_broker->setBoardSize(size);
            });
}

// ========================================================================
// РЕАЛИЗАЦИЯ КНОПОК НАВИГАЦИИ
// ========================================================================

void BrokerWindow::onFirstSolutionClicked()
{
    stopAnimation();
    if (!m_allSolutions.empty()) {
        displaySolution(0);
        appendLog(QString("→ Первое решение (1 из %1)").arg(m_allSolutions.size()));
    }
}

void BrokerWindow::onPreviousSolutionClicked()
{
    stopAnimation();
    if (m_currentSolutionIndex > 0 && !m_allSolutions.empty()) {
        displaySolution(m_currentSolutionIndex - 1);
        appendLog(QString("→ Предыдущее решение (%1 из %2)")
                      .arg(m_currentSolutionIndex + 1)
                      .arg(m_allSolutions.size()));
    }
}

void BrokerWindow::onNextSolutionClicked()
{
    stopAnimation();
    if (m_currentSolutionIndex < (int)m_allSolutions.size() - 1 && !m_allSolutions.empty()) {
        displaySolution(m_currentSolutionIndex + 1);
        appendLog(QString("→ Следующее решение (%1 из %2)")
                      .arg(m_currentSolutionIndex + 1)
                      .arg(m_allSolutions.size()));
    }
}

void BrokerWindow::onShowAllSolutionsClicked()
{
    if (m_animationRunning) {
        stopAnimation();
    }

    if (m_allSolutions.empty()) {
        appendLog("[!] Нет решений для отображения");
        return;
    }

    m_animationRunning = true;
    m_animationIndex = 0;
    m_showAllButton->setEnabled(false);
    m_stopAnimationButton->setEnabled(true);
    m_firstSolutionButton->setEnabled(false);
    m_prevSolutionButton->setEnabled(false);
    m_nextSolutionButton->setEnabled(false);

    appendLog("\n[Анимация: последовательный показ всех решений]");
    animateSolutions(0);
}

void BrokerWindow::onStopAnimationClicked()
{
    stopAnimation();
}

void BrokerWindow::displaySolution(int index)
{
    appendLog(QString("[DEBUG] displaySolution вызван с index=%1").arg(index));

    if (index < 0 || index >= (int)m_allSolutions.size()) {
        appendLog(QString("[DEBUG] Ошибка: index=%1 вне диапазона [0, %2]").arg(index).arg(m_allSolutions.size() - 1));
        return;
    }

    m_currentSolutionIndex = index;
    const auto& solution = m_allSolutions[index];
    m_boardWidget->setQueenPositions(solution);
    m_boardWidget->update();

    appendLog(QString("→ Отображено решение %1 из %2").arg(index + 1).arg(m_allSolutions.size()));

    updateNavigationButtons();
}

void BrokerWindow::animateSolutions(int index)
{
    if (!m_animationRunning) return;

    if (index >= (int)m_allSolutions.size()) {
        // Анимация завершена
        m_animationRunning = false;
        m_stopAnimationButton->setEnabled(false);
        m_showAllButton->setEnabled(true);
        updateNavigationButtons();
        appendLog(QString("\n[Анимация завершена. Показано %1 решений]").arg(m_allSolutions.size()));
        return;
    }

    // Отображаем текущее решение
    m_currentSolutionIndex = index;
    m_boardWidget->setQueenPositions(m_allSolutions[index]);
    m_boardWidget->update();

    // Планируем показ следующего решения через 500 мс
    QTimer::singleShot(500, this, [this, index]() {
        animateSolutions(index + 1);
    });
}

void BrokerWindow::stopAnimation()
{
    if (m_animationRunning) {
        m_animationRunning = false;
        m_stopAnimationButton->setEnabled(false);
        m_showAllButton->setEnabled(true);
        updateNavigationButtons();
        appendLog("\n[Анимация остановлена]");
    }
}

void BrokerWindow::updateNavigationButtons()
{
    bool hasSolutions = !m_allSolutions.empty();

    m_firstSolutionButton->setEnabled(hasSolutions && m_currentSolutionIndex > 0);
    m_prevSolutionButton->setEnabled(hasSolutions && m_currentSolutionIndex > 0);
    m_nextSolutionButton->setEnabled(hasSolutions && m_currentSolutionIndex < (int)m_allSolutions.size() - 1);
    m_showAllButton->setEnabled(hasSolutions && !m_animationRunning);
}

// ========================================================================
// ОСТАЛЬНЫЕ МЕТОДЫ
// ========================================================================

void BrokerWindow::onStartServerClicked()
{
    int port = m_portSpin->value();
    int boardSize = m_boardSizeSpin->value();

    if (m_boardWidget) m_boardWidget->setBoardSize(boardSize);

    if (m_broker->start(port, boardSize)) {
        m_serverStatusLabel->setText(QString("Сервер: Запущен (порт %1)").arg(port));
        m_serverStatusLabel->setStyleSheet("color: #27ae60; font-weight: bold;");

        m_startServerButton->setEnabled(false);
        m_stopServerButton->setEnabled(true);
        m_portSpin->setEnabled(false);
        m_boardSizeSpin->setEnabled(false);
        m_startSearchButton->setEnabled(true);
        m_launchAllAgentsButton->setEnabled(true);

        m_agentsTable->setRowCount(0);

        appendLog(QString("Сервер запущен на порту %1, размер доски %2x%2")
                      .arg(port).arg(boardSize));

        if (m_autoLaunchAgentsCheckBox->isChecked()) {
            QTimer::singleShot(1000, this, &BrokerWindow::onLaunchAllAgentsClicked);
        }
    }
}

void BrokerWindow::onStopServerClicked()
{
    for (QProcess* proc : m_localAgentProcesses) {
        if (proc->state() == QProcess::Running) {
            proc->terminate();
            proc->waitForFinished(3000);
        }
        proc->deleteLater();
    }
    m_localAgentProcesses.clear();

    m_broker->stop();

    m_serverStatusLabel->setText("Сервер: Остановлен");
    m_serverStatusLabel->setStyleSheet("color: #e74c3c; font-weight: bold;");

    m_startServerButton->setEnabled(true);
    m_stopServerButton->setEnabled(false);
    m_portSpin->setEnabled(true);
    m_boardSizeSpin->setEnabled(true);
    m_startSearchButton->setEnabled(false);
    m_stopSearchButton->setEnabled(false);
    m_launchAllAgentsButton->setEnabled(false);

    appendLog("Сервер остановлен");
}

void BrokerWindow::onStartSearchClicked()
{
    if (!m_broker->isRunning()) {
        QMessageBox::warning(this, "Ошибка", "Сервер не запущен!");
        return;
    }

    // Очищаем предыдущие решения
    m_allSolutions.clear();
    m_currentSolutionIndex = -1;
    m_solutionsEdit->clear();
    m_solutionsCountLabel->setText("Решений: 0");

    // Обновляем кнопки навигации
    updateNavigationButtons();

    appendLog("Попытка запуска распределенного поиска...");

    bool success = m_broker->startSearch();

    if (success) {
        m_startSearchButton->setEnabled(false);
        m_stopSearchButton->setEnabled(true);
        m_progressBar->setVisible(true);
        m_progressBar->setRange(0, 0);

        appendLog("Распределенный поиск решений начат");
    } else {
        appendLog("ОШИБКА: Не удалось начать поиск");
    }
}

void BrokerWindow::onStopSearchClicked()
{
    m_broker->stopSearch();

    m_startSearchButton->setEnabled(true);
    m_stopSearchButton->setEnabled(false);
    m_progressBar->setVisible(false);

    stopAnimation();

    appendLog("Поиск остановлен");
}

void BrokerWindow::onSolutionFound(const std::vector<int>& solution)
{
    // Сохраняем ВСЕ решения
    m_allSolutions.push_back(solution);

    int count = (int)m_allSolutions.size();
    m_solutionsCountLabel->setText(QString("Решений: %1").arg(count));

    // Показываем ТОЛЬКО первое решение на доске
    if (count == 1) {
        m_currentSolutionIndex = 0;
        m_boardWidget->setQueenPositions(solution);
        m_boardWidget->update();

        // Выводим первое решение в лог с визуализацией
        appendLog("\n╔══════════════════════════════════════════════════════════════╗");
        appendLog("║                     ПЕРВОЕ РЕШЕНИЕ                            ║");
        appendLog("╚══════════════════════════════════════════════════════════════╝");

        QString solutionStr = formatSolution(solution);
        appendLog(solutionStr);

        appendToBoardVisualization(solution);

        // Активируем кнопки навигации
        updateNavigationButtons();
    }

    // Добавляем решение в текстовое поле (кратко)
    QString shortSolution;
    for (size_t i = 0; i < solution.size(); ++i) {
        if (solution[i] >= 0) {
            shortSolution += QString("%1%2 ").arg(QChar('A' + (int)i)).arg(solution[i] + 1);
        }
    }
    m_solutionsEdit->append(QString("#%1: %2").arg(count).arg(shortSolution));

    if (m_solutionsEdit->verticalScrollBar()) {
        m_solutionsEdit->verticalScrollBar()->setValue(m_solutionsEdit->verticalScrollBar()->maximum());
    }
}

void BrokerWindow::onProgressUpdated(int solutionsFound, int backtracks)
{
    m_solutionsCountLabel->setText(QString("Решений: %1").arg(solutionsFound));
    //m_backtracksLabel->setText(QString("Откатов: %1").arg(backtracks));
}

void BrokerWindow::onSearchFinished(int totalSolutions)
{
    m_startSearchButton->setEnabled(true);
    m_stopSearchButton->setEnabled(false);
    m_progressBar->setVisible(false);

    appendLog(QString("\n═══════════════════════════════════════════════════════════════"));
    appendLog(QString("  ПОИСК ЗАВЕРШЕН"));
    appendLog(QString("═══════════════════════════════════════════════════════════════"));
    appendLog(QString("Всего найдено решений: %1").arg(totalSolutions));

    if (!m_allSolutions.empty()) {
        updateNavigationButtons();
        appendLog(QString("\n[Используйте кнопки навигации для просмотра %1 решений]")
                      .arg(m_allSolutions.size()));
    }
}

void BrokerWindow::onAgentConnected(int agentId)
{
    updateAgentsTable();
}

void BrokerWindow::onAgentDisconnected(int agentId)
{
    appendLog(QString("Агент %1 отключился").arg(QChar('A' + agentId)));
    updateAgentsTable();
}

void BrokerWindow::onAgentPositionChanged(int agentId, int row)
{
    updateAgentsTable();

}

void BrokerWindow::onLogMessage(const QString& message)
{
    appendLog(message);
}

void BrokerWindow::onError(const QString& message)
{
    appendLog(QString("ОШИБКА: %1").arg(message));
    QMessageBox::warning(this, "Ошибка", message);
}

void BrokerWindow::onOpenKnowledgeBaseDialog()
{
    if (!m_knowledgeBase) return;

    int boardSize = m_boardSizeSpin->value();

    KnowledgeBaseDialog dialog(m_knowledgeBase, boardSize, this);
    dialog.exec();

    if (m_boardWidget) m_boardWidget->refreshDisplay();
}

void BrokerWindow::onLaunchAllAgentsClicked()
{
    int boardSize = m_boardSizeSpin->value();

    // Останавливаем старые процессы
    for (QProcess* proc : m_localAgentProcesses) {
        if (proc->state() == QProcess::Running) {
            proc->terminate();
            proc->waitForFinished(1000);
        }
        proc->deleteLater();
    }
    m_localAgentProcesses.clear();

    // ========================================================================
    // ПРАВИЛЬНОЕ ОПРЕДЕЛЕНИЕ ПУТЕЙ НА MACOS
    // ========================================================================

    QString agentPath;
    QString appDir = QApplication::applicationDirPath();
    QDir dir(appDir);

    // Поднимаемся на два уровня вверх из .app/Contents/MacOS
    dir.cdUp();  // -> Contents
    dir.cdUp();  // -> queens-broker.app
    dir.cdUp();  // -> директория сборки

    QString buildDir = dir.absolutePath();

    appendLog("=== Поиск исполняемого файла агента ===");
    appendLog(QString("Директория сборки: %1").arg(buildDir));

    // Список возможных путей (в порядке приоритета)
    QStringList possiblePaths;

    // 1. Рядом с брокером в той же директории сборки
    possiblePaths << buildDir + "/queens-agent.app/Contents/MacOS/queens-agent";
    possiblePaths << buildDir + "/queens-agent";

    // 2. В текущей директории запуска
    possiblePaths << QDir::currentPath() + "/queens-agent.app/Contents/MacOS/queens-agent";
    possiblePaths << QDir::currentPath() + "/queens-agent";

    // 3. По относительному пути
    possiblePaths << "./queens-agent.app/Contents/MacOS/queens-agent";
    possiblePaths << "./queens-agent";

    // 4. Абсолютные пути
    possiblePaths << "/Applications/queens-agent.app/Contents/MacOS/queens-agent";
    possiblePaths << "/usr/local/bin/queens-agent";

    // Ищем первый существующий путь
    for (const QString& path : possiblePaths) {
        QFileInfo fi(path);
        bool exists = fi.exists() && fi.isExecutable();
        appendLog(QString("  %1 %2").arg(exists ? "✓" : "✗").arg(path));

        if (exists && agentPath.isEmpty()) {
            agentPath = path;
        }
    }

    // Если не нашли - пробуем найти через which
    if (agentPath.isEmpty()) {
        QProcess which;
        which.start("which", QStringList() << "queens-agent");
        which.waitForFinished();
        QString whichPath = which.readAllStandardOutput().trimmed();
        if (!whichPath.isEmpty() && QFile::exists(whichPath)) {
            agentPath = whichPath;
            appendLog(QString("  ✓ Найдено через which: %1").arg(agentPath));
        }
    }

    if (agentPath.isEmpty()) {
        appendLog("ОШИБКА: Исполняемый файл агента не найден!");

        QString msg = "Не найден исполняемый файл queens-agent!\n\n";
        msg += "Возможные решения:\n";
        msg += "1. Убедитесь, что queens-agent.app собран\n";
        msg += "2. Запустите агентов вручную из терминала:\n";
        msg += "   cd " + buildDir + "\n";
        msg += "   open queens-agent.app --args --id 0\n";
        msg += "3. Или запустите исполняемый файл напрямую:\n";
        msg += "   ./queens-agent.app/Contents/MacOS/queens-agent --id 0\n";

        QMessageBox::warning(this, "Файл не найден", msg);
        return;
    }

    appendLog(QString("Найден исполняемый файл: %1").arg(agentPath));

    // Делаем файл исполняемым (на всякий случай)
    QFile::setPermissions(agentPath,
                          QFile::permissions(agentPath) |
                              QFileDevice::ExeOwner |
                              QFileDevice::ExeUser |
                              QFileDevice::ExeGroup);

    QString host = "127.0.0.1";
    int port = m_portSpin->value();

    appendLog(QString("Запуск %1 агентов...").arg(boardSize));

    int successCount = 0;

    for (int i = boardSize - 1; i >= 0; i--) {
        QProcess* process = new QProcess(this);

        QStringList args;
        args << "--mode" << "agent"
             << "--id" << QString::number(i)
             << "--host" << host
             << "--port" << QString::number(port);

        // Подключаем вывод для отладки
        connect(process, &QProcess::errorOccurred, [this, i](QProcess::ProcessError error) {
            appendLog(QString("  Ошибка процесса агента %1: %2").arg(i).arg(error));
        });

        connect(process, &QProcess::readyReadStandardError, [process, this]() {
            QString err = process->readAllStandardError();
            if (!err.isEmpty()) {
                appendLog(QString("  stderr: %1").arg(err.trimmed()));
            }
        });

        process->start(agentPath, args);

        if (process->waitForStarted(3000)) {
            m_localAgentProcesses.append(process);
            appendLog(QString("  ✓ Агент %1 запущен (PID: %2, столбец %3)")
                          .arg(i)
                          .arg(process->processId())
                          .arg(QChar('A' + i)));
            successCount++;
        } else {
            appendLog(QString("  ✗ Ошибка запуска агента %1: %2")
                          .arg(i)
                          .arg(process->errorString()));
            process->deleteLater();
        }

        QThread::msleep(300);
    }

    appendLog(QString("=== Запущено %1 из %2 агентов ===").arg(successCount).arg(boardSize));

    if (successCount == 0) {
        QMessageBox::warning(this, "Ошибка запуска",
                             "Не удалось запустить ни одного агента.\n\n"
                             "Попробуйте запустить вручную:\n"
                             "cd " + buildDir + "\n" +
                                 "./queens-agent.app/Contents/MacOS/queens-agent --id 0");
    }
}
void BrokerWindow::updateAgentsTable()
{
    if (!m_broker) return;

    int boardSize = m_broker->getBoardSize();
    m_agentsTable->setRowCount(boardSize);

    // Получаем текущее состояние от брокера
    std::vector<int> state = m_broker->collectCurrentState();

    for (int i = 0; i < boardSize; ++i) {
        // ID
        QTableWidgetItem* idItem = new QTableWidgetItem(QString::number(i));
        idItem->setTextAlignment(Qt::AlignCenter);
        m_agentsTable->setItem(i, 0, idItem);

        // Столбец (буква)
        QTableWidgetItem* colItem = new QTableWidgetItem(QString(QChar('A' + i)));
        colItem->setTextAlignment(Qt::AlignCenter);
        m_agentsTable->setItem(i, 1, colItem);

        // Строка (текущая позиция ферзя от агента)
        QString rowStr = "—";
        if (i < (int)state.size() && state[i] >= 0) {
            rowStr = QString::number(state[i] + 1);
        }
        QTableWidgetItem* rowItem = new QTableWidgetItem(rowStr);
        rowItem->setTextAlignment(Qt::AlignCenter);

        // Подсветка если позиция зафиксирована агентом
        if (m_broker->getFixedPosition(i).has_value()) {
            rowItem->setForeground(QBrush(QColor(255, 100, 0)));  // Оранжевый текст
            rowItem->setToolTip("Зафиксировано агентом");
        }
        m_agentsTable->setItem(i, 2, rowItem);

        // Фиксация (статус от агента)
        QString fixedStr = "Нет";
        auto fixedPos = m_broker->getFixedPosition(i);
        if (fixedPos.has_value()) {
            fixedStr = QString("Да (строка %1)").arg(fixedPos.value() + 1);
        }
        QTableWidgetItem* fixedItem = new QTableWidgetItem(fixedStr);
        fixedItem->setTextAlignment(Qt::AlignCenter);
        if (fixedPos.has_value()) {
            fixedItem->setBackground(QBrush(QColor(255, 240, 200)));  // Светло-оранжевый фон
            fixedItem->setForeground(QBrush(QColor(200, 80, 0)));
        }
        m_agentsTable->setItem(i, 3, fixedItem);

        // Приоритет (от агента)
        QString priorityStr = "—";
        int priority = m_broker->getPriority(i);
        if (priority > 0) {
            priorityStr = QString::number(priority);
            if (priority == 1) priorityStr += " ★★★ (наивысший)";
            else if (priority == 2) priorityStr += " ★★ (высокий)";
            else if (priority == 3) priorityStr += " ★ (выше среднего)";
            else if (priority == 10) priorityStr += " (низший)";
        }
        QTableWidgetItem* prioItem = new QTableWidgetItem(priorityStr);
        prioItem->setTextAlignment(Qt::AlignCenter);

        // Цветовая индикация приоритета
        if (priority == 1) {
            prioItem->setForeground(QBrush(QColor(231, 76, 60)));  // Красный
            prioItem->setToolTip("Наивысший приоритет");
        } else if (priority == 2) {
            prioItem->setForeground(QBrush(QColor(230, 126, 34)));  // Оранжевый
        } else if (priority == 3) {
            prioItem->setForeground(QBrush(QColor(241, 196, 15)));  // Желтый
        } else if (priority >= 8) {
            prioItem->setForeground(QBrush(QColor(149, 165, 166)));  // Серый
            prioItem->setToolTip("Низкий приоритет");
        }

        m_agentsTable->setItem(i, 4, prioItem);
    }

    // Растягиваем колонки
    m_agentsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}
void BrokerWindow::appendLog(const QString& text)
{
    if (!m_logEdit) return;
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_logEdit->append(QString("[%1] %2").arg(timestamp).arg(text));
    if (m_logEdit->verticalScrollBar()) {
        m_logEdit->verticalScrollBar()->setValue(m_logEdit->verticalScrollBar()->maximum());
    }
}

void BrokerWindow::clearLog()
{
    if (m_logEdit) m_logEdit->clear();
    appendLog("Лог очищен");
}

QString BrokerWindow::formatSolution(const std::vector<int>& solution) const
{
    QString result = "Позиции: ";
    for (size_t i = 0; i < solution.size(); ++i) {
        if (solution[i] >= 0) {
            result += QString("%1%2 ").arg(QChar('A' + (int)i)).arg(solution[i] + 1);
        }
    }
    return result;
}

void BrokerWindow::appendToBoardVisualization(const std::vector<int>& solution)
{
    int n = (int)solution.size();

    QString topLine = "    ";
    for (int col = 0; col < n; ++col) {
        topLine += QString(" %1 ").arg(QChar('A' + col));
    }
    appendLog(topLine);

    appendLog("    " + QString(n * 2 + 1, '-'));

    for (int row = 0; row < n; ++row) {
        QString line = QString("%1 |").arg(row + 1, 2);
        for (int col = 0; col < n; ++col) {
            if (solution[col] == row) {
                line += " Q ";
            } else {
                line += " . ";
            }
        }
        line += "|";
        appendLog(line);
    }

    appendLog("    " + QString(n * 2 + 1, '-'));
}

void BrokerWindow::closeEvent(QCloseEvent* event)
{
    for (QProcess* proc : m_localAgentProcesses) {
        if (proc->state() == QProcess::Running) {
            proc->terminate();
            proc->waitForFinished(3000);
        }
        proc->deleteLater();
    }
    m_localAgentProcesses.clear();

    if (m_broker) m_broker->stop();
    event->accept();
}

QString BrokerWindow::findAgentExecutable()
{
    QStringList possiblePaths;
    QString appDir = QCoreApplication::applicationDirPath();

#ifdef Q_OS_WIN
    possiblePaths << appDir + "/queens-agent.exe";
    possiblePaths << "./queens-agent.exe";
#elif defined(Q_OS_MAC)
    possiblePaths << appDir + "/queens-agent";
    possiblePaths << appDir + "/queens-agent.app/Contents/MacOS/queens-agent";
    possiblePaths << "./queens-agent";
#else
    possiblePaths << appDir + "/queens-agent";
    possiblePaths << "./queens-agent";
#endif

    for (const QString& path : possiblePaths) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    return QString();
}