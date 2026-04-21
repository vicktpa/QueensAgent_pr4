#include "MainWindow.h"
#include "ChessBoardWidget.h"
#include "QueensSolver.h"
#include "KnowledgeBase.h"
#include "KnowledgeBaseDialog.h"
#include "SortDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QThread>
#include <QTextEdit>
#include <QScrollBar>
#include <QFileDialog>
#include <QTextStream>
#include <QCloseEvent>
#include <QCheckBox>
#include <QRandomGenerator>

// ============================================================================
// Конструктор и деструктор
// ============================================================================

/**
 *  Конструктор главного окна
 *  parent Родительский виджет
 *
 * Инициализирует все переменные, создает интерфейс,
 * подключает сигналы и устанавливает начальные параметры.
 */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_isSolving(false)
    , m_currentSolutionsCount(0)
    , m_currentAnimationIndex(0)
    , m_animationRunning(false)
    , m_boardWidget(nullptr)
    , m_startButton(nullptr)
    , m_stopButton(nullptr)
    , m_firstButton(nullptr)
    , m_nextButton(nullptr)
    , m_prevButton(nullptr)
    , m_showAllButton(nullptr)
    , m_stopAnimationButton(nullptr)
    , m_boardSizeSpinBox(nullptr)
    , m_applySizeButton(nullptr)
    , m_solutionsCountLabel(nullptr)
    , m_progressBar(nullptr)
    , m_editModeCheckBox(nullptr)
    , m_knowledgeBaseButton(nullptr)
    , m_sortSolutionsButton(nullptr)
    , m_randomizeKnowledgeButton(nullptr)
    , m_resetKnowledgeButton(nullptr)
    , m_consoleTextEdit(nullptr)
    , m_clearConsoleButton(nullptr)
    , m_saveConsoleButton(nullptr)
    , m_solver(nullptr)
    , m_solverThread(nullptr)
    , m_knowledgeBase(nullptr)
    , m_priorityManager(nullptr)
    , m_priorityModeCheckBox(nullptr)
{
    setupUI();                    // Создаем пользовательский интерфейс
    initPriorityManager();        // Инициализируем менеджер приоритетов
    connectSignals();             // Подключаем сигналы и слоты
    setWindowTitle("Задача о ферзях. Пункт 2");
    resize(1200, 800);

    // Выводим справочную информацию при запуске
    showWelcomeMessage();
}

/**
 *  Деструктор главного окна
 *
 * Останавливает поиск и анимацию, завершает поток решателя.
 */
MainWindow::~MainWindow()
{
    stopSolving();                // Останавливаем поиск
    if (m_solverThread) {
        m_solverThread->quit();   // Завершаем поток
        m_solverThread->wait();   // Ждем завершения
    }
}

// ============================================================================
// Инициализация интерфейса
// ============================================================================

/**
 * Создает и настраивает пользовательский интерфейс
 */
void MainWindow::setupUI()
{
    // Центральный виджет (контейнер для всего содержимого)
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // Основной горизонтальный layout (левая + правая панели)
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    // ========================================================================
    // ЛЕВАЯ ПАНЕЛЬ
    // ========================================================================
    QWidget* leftPanel = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setSpacing(8);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // ------------------------------------------------------------------------
    // Виджет шахматной доски
    // ------------------------------------------------------------------------
    m_boardWidget = new ChessBoardWidget(this);
    m_boardWidget->setMinimumSize(350, 350);
    leftLayout->addWidget(m_boardWidget, 1);  // Растягивается

    // ------------------------------------------------------------------------
    // Группа "Поиск решений"
    // ------------------------------------------------------------------------
    QGroupBox* searchGroup = new QGroupBox("Поиск решений", this);
    QVBoxLayout* searchLayout = new QVBoxLayout(searchGroup);
    searchLayout->setSpacing(5);
    searchLayout->setContentsMargins(5, 8, 5, 5);

    // Настройки размера доски
    QHBoxLayout* settingsLayout = new QHBoxLayout();
    QLabel* sizeLabel = new QLabel("N:", this);
    sizeLabel->setFixedWidth(20);
    m_boardSizeSpinBox = new QSpinBox(this);
    m_boardSizeSpinBox->setRange(4, 10);      // Размер доски от 4 до 10
    m_boardSizeSpinBox->setValue(8);          // По умолчанию 8x8
    m_boardSizeSpinBox->setFixedWidth(60);
    m_applySizeButton = new QPushButton("Применить размер", this);
    m_applySizeButton->setFixedHeight(25);
    settingsLayout->addWidget(sizeLabel);
    settingsLayout->addWidget(m_boardSizeSpinBox);
    settingsLayout->addWidget(m_applySizeButton);
    settingsLayout->addStretch();
    searchLayout->addLayout(settingsLayout);

    // Кнопки управления поиском
    QHBoxLayout* searchButtonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("Начать поиск", this);
    m_stopButton = new QPushButton("Остановить поиск", this);
    m_stopButton->setEnabled(false);          // Изначально недоступна
    searchButtonLayout->addWidget(m_startButton);
    searchButtonLayout->addWidget(m_stopButton);
    searchButtonLayout->addStretch();
    searchLayout->addLayout(searchButtonLayout);

    // Счетчик решений
    m_solutionsCountLabel = new QLabel("Найдено решений: 0", this);
    searchLayout->addWidget(m_solutionsCountLabel);

    // Прогресс-бар
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setFixedHeight(15);
    searchLayout->addWidget(m_progressBar);

    leftLayout->addWidget(searchGroup);

    // ------------------------------------------------------------------------
    // Группа "Отображение"
    // ------------------------------------------------------------------------
    QGroupBox* displayGroup = new QGroupBox("Отображение", this);
    QVBoxLayout* displayLayout = new QVBoxLayout(displayGroup);
    displayLayout->setSpacing(3);
    displayLayout->setContentsMargins(5, 8, 5, 5);

    m_editModeCheckBox = new QCheckBox("Режим редактирования", this);
    displayLayout->addWidget(m_editModeCheckBox);

    m_priorityModeCheckBox = new QCheckBox("Режим приоритетов (двойной клик)", this);
    displayLayout->addWidget(m_priorityModeCheckBox);

    leftLayout->addWidget(displayGroup);

    // ------------------------------------------------------------------------
    // Группа "База знаний"
    // ------------------------------------------------------------------------
    QGroupBox* knowledgeGroup = new QGroupBox("База знаний", this);
    QVBoxLayout* knowledgeLayout = new QVBoxLayout(knowledgeGroup);
    knowledgeLayout->setSpacing(3);
    knowledgeLayout->setContentsMargins(5, 8, 5, 5);

    m_knowledgeBaseButton = new QPushButton("Управлять базой знаний", this);
    m_sortSolutionsButton = new QPushButton("Сортировать", this);
    m_randomizeKnowledgeButton = new QPushButton("Случайно (2 цвета)", this);
    m_resetKnowledgeButton = new QPushButton("Сбросить базу знаний", this);
    m_resetBoardButton = new QPushButton("Сбросить состояние доски", this);


    // Устанавливаем единую высоту для всех кнопок
    int btnHeight = 28;
    m_knowledgeBaseButton->setFixedHeight(btnHeight);
    m_sortSolutionsButton->setFixedHeight(btnHeight);
    m_randomizeKnowledgeButton->setFixedHeight(btnHeight);
    m_resetKnowledgeButton->setFixedHeight(btnHeight);
    m_resetBoardButton->setFixedHeight(btnHeight);

    knowledgeLayout->addWidget(m_knowledgeBaseButton);
    knowledgeLayout->addWidget(m_sortSolutionsButton);
    knowledgeLayout->addWidget(m_randomizeKnowledgeButton);
    knowledgeLayout->addWidget(m_resetKnowledgeButton);
    knowledgeLayout->addWidget(m_resetBoardButton);

    leftLayout->addWidget(knowledgeGroup);

    // ------------------------------------------------------------------------
    // Группа "Навигация"
    // ------------------------------------------------------------------------
    QGroupBox* navigationGroup = new QGroupBox("Навигация", this);
    QVBoxLayout* navigationLayout = new QVBoxLayout(navigationGroup);
    navigationLayout->setSpacing(3);
    navigationLayout->setContentsMargins(5, 8, 5, 5);

    // Кнопки навигации (первое, предыдущее, следующее)
    QHBoxLayout* navButtonLayout = new QHBoxLayout();
    m_firstButton = new QPushButton("Первое решение", this);
    m_prevButton = new QPushButton("Предыдущее", this);
    m_nextButton = new QPushButton("Следующее", this);
    m_firstButton->setEnabled(false);
    m_prevButton->setEnabled(false);
    m_nextButton->setEnabled(false);
    navButtonLayout->addWidget(m_firstButton);
    navButtonLayout->addWidget(m_prevButton);
    navButtonLayout->addWidget(m_nextButton);
    navigationLayout->addLayout(navButtonLayout);

    // Кнопки анимации
    QHBoxLayout* animationLayout = new QHBoxLayout();
    m_showAllButton = new QPushButton("Показать все", this);
    m_stopAnimationButton = new QPushButton("Остановить показ", this);
    m_showAllButton->setEnabled(false);
    m_stopAnimationButton->setEnabled(false);
    animationLayout->addWidget(m_showAllButton);
    animationLayout->addWidget(m_stopAnimationButton);
    navigationLayout->addLayout(animationLayout);

    leftLayout->addWidget(navigationGroup);
    leftLayout->addStretch();  // Растягивает пустое пространство внизу

    // ========================================================================
    // ПРАВАЯ ПАНЕЛЬ (Консоль)
    // ========================================================================
    QWidget* rightPanel = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setSpacing(5);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* consoleGroup = new QGroupBox("Консоль решений", this);
    QVBoxLayout* consoleLayout = new QVBoxLayout(consoleGroup);
    consoleLayout->setSpacing(3);
    consoleLayout->setContentsMargins(5, 8, 5, 5);

    // Текстовое поле для вывода решений
    m_consoleTextEdit = new QTextEdit(this);
    m_consoleTextEdit->setReadOnly(true);              // Только для чтения
    m_consoleTextEdit->setFont(QFont("Courier New", 9)); // Моноширинный шрифт
    m_consoleTextEdit->setMinimumWidth(350);
    m_consoleTextEdit->setMaximumHeight(500);
    consoleLayout->addWidget(m_consoleTextEdit, 1);

    // Кнопки управления консолью
    QHBoxLayout* consoleButtonLayout = new QHBoxLayout();
    m_clearConsoleButton = new QPushButton("Очистить", this);
    m_saveConsoleButton = new QPushButton("Сохранить в файл", this);
    consoleButtonLayout->addWidget(m_clearConsoleButton);
    consoleButtonLayout->addWidget(m_saveConsoleButton);
    consoleButtonLayout->addStretch();
    consoleLayout->addLayout(consoleButtonLayout);

    rightLayout->addWidget(consoleGroup);

    // ========================================================================
    // Сборка главного окна
    // ========================================================================
    mainLayout->addWidget(leftPanel, 2);   // Левая панель занимает 2 части
    mainLayout->addWidget(rightPanel, 1);  // Правая панель занимает 1 часть

    setMinimumSize(800, 600);  // Минимальный размер окна
}

// ============================================================================
// Подключение сигналов и слотов
// ============================================================================

/**
 *  Подключает все сигналы к соответствующим слотам
 *
 * Это сердце приложения - здесь определяются связи между
 * действиями пользователя и реакцией программы.
 */
void MainWindow::connectSignals()
{
    // ========================================================================
    // Управление поиском
    // ========================================================================
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startSolving);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopSolving);

    // ========================================================================
    // Навигация по решениям
    // ========================================================================
    connect(m_nextButton, &QPushButton::clicked, this, &MainWindow::nextSolution);
    connect(m_prevButton, &QPushButton::clicked, this, &MainWindow::previousSolution);
    connect(m_firstButton, &QPushButton::clicked, this, &MainWindow::firstSolution);
    connect(m_showAllButton, &QPushButton::clicked, this, &MainWindow::showAllSolutions);
    connect(m_stopAnimationButton, &QPushButton::clicked, this, &MainWindow::stopAnimation);

    // ========================================================================
    // Управление доской
    // ========================================================================
    connect(m_applySizeButton, &QPushButton::clicked, this, &MainWindow::applyBoardSize);

    // ========================================================================
    // Управление консолью
    // ========================================================================
    if (m_clearConsoleButton && m_saveConsoleButton) {
        connect(m_clearConsoleButton, &QPushButton::clicked, this, &MainWindow::clearConsole);
        connect(m_saveConsoleButton, &QPushButton::clicked, this, &MainWindow::saveConsoleToFile);
    }

    // ========================================================================
    // База знаний и связанные функции
    // ========================================================================
    if (m_knowledgeBase) {
        // При обновлении БЗ обновляем отображение доски
        connect(m_knowledgeBase, &KnowledgeBase::knowledgeUpdated,
                this, [this]() {
                    if (m_boardWidget) {
                        m_boardWidget->refreshDisplay();
                    }
                });
    }

    connect(m_editModeCheckBox, &QCheckBox::toggled, this, &MainWindow::toggleEditMode);
    connect(m_knowledgeBaseButton, &QPushButton::clicked, this, &MainWindow::openKnowledgeBaseDialog);
    connect(m_sortSolutionsButton, &QPushButton::clicked, this, &MainWindow::openSortDialog);
    connect(m_randomizeKnowledgeButton, &QPushButton::clicked, this, &MainWindow::randomizeAllKnowledge);
    connect(m_resetKnowledgeButton, &QPushButton::clicked, this, &MainWindow::resetAllKnowledge);

    // ========================================================================
    // Сигналы от шахматной доски (изменения в БЗ через редактирование)
    // ========================================================================
    if (m_boardWidget) {
        connect(m_boardWidget, &ChessBoardWidget::cellWeightChanged,
                this, &MainWindow::onCellWeightChanged);
        connect(m_boardWidget, &ChessBoardWidget::cellColorChanged,
                this, &MainWindow::onCellColorChanged);
    }

    connect(m_priorityModeCheckBox, &QCheckBox::toggled, this, &MainWindow::togglePriorityMode);
    connect(m_resetBoardButton, &QPushButton::clicked, this, &MainWindow::resetBoardState);
}

// ============================================================================
// Управление доской и размером
// ============================================================================

/**
 *  Применяет новый размер доски
 *
 * Вызывается при нажатии кнопки "Применить размер".
 * Сбрасывает все текущие решения и инициализирует БЗ для нового размера.
 */
void MainWindow::applyBoardSize()
{
    int newSize = m_boardSizeSpinBox->value();

    // Останавливаем текущий поиск, если он выполняется
    if (m_isSolving) {
        stopSolving();
    }

    // Останавливаем анимацию
    stopAnimation();

    // Обновляем виджет доски
    m_boardWidget->setBoardSize(newSize);
    m_boardWidget->clearQueens();

    // Сбрасываем список решений
    m_boardWidget->setSolutions({});
    m_currentSolutionsCount = 0;
    m_solutionsCountLabel->setText("Решений: 0");

    // Отключаем кнопки навигации (решений пока нет)
    m_prevButton->setEnabled(false);
    m_nextButton->setEnabled(false);
    m_firstButton->setEnabled(false);
    m_showAllButton->setEnabled(false);

    // Инициализируем базу знаний для нового размера
    if (m_knowledgeBase) {
        if (m_knowledgeBase->getBoardSize() != newSize || !m_knowledgeBase->isValid()) {
            generateDefaultAllGreen();  // Создаем все зеленые клетки по умолчанию
        }
        m_boardWidget->setKnowledgeBase(m_knowledgeBase);
        m_boardWidget->refreshDisplay();
    }

    // Выводим информацию в консоль
    appendToConsole("========================================");
    appendToConsole("Размер доски изменен на " + QString::number(newSize) + "x" + QString::number(newSize));
    appendToConsole("========================================");
}

// ============================================================================
// Поиск решений
// ============================================================================

/**
 *  Начинает поиск всех решений
 *
 * Создает отдельный поток, инициализирует решатель и запускает поиск.
 * Во время поиска блокируются элементы управления, которые могут
 * повлиять на процесс.
 */
void MainWindow::startSolving()
{
    // Если уже идет поиск - останавливаем
    if (m_isSolving) stopSolving();

    // Останавливаем анимацию
    stopAnimation();

    int boardSize = m_boardSizeSpinBox->value();
    m_boardWidget->setBoardSize(boardSize);
    m_boardWidget->clearQueens();

    // Создаем базу знаний, если еще не создана
    if (!m_knowledgeBase) {
        m_knowledgeBase = new KnowledgeBase(this);
        m_knowledgeBase->initializeDatabase();
    }

    m_boardWidget->setKnowledgeBase(m_knowledgeBase);

    // Проверяем, инициализирована ли БЗ для текущего размера
    if (!m_knowledgeBase->isValid() || m_knowledgeBase->getBoardSize() != boardSize) {
        appendToConsole("[ВНИМАНИЕ] База знаний не инициализирована для доски " +
                        QString::number(boardSize) + "x" + QString::number(boardSize));
        appendToConsole("[СОВЕТ] Автоматически генерируем все зеленые клетки...");

        generateDefaultAllGreen();

        if (!m_knowledgeBase->isValid()) {
            appendToConsole("[ОШИБКА] Не удалось инициализировать базу знаний");
            m_startButton->setEnabled(true);
            m_stopButton->setEnabled(false);
            m_boardSizeSpinBox->setEnabled(true);
            m_applySizeButton->setEnabled(true);
            m_progressBar->setVisible(false);
            return;
        }
    }

    // Сбрасываем счетчики
    m_currentSolutionsCount = 0;
    m_solutionsCountLabel->setText("Поиск решений...");

    // Настраиваем прогресс-бар
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);  // Бесконечный режим (полоса прокрутки)

    // Блокируем кнопки во время поиска
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_prevButton->setEnabled(false);
    m_nextButton->setEnabled(false);
    m_firstButton->setEnabled(false);
    m_showAllButton->setEnabled(false);
    m_stopAnimationButton->setEnabled(false);
    m_boardSizeSpinBox->setEnabled(false);
    m_applySizeButton->setEnabled(false);

    // Очищаем и заполняем консоль информацией о начале поиска
    clearConsole();
    appendToConsole("========================================");
    appendToConsole("Количество ферзей: N = " + QString::number(boardSize));
    appendToConsole("========================================");

    // Показываем фиксированные позиции в консоли
    if (m_priorityManager) {
        bool hasFixed = false;
        appendToConsole("ФИКСИРОВАННЫЕ ПОЗИЦИИ:");
        for (int col = 0; col < boardSize; ++col) {
            auto fixed = m_priorityManager->getFixedPosition(col);
            if (fixed.has_value()) {
                hasFixed = true;
                appendToConsole(QString("  Столбец %1: строка %2")
                                    .arg(QChar('A' + col))
                                    .arg(fixed.value() + 1));
            }
        }
        if (!hasFixed) {
            appendToConsole("  Нет фиксированных позиций");
        }

        // Показываем приоритеты
        appendToConsole("\nПРИОРИТЕТЫ СТОЛБЦОВ:");
        bool hasPriority = false;
        for (int col = 0; col < boardSize; ++col) {
            int priority = m_priorityManager->getPriority(col);
            if (priority > 0) {
                hasPriority = true;
                appendToConsole(QString("  Столбец %1: приоритет %2")
                                    .arg(QChar('A' + col))
                                    .arg(priority));
            }
        }
        if (!hasPriority) {
            appendToConsole("  Нет заданных приоритетов (обычный порядок)");
        }
        appendToConsole("========================================\n");
    }

    appendToConsole("ПРАВИЛА ПОИСКА:");
    appendToConsole("1. Ферзи не должны атаковать друг друга");
    appendToConsole("2. ВСЕ ферзи должны быть на клетках ОДНОГО ЦВЕТА");
    appendToConsole("3. Фиксированные позиции не изменяются");
    appendToConsole("========================================\n");

    // Выводим текущее распределение цветов в БЗ
    auto distribution = m_knowledgeBase->getColorDistribution();
    appendToConsole(QString("Текущее распределение цветов: зеленый=%1 красный=%2 синий=%3")
                        .arg(distribution[PositionColor::Green])
                        .arg(distribution[PositionColor::Red])
                        .arg(distribution[PositionColor::Blue]));

    appendToConsole("\nНачинаем поиск всех решений...\n");

    // ========================================================================
    // Создание и запуск потока для поиска решений
    // ========================================================================

    // Удаляем старый решатель и поток, если они есть
    if (m_solver) {
        m_solver->deleteLater();
        m_solver = nullptr;
    }

    if (m_solverThread) {
        if (m_solverThread->isRunning()) {
            m_solverThread->quit();
            m_solverThread->wait(1000);
        }
        m_solverThread->deleteLater();
        m_solverThread = nullptr;
    }

    m_solverThread = new QThread(this);
    m_solver = new QueensSolver(boardSize, m_knowledgeBase);

    // Передаем приоритеты и фиксированные позиции в решатель
    if (m_priorityManager) {
        m_solver->setPriorityManager(m_priorityManager);

        // Копируем фиксированные позиции из PriorityManager
        std::vector<std::optional<int>> fixedPositions;
        fixedPositions.resize(boardSize);
        for (int col = 0; col < boardSize; ++col) {
            fixedPositions[col] = m_priorityManager->getFixedPosition(col);
        }
        m_solver->setFixedPositions(fixedPositions);
    }

    m_solver->moveToThread(m_solverThread);

    // Подключаем сигналы решателя к слотам главного окна
    connect(m_solverThread, &QThread::started, m_solver, &QueensSolver::solve);
    connect(m_solver, &QueensSolver::solutionFound, this, &MainWindow::onSolutionFound);

    // Обработка решения с весом (для вывода информации о цвете)
    connect(m_solver, &QueensSolver::solutionWithCostFound, this, [this](const SolutionWithCost& sol) {
        if (!sol.positions.empty()) {
            auto it = sol.colorMap.find(0);
            if (it != sol.colorMap.end()) {
                QString colorName;
                switch (it->second) {
                case PositionColor::Red: colorName = "КРАСНЫЙ"; break;
                case PositionColor::Green: colorName = "ЗЕЛЕНЫЙ"; break;
                case PositionColor::Blue: colorName = "СИНИЙ"; break;
                }
                appendToConsole(QString("  Все ферзи на клетках цвета: %1 |  Общий вес: %2")
                                    .arg(colorName)
                                    .arg(sol.totalWeight));
            }
        }
    });

    connect(m_solver, &QueensSolver::solutionPrinted, this, &MainWindow::onSolutionPrinted);
    connect(m_solver, &QueensSolver::progressUpdated, this, &MainWindow::updateProgress);
    connect(m_solver, &QueensSolver::finished, this, &MainWindow::onSolvingFinished);
    connect(m_solver, &QueensSolver::error, this, &MainWindow::onError);
    connect(m_solver, &QueensSolver::allSolutionsFound, this, &MainWindow::onAllSolutionsFound);

    // Обработка всех решений с весами
    connect(m_solver, &QueensSolver::allSolutionsWithCostReady, this, [this](const std::vector<SolutionWithCost>& solutions) {
        appendToConsole(QString("\n[Найдено %1 решений]").arg(solutions.size()));

        // Подсчитываем количество решений по цветам
        int greenSolutions = 0, redSolutions = 0, blueSolutions = 0;
        for (const auto& sol : solutions) {
            auto it = sol.colorMap.find(0);
            if (it != sol.colorMap.end()) {
                switch (it->second) {
                case PositionColor::Red: redSolutions++; break;
                case PositionColor::Green: greenSolutions++; break;
                case PositionColor::Blue: blueSolutions++; break;
                }
            }
        }
        appendToConsole(QString("  Решения по цветам: зеленый=%1 красный=%2 синий=%3")
                            .arg(greenSolutions).arg(redSolutions).arg(blueSolutions));
    });

    // Обработка завершения сортировки
    connect(m_solver, &QueensSolver::sortingCompleted, this, [this](int count, bool ascending) {
        appendToConsole(QString("[Отсортировано %1 решений %2 по весу]")
                            .arg(count)
                            .arg(ascending ? "по возрастанию" : "по убыванию"));
    });

    // Очистка после завершения потока
    connect(m_solverThread, &QThread::finished, m_solver, &QObject::deleteLater);
    connect(m_solverThread, &QThread::finished, m_solverThread, &QObject::deleteLater);
    connect(m_solverThread, &QThread::finished, this, [this]() {
        m_solverThread = nullptr;
        m_solver = nullptr;
    });

    m_isSolving = true;
    m_solverThread->start();  // Запускаем поток
}

/**
 *  Останавливает текущий поиск решений
 *
 * Устанавливает флаг остановки и ожидает завершения потока.
 */
void MainWindow::stopSolving()
{
    if (m_solver) m_solver->stop();

    if (m_solverThread && m_solverThread->isRunning()) {
        m_solverThread->quit();
        m_solverThread->wait(1000);
    }

    m_isSolving = false;
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_boardSizeSpinBox->setEnabled(true);
    m_applySizeButton->setEnabled(true);
    m_progressBar->setVisible(false);

    appendToConsole("\n[ПОИСК ОСТАНОВЛЕН ПОЛЬЗОВАТЕЛЕМ]\n");
}

/**
 *  Обновляет прогресс-бар поиска
 *  current Текущее количество найденных решений
 *  total Ожидаемое общее количество решений
 *
 * Если total > 0, прогресс-бар переключается в режим с известным максимумом.
 */
void MainWindow::updateProgress(int current, int total)
{
    m_currentSolutionsCount = current;
    if (total > 0) {
        m_progressBar->setRange(0, total);
        m_progressBar->setValue(current);
        m_solutionsCountLabel->setText(QString("Найдено решений: %1 / %2").arg(current).arg(total));
        if (current >= total) {
            m_solutionsCountLabel->setText(QString("Найдено решений: %1 / %2 - ВСЕ!").arg(current).arg(total));
        }
    } else {
        m_progressBar->setRange(0, 0);  // Бесконечный режим
        m_solutionsCountLabel->setText(QString("Найдено решений: %1").arg(current));
    }
}

// ============================================================================
// Обработка результатов поиска
// ============================================================================

/**
 *  Обрабатывает найденное решение
 *  solution Вектор позиций ферзей
 *
 * При нахождении первого решения отображает его на доске.
 */
void MainWindow::onSolutionFound(const std::vector<int>& solution)
{
    m_currentSolutionsCount++;
    m_solutionsCountLabel->setText(QString("Найдено решений: %1").arg(m_currentSolutionsCount));

    // Показываем первое решение на доске
    if (m_currentSolutionsCount == 1) {
        m_boardWidget->setQueenPositions(solution);
        m_boardWidget->update();
        appendToConsole("\n[Первое решение отображено на доске]\n");
    }
}

/**
 *  Выводит отформатированное решение в консоль
 *  solutionText Текст решения
 */
void MainWindow::onSolutionPrinted(const QString& solutionText)
{
    appendToConsole(solutionText);
}

/**
 *  Обрабатывает завершение поиска
 *  totalSolutions Общее количество найденных решений
 */
void MainWindow::onSolvingFinished(int totalSolutions)
{
    m_isSolving = false;
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_boardSizeSpinBox->setEnabled(true);
    m_applySizeButton->setEnabled(true);
    m_progressBar->setVisible(false);

    m_solutionsCountLabel->setText(QString("Найдено решений: %1").arg(totalSolutions));

    if (totalSolutions == 0) {
        appendToConsole("\n[РЕШЕНИЙ НЕ НАЙДЕНО]\n");
    } else {
        appendToConsole("\n========================================");
        appendToConsole("  ПОИСК ЗАВЕРШЁН");
        appendToConsole("========================================");
        appendToConsole("Всего найдено решений: " + QString::number(totalSolutions));
        appendToConsole("\n");
    }
}

/**
 *  Обрабатывает ошибку, возникшую при поиске
 *  errorMessage Текст ошибки
 */
void MainWindow::onError(const QString& errorMessage)
{
    QMessageBox::critical(this, "Ошибка", errorMessage);
    appendToConsole("\n[ОШИБКА] " + errorMessage + "\n");
    stopSolving();
}

/**
 *  Обрабатывает загрузку всех найденных решений
 *  solutions Вектор всех решений
 *
 * Активирует кнопки навигации после загрузки решений.
 */
void MainWindow::onAllSolutionsFound(const std::vector<std::vector<int>>& solutions)
{
    m_boardWidget->setSolutions(solutions);

    m_nextButton->setEnabled(true);
    m_prevButton->setEnabled(true);
    m_firstButton->setEnabled(true);
    m_showAllButton->setEnabled(true);

    appendToConsole(QString("\n[Загружено %1 решений. Используйте кнопки навигации]\n")
                        .arg(solutions.size()));
}

// ============================================================================
// Навигация по решениям
// ============================================================================

/**
 *  Переход к следующему решению
 */
void MainWindow::nextSolution()
{
    stopAnimation();
    m_boardWidget->nextSolution();
    int currentIndex = m_boardWidget->getCurrentSolutionIndex() + 1;
    int total = m_boardWidget->getAllSolutions().size();
    appendToConsole(QString("Переход к следующему решению (%1 из %2)")
                        .arg(currentIndex)
                        .arg(total));
    updateSolutionInfoWithWeight();
    m_boardWidget->refreshDisplay();
}

/**
 *  Переход к предыдущему решению
 */
void MainWindow::previousSolution()
{
    stopAnimation();
    m_boardWidget->previousSolution();
    int currentIndex = m_boardWidget->getCurrentSolutionIndex() + 1;
    int total = m_boardWidget->getAllSolutions().size();
    appendToConsole(QString("Переход к предыдущему решению (%1 из %2)")
                        .arg(currentIndex)
                        .arg(total));
    updateSolutionInfoWithWeight();
    m_boardWidget->refreshDisplay();
}

/**
 *  Переход к первому решению
 */
void MainWindow::firstSolution()
{
    stopAnimation();
    const auto& solutions = m_boardWidget->getAllSolutions();
    if (!solutions.empty()) {
        m_boardWidget->setCurrentSolutionIndex(0);
        m_boardWidget->setQueenPositions(solutions[0]);
        m_boardWidget->update();
        appendToConsole(QString("Переход к первому решению (1 из %1)")
                            .arg(solutions.size()));
        updateSolutionInfoWithWeight();
    }
}

// ============================================================================
// Анимация показа всех решений
// ============================================================================

/**
 *  Запускает анимацию последовательного показа всех решений
 *
 * Каждое решение отображается на доске в течение 600 мс.
 */
void MainWindow::showAllSolutions()
{
    if (m_animationRunning) stopAnimation();

    const auto& solutions = m_boardWidget->getAllSolutions();
    if (solutions.empty()) {
        appendToConsole("[!] Нет решений для отображения");
        return;
    }

    m_animationRunning = true;
    m_currentAnimationIndex = 0;
    m_showAllButton->setEnabled(false);
    m_stopAnimationButton->setEnabled(true);
    m_prevButton->setEnabled(false);
    m_nextButton->setEnabled(false);
    m_firstButton->setEnabled(false);

    appendToConsole("\n[Анимация: последовательный показ всех решений]");
    animateSolution(m_currentAnimationIndex);
}

/**
 *  Останавливает анимацию показа решений
 */
void MainWindow::stopAnimation()
{
    if (m_animationRunning) {
        m_animationRunning = false;
        m_stopAnimationButton->setEnabled(false);
        m_showAllButton->setEnabled(true);
        m_prevButton->setEnabled(true);
        m_nextButton->setEnabled(true);
        m_firstButton->setEnabled(true);
        appendToConsole("\n[Анимация остановлена]");
    }
}

/**
 *  Рекурсивно отображает решения с задержкой
 *  index Индекс текущего решения
 *
 * Использует QTimer::singleShot для создания задержки между решениями.
 */
void MainWindow::animateSolution(int index)
{
    if (!m_animationRunning) return;

    const auto& solutions = m_boardWidget->getAllSolutions();
    if (index >= static_cast<int>(solutions.size())) {
        // Анимация завершена
        m_animationRunning = false;
        m_stopAnimationButton->setEnabled(false);
        m_showAllButton->setEnabled(true);
        m_prevButton->setEnabled(true);
        m_nextButton->setEnabled(true);
        m_firstButton->setEnabled(true);
        appendToConsole(QString("\n[Анимация завершена. Показано %1 решений]").arg(solutions.size()));
        return;
    }

    // Отображаем текущее решение
    m_boardWidget->setQueenPositions(solutions[index]);
    m_boardWidget->update();
    appendToConsole(QString("  Решение %1 из %2").arg(index + 1).arg(solutions.size()));

    // Планируем показ следующего решения через 600 мс
    QTimer::singleShot(600, this, [this, index]() {
        animateSolution(index + 1);
    });
}

// ============================================================================
// Управление консолью
// ============================================================================

/**
 *  Выводит текст в консоль решений
 *  text Текст для вывода
 *
 * Автоматически прокручивает консоль вниз, чтобы показать новое сообщение.
 */
void MainWindow::appendToConsole(const QString& text)
{
    if (m_consoleTextEdit) {
        m_consoleTextEdit->append(text);
        // Прокручиваем к последней строке
        QScrollBar* scrollBar = m_consoleTextEdit->verticalScrollBar();
        if (scrollBar) scrollBar->setValue(scrollBar->maximum());
    }
}

/**
 *  Очищает консоль решений
 */
void MainWindow::clearConsole()
{
    if (m_consoleTextEdit) m_consoleTextEdit->clear();
}

/**
 *  Сохраняет содержимое консоли в текстовый файл
 *
 * Открывает диалог выбора файла и сохраняет всё содержимое консоли.
 */
void MainWindow::saveConsoleToFile()
{
    if (!m_consoleTextEdit) return;

    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "Сохранить консоль решений",
                                                    QString("solutions_%1x%2.txt")
                                                        .arg(m_boardSizeSpinBox->value())
                                                        .arg(m_boardSizeSpinBox->value()),
                                                    "Text files (*.txt);;All files (*)");

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << m_consoleTextEdit->toPlainText();
        file.close();
        QMessageBox::information(this, "Сохранение",
                                 QString("Консоль сохранена в файл:\n%1").arg(fileName));
    } else {
        QMessageBox::warning(this, "Ошибка",
                             QString("Не удалось сохранить файл:\n%1").arg(fileName));
    }
}

// ============================================================================
// Управление базой знаний
// ============================================================================

/**
 *  Открывает диалог управления базой знаний
 *
 * Позволяет просматривать и редактировать веса и цвета всех клеток.
 */
void MainWindow::openKnowledgeBaseDialog()
{
    // Создаем БЗ, если её еще нет
    if (!m_knowledgeBase) {
        m_knowledgeBase = new KnowledgeBase(this);
        m_knowledgeBase->initializeDatabase();
    }

    int boardSize = m_boardSizeSpinBox->value();

    // Если БЗ не инициализирована для текущего размера, генерируем
    if (m_knowledgeBase->getBoardSize() != boardSize || !m_knowledgeBase->isValid()) {
        generateDefaultTwoColorDistribution();
    }

    // Создаем и показываем диалог
    KnowledgeBaseDialog dialog(m_knowledgeBase, boardSize, this);
    dialog.exec();

    // Обновляем отображение доски
    m_boardWidget->refreshDisplay();

    // Предупреждаем, что для применения изменений нужен новый поиск
    if (m_boardWidget->getAllSolutions().size() > 0) {
        appendToConsole("[i] База знаний обновлена. Для применения новых цветов и весов выполните поиск заново.");
    }
}

/**
 *  Открывает диалог сортировки решений
 *
 * Позволяет отсортировать найденные решения по весу.
 */
void MainWindow::openSortDialog()
{
    if (m_boardWidget->getAllSolutions().empty()) {
        appendToConsole("[!] Нет решений для сортировки");
        return;
    }

    SortDialog dialog(this);
    // Подключаем временный сигнал к слоту сортировки
    connect(&dialog, &SortDialog::sortRequested, this, [this](bool ascending) {
        onSolutionsSorted(ascending);
    });
    dialog.exec();
}

/**
 *  Обрабатывает сортировку решений
 *  ascending true - по возрастанию, false - по убыванию
 *
 * Сортирует решения по суммарному весу и обновляет отображение.
 */
void MainWindow::onSolutionsSorted(bool ascending)
{
    if (m_solver) {
        // Выполняем сортировку
        m_solver->sortSolutionsByCost(ascending, 0);

        // Получаем отсортированные решения
        const auto& solutions = m_solver->getAllSolutions();
        const auto& solutionsWithWeight = m_solver->getSolutionsWithCost();

        // Обновляем виджет доски
        m_boardWidget->setSolutions(solutions);

        if (!solutions.empty()) {
            m_boardWidget->setCurrentSolutionIndex(0);
            m_boardWidget->setQueenPositions(solutions[0]);
            m_boardWidget->update();
        }

        // Выводим информацию о сортировке в консоль
        QString sortOrder = ascending ? "по возрастанию (от легких к тяжелым)"
                                      : "по убыванию (от тяжелых к легким)";
        appendToConsole(QString("\n РЕЗУЛЬТАТЫ СОРТИРОВКИ "));
        appendToConsole(QString(" Тип сортировки: по весу клеток"));
        appendToConsole(QString(" Порядок: %1").arg(sortOrder));
        appendToConsole(QString(" Всего решений: %1").arg(solutions.size()));
        appendToConsole(QString("\n"));

        // Выводим первые 10 решений
        int showCount = qMin(10, (int)solutionsWithWeight.size());
        appendToConsole(QString("Первые %1 отсортированных решений:\n").arg(showCount));

        for (int i = 0; i < showCount; ++i) {
            const auto& sol = solutionsWithWeight[i];
            QString colorName;
            switch (sol.colorMap.begin()->second) {
            case PositionColor::Red: colorName = "Красный"; break;
            case PositionColor::Green: colorName = "Зеленый"; break;
            case PositionColor::Blue: colorName = "Синий"; break;
            }

            appendToConsole(QString("  #%1: Общий вес=%2 | Цвет=%3")
                                .arg(i + 1)
                                .arg(sol.totalWeight)
                                .arg(colorName));
        }

        if (solutionsWithWeight.size() > 10) {
            appendToConsole(QString("  ... и еще %1 решений").arg(solutionsWithWeight.size() - 10));
        }

        // Обновляем информацию о текущем решении
        updateSolutionInfoWithWeight();

        // Активируем кнопки навигации
        m_prevButton->setEnabled(true);
        m_nextButton->setEnabled(true);
        m_firstButton->setEnabled(true);
        m_showAllButton->setEnabled(true);

        m_solutionsCountLabel->setText(QString("Решений: %1 (отсортировано)").arg(solutions.size()));

        m_boardWidget->refreshDisplay();
    }
}

/**
 *  Обновляет информацию о весе текущего решения в консоли
 *
 * Выводит детальную информацию:
 * - Общий вес решения
 * - Позиции всех ферзей
 * - Цвета ферзей
 * - Веса отдельных ферзей
 */
void MainWindow::updateSolutionInfoWithWeight()
{
    if (m_solver && m_boardWidget->getCurrentSolutionIndex() >= 0) {
        const auto& solutions = m_solver->getSolutionsWithCost();
        int index = m_boardWidget->getCurrentSolutionIndex();
        if (index < static_cast<int>(solutions.size())) {
            const auto& sol = solutions[index];

            // Выводим рамку с информацией
            appendToConsole(QString("  Решение #%1 из %2").arg(index + 1).arg(solutions.size()));
            appendToConsole(QString("  Общий вес решения: %1").arg(sol.totalWeight));

            // Выводим позиции (формат: A1, B5, C8, ...)
            QString positionsStr = "  Позиции: ";
            for (int col = 0; col < static_cast<int>(sol.positions.size()); ++col) {
                if (sol.positions[col] != -1) {
                    positionsStr += QString("%1%2 ").arg(QChar('A' + col)).arg(sol.positions[col] + 1);
                }
            }
            appendToConsole(positionsStr);

            // Выводим цвета каждого ферзя
            QString colorsStr = "  Цвета: ";
            for (auto it = sol.colorMap.begin(); it != sol.colorMap.end(); ++it) {
                QString colorName;
                switch (it->second) {
                case PositionColor::Red: colorName = "красный"; break;
                case PositionColor::Green: colorName = "зеленый"; break;
                case PositionColor::Blue: colorName = "синий"; break;
                }
                colorsStr += QString("%1:%2 ").arg(QChar('A' + it->first)).arg(colorName);
            }
            appendToConsole(colorsStr);

            // Выводим веса каждого ферзя
            QString weightsStr = "  Веса: ";
            for (int col = 0; col < static_cast<int>(sol.positions.size()); ++col) {
                if (sol.positions[col] != -1) {
                    int row = sol.positions[col] + 1;
                    int weight = m_knowledgeBase->getPositionWeight(col, row);
                    weightsStr += QString("%1:%2 ").arg(QChar('A' + col)).arg(weight);
                }
            }
            appendToConsole(weightsStr);
        }
    }
}

/**
 *  Обновляет отображение текущего решения
 *
 * Перерисовывает доску с текущей позицией ферзей.
 */
void MainWindow::updateCurrentSolutionDisplay()
{
    const auto& solutions = m_boardWidget->getAllSolutions();
    int currentIndex = m_boardWidget->getCurrentSolutionIndex();

    if (!solutions.empty() && currentIndex >= 0 && currentIndex < (int)solutions.size()) {
        m_boardWidget->setQueenPositions(solutions[currentIndex]);
        m_boardWidget->update();
        updateSolutionInfoWithWeight();
    }
}

// ============================================================================
// Режим редактирования и работа с БЗ
// ============================================================================

/**
 *  Включает/выключает режим редактирования базы знаний
 *  enabled true - режим включен, false - выключен
 *
 * В режиме редактирования клик по клетке открывает диалог изменения
 * веса и цвета этой клетки.
 */
void MainWindow::toggleEditMode(bool enabled)
{
    if (m_boardWidget) {
        m_boardWidget->setEditMode(enabled);
        appendToConsole(enabled ? "Режим редактирования включен. Нажимайте на клетки для изменения веса и цвета."
                                : "Режим редактирования выключен.");
    }
}

/**
 *  Генерирует случайное распределение весов и цветов (2 цвета)
 *
 * 70% зеленых клеток, 30% красных.
 * Веса случайные от 1 до 20.
 */
void MainWindow::randomizeAllKnowledge()
{
    if (!m_knowledgeBase) {
        m_knowledgeBase = new KnowledgeBase(this);
        m_knowledgeBase->initializeDatabase();
    }

    int boardSize = m_boardSizeSpinBox->value();
    m_knowledgeBase->clearAndSetBoardSize(boardSize);

    QRandomGenerator* gen = QRandomGenerator::global();

    for (int col = 0; col < boardSize; ++col) {
        for (int row = 1; row <= boardSize; ++row) {
            int weight = gen->bounded(1, 21);  // Вес от 1 до 20

            // 70% зеленых, 30% красных
            int colorRand = gen->bounded(100);
            PositionColor color;
            if (colorRand < 70) {
                color = PositionColor::Green;
            } else {
                color = PositionColor::Red;
            }

            m_knowledgeBase->setPositionWeight(col, row, weight);
            m_knowledgeBase->setPositionColor(col, row, color);
        }
    }

    if (m_boardWidget) {
        m_boardWidget->setKnowledgeBase(m_knowledgeBase);
        m_boardWidget->refreshDisplay();
        m_boardWidget->update();
    }

    // Подсчитываем и выводим статистику
    int greenCount = 0, redCountActual = 0, blueCount = 0;
    int totalWeight = 0;

    for (int col = 0; col < boardSize; ++col) {
        for (int row = 1; row <= boardSize; ++row) {
            PositionColor color = m_knowledgeBase->getPositionColor(col, row);
            if (color == PositionColor::Green) greenCount++;
            else if (color == PositionColor::Red) redCountActual++;
            else blueCount++;
            totalWeight += m_knowledgeBase->getPositionWeight(col, row);
        }
    }

    int total = boardSize * boardSize;
    int avgWeight = totalWeight / total;

    appendToConsole(QString("База знаний: зеленый=%1 (%2%) красный=%3 (%4%)")
                        .arg(greenCount).arg(greenCount * 100 / total)
                        .arg(redCountActual).arg(redCountActual * 100 / total));
    appendToConsole(QString("Средний вес клетки: %1").arg(avgWeight));
    appendToConsole("У всех клеток случайные веса (1-20)");
}

/**
 *  Сбрасывает базу знаний - все клетки становятся зелеными со случайными весами
 *
 * Веса: случайные от 1 до 20.
 */
void MainWindow::resetAllKnowledge()
{
    if (!m_knowledgeBase) {
        m_knowledgeBase = new KnowledgeBase(this);
        m_knowledgeBase->initializeDatabase();
    }

    int boardSize = m_boardSizeSpinBox->value();
    m_knowledgeBase->clearAndSetBoardSize(boardSize);

    QRandomGenerator* gen = QRandomGenerator::global();

    for (int col = 0; col < boardSize; ++col) {
        for (int row = 1; row <= boardSize; ++row) {
            int weight = gen->bounded(1, 21);
            m_knowledgeBase->setPositionWeight(col, row, weight);
            m_knowledgeBase->setPositionColor(col, row, PositionColor::Green);
        }
    }

    m_boardWidget->refreshDisplay();
    m_boardWidget->update();

    appendToConsole("База знаний сброшена: ВСЕ клетки ЗЕЛЕНЫЕ");
    appendToConsole("Вес каждой клетки: случайный (1-20)");
}

/**
 *  Генерирует базу знаний по умолчанию - все клетки зеленые со случайными весами
 *
 * Используется при инициализации нового размера доски.
 */
void MainWindow::generateDefaultAllGreen()
{
    if (!m_knowledgeBase) {
        m_knowledgeBase = new KnowledgeBase(this);
        m_knowledgeBase->initializeDatabase();
    }

    int boardSize = m_boardSizeSpinBox->value();
    m_knowledgeBase->clearAndSetBoardSize(boardSize);

    QRandomGenerator* gen = QRandomGenerator::global();

    for (int col = 0; col < boardSize; ++col) {
        for (int row = 1; row <= boardSize; ++row) {
            int weight = gen->bounded(1, 21);
            m_knowledgeBase->setPositionWeight(col, row, weight);
            m_knowledgeBase->setPositionColor(col, row, PositionColor::Green);
        }
    }

    if (m_boardWidget) {
        m_boardWidget->setKnowledgeBase(m_knowledgeBase);
        m_boardWidget->refreshDisplay();
        m_boardWidget->update();
    }

    // Вычисляем и выводим средний вес
    int totalWeight = 0;
    for (int col = 0; col < boardSize; ++col) {
        for (int row = 1; row <= boardSize; ++row) {
            totalWeight += m_knowledgeBase->getPositionWeight(col, row);
        }
    }
    int avgWeight = totalWeight / (boardSize * boardSize);

    appendToConsole("База знаний: ВСЕ клетки ЗЕЛЕНЫЕ");
    appendToConsole(QString("Средний вес клетки: %1 (1-20)").arg(avgWeight));
    appendToConsole("Теперь можно нажать 'Старт' для поиска решений");
}

/**
 *  Генерирует двухцветное распределение (зеленый и красный, 50/50)
 *
 * Используется как альтернатива зеленому распределению по умолчанию.
 */
void MainWindow::generateDefaultTwoColorDistribution()
{
    if (!m_knowledgeBase) {
        m_knowledgeBase = new KnowledgeBase(this);
        m_knowledgeBase->initializeDatabase();
    }

    int boardSize = m_boardSizeSpinBox->value();
    m_knowledgeBase->generateTwoColorDistribution(boardSize);

    m_boardWidget->refreshDisplay();
    m_boardWidget->update();

    appendToConsole("Сгенерировано распределение цветов (2 цвета: зеленый и красный)");
    appendToConsole("Правило: все ферзи должны быть одного цвета");
}

// ============================================================================
// Обработка изменений в базе знаний
// ============================================================================

/**
 *  Обрабатывает изменение веса клетки
 *  col Столбец клетки
 *  row Строка клетки (1-индексация)
 *  newWeight Новый вес
 *
 * При изменении веса пересчитываются веса всех решений.
 */
void MainWindow::onCellWeightChanged(int col, int row, int newWeight)
{
    appendToConsole(QString("  Клетка %1%2: вес изменен на %3")
                        .arg(QChar('A' + col))
                        .arg(row)
                        .arg(newWeight));

    // Пересчитываем веса всех решений, если они есть
    if (m_solver && !m_solver->getAllSolutions().empty()) {
        appendToConsole("\n[Пересчет весов решений...]");

        // Получаем текущие решения
        const auto& solutions = m_solver->getAllSolutions();
        auto& solutionsWithCost = const_cast<std::vector<SolutionWithCost>&>(m_solver->getSolutionsWithCost());

        // Пересчитываем вес для каждого решения
        for (auto& sol : solutionsWithCost) {
            int totalWeight = 0;
            for (size_t c = 0; c < sol.positions.size(); ++c) {
                if (sol.positions[c] != -1) {
                    int r = sol.positions[c] + 1;
                    totalWeight += m_knowledgeBase->getPositionWeight(c, r);
                }
            }
            sol.totalWeight = totalWeight;
        }

        // Обновляем отображение текущего решения
        int currentIndex = m_boardWidget->getCurrentSolutionIndex();
        if (currentIndex >= 0 && currentIndex < (int)solutionsWithCost.size()) {
            const auto& currentSol = solutionsWithCost[currentIndex];
            appendToConsole(QString("  Текущее решение: новый вес = %1").arg(currentSol.totalWeight));
        }

        appendToConsole("Веса решений пересчитаны\n");

        // Обновляем информацию в консоли
        updateSolutionInfoWithWeight();

        // Обновляем доску
        m_boardWidget->refreshDisplay();
    }
}

/**
 *  Обрабатывает изменение цвета клетки
 *  col Столбец клетки
 *  row Строка клетки (1-индексация)
 *  newColor Новый цвет
 *
 * Выводит информацию об изменении в консоль.
 */
void MainWindow::onCellColorChanged(int col, int row, PositionColor newColor)
{
    QString colorName;
    switch (newColor) {
    case PositionColor::Red: colorName = "Красный"; break;
    case PositionColor::Green: colorName = "Зеленый"; break;
    case PositionColor::Blue: colorName = "Синий"; break;
    }
    appendToConsole(QString("  Клетка %1%2: цвет изменен на %3")
                        .arg(QChar('A' + col))
                        .arg(row)
                        .arg(colorName));
}

// ============================================================================
// Обработчики событий окна
// ============================================================================

/**
 *  Обрабатывает изменение размера окна
 *  event Событие изменения размера
 *
 * Адаптирует высоту консоли под новое окно.
 */
void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    // Устанавливаем максимальную высоту консоли пропорционально окну
    if (m_consoleTextEdit) {
        int newHeight = qMax(200, height() - 300);
        m_consoleTextEdit->setMaximumHeight(newHeight);
    }
}

/**
 *  Обрабатывает закрытие окна
 *  event Событие закрытия
 *
 * Останавливает анимацию и поиск перед закрытием.
 */
void MainWindow::closeEvent(QCloseEvent* event)
{
    stopAnimation();
    stopSolving();
    event->accept();
}

// ============================================================================
// Инициализация менеджера приоритетов
// ============================================================================

void MainWindow::initPriorityManager()
{
    m_priorityManager = new PriorityManager(this);
    if (m_boardWidget) {
        m_boardWidget->setPriorityManager(m_priorityManager);
    }
}

// ============================================================================
// Управление приоритетами
// ============================================================================

/**
 *  Включает/выключает режим отображения и редактирования приоритетов
 *  В этом режиме над доской появляются индикаторы приоритетов
 */
void MainWindow::togglePriorityMode(bool enabled)
{
    if (m_boardWidget) {
        m_boardWidget->setPriorityMode(enabled);
    }
    appendToConsole(enabled ?
                        "Режим приоритетов включен. Над доской появились индикаторы." :
                        "Режим приоритетов выключен.");
}


/**
 *  Применяет текущие приоритеты и фиксации к решателю
 *  Вызывается автоматически при изменении настроек через диалог
 */

void MainWindow::applyPrioritiesToAgents()
{
    if (!m_priorityManager) {
        return;
    }

    ensureSolverExists();

    if (!m_solver) {
        return;
    }

    int boardSize = m_boardSizeSpinBox->value();

    // Собираем фиксированные позиции из менеджера приоритетов
    std::vector<std::optional<int>> fixedPositions;
    fixedPositions.resize(boardSize);

    for (int col = 0; col < boardSize; ++col) {
        fixedPositions[col] = m_priorityManager->getFixedPosition(col);
    }

    // Передаём в решатель
    m_solver->setFixedPositions(fixedPositions);
    m_solver->setPriorityManager(m_priorityManager);

    // Обновляем отображение ферзей на доске
    if (m_boardWidget) {
        std::vector<int> currentPositions = m_boardWidget->getQueenPositions();
        if (static_cast<int>(currentPositions.size()) < boardSize) {
            currentPositions.resize(boardSize, -1);
        }

        // Устанавливаем зафиксированные позиции
        for (int col = 0; col < boardSize; ++col) {
            auto fixed = m_priorityManager->getFixedPosition(col);
            if (fixed.has_value() && fixed.value() >= 0 && fixed.value() < boardSize) {
                currentPositions[col] = fixed.value();
            }
        }

        m_boardWidget->setQueenPositions(currentPositions);
        m_boardWidget->refreshDisplay();
    }
}

void MainWindow::ensureSolverExists()
{
    int boardSize = m_boardSizeSpinBox->value();

    if (m_solver && m_solver->getBoardSize() == boardSize) {
        return;
    }

    if (!m_knowledgeBase) {
        m_knowledgeBase = new KnowledgeBase(this);
        m_knowledgeBase->initializeDatabase();
    }

    if (m_solver) {
        delete m_solver;
    }

    m_solver = new QueensSolver(boardSize, m_knowledgeBase, this);

    // Подключаем сигналы (скопируйте из startSolving)
    connect(m_solver, &QueensSolver::solutionFound, this, &MainWindow::onSolutionFound);
    connect(m_solver, &QueensSolver::solutionPrinted, this, &MainWindow::onSolutionPrinted);
    connect(m_solver, &QueensSolver::progressUpdated, this, &MainWindow::updateProgress);
    connect(m_solver, &QueensSolver::finished, this, &MainWindow::onSolvingFinished);
    connect(m_solver, &QueensSolver::error, this, &MainWindow::onError);
    connect(m_solver, &QueensSolver::allSolutionsFound, this, &MainWindow::onAllSolutionsFound);
}

/**
 *  Обработчик сигнала об изменении приоритетов от ChessBoardWidget
 */
void MainWindow::onPrioritiesChanged()
{
    // Применяем изменения приоритетов к решателю
    applyPrioritiesToAgents();

    // Обновляем отображение на доске
    if (m_boardWidget && m_priorityManager) {
        int boardSize = m_boardSizeSpinBox->value();

        // Получаем текущие позиции с доски
        std::vector<int> currentPositions = m_boardWidget->getQueenPositions(); // Нужен геттер

        // Обновляем позиции с учетом фиксаций
        for (int col = 0; col < boardSize; ++col) {
            auto fixed = m_priorityManager->getFixedPosition(col);
            if (fixed.has_value() && fixed.value() >= 0 && fixed.value() < boardSize) {
                // Если позиция зафиксирована, устанавливаем её
                if (col < static_cast<int>(currentPositions.size())) {
                    currentPositions[col] = fixed.value();
                }
            }
        }

        m_boardWidget->setQueenPositions(currentPositions);
        m_boardWidget->refreshDisplay();
        m_boardWidget->update();
        m_boardWidget->repaint();  // Немедленная перерисовка
    }
}
// ============================================================================
// Справочная информация при запуске
// ============================================================================

/**
 *  Выводит в консоль подробную справочную информацию о возможностях программы
 *  Особое внимание уделено работе с приоритетами и фиксацией позиций
 */

void MainWindow::showWelcomeMessage()
{
    appendToConsole(" ЗАДАЧА О ФЕРЗЯХ - СПРАВКА ПО ПРИОРИТЕТАМ");
    appendToConsole("\n ПРИОРИТЕТЫ СТОЛБЦОВ (0-10):");
    appendToConsole("  0 = нет приоритета (обычный порядок)");
    appendToConsole("  1 = НАИВЫСШИЙ приоритет ★★★ (первый в переборе)");
    appendToConsole("  2-3 = Высокий приоритет ★★ / ★");
    appendToConsole("  4-10 = Обычный/низкий приоритет");
    appendToConsole(" Чем МЕНЬШЕ число, тем ВЫШЕ приоритет!");
    appendToConsole("\n ФИКСАЦИЯ ПОЗИЦИЙ:");
    appendToConsole(" Ферзь НЕ двигается при поиске");
    appendToConsole(" Отображается БЕЛЫМ цветом ");
    appendToConsole("");
}

// ============================================================================
// Сброс состояния доски
// ============================================================================

/**
 *  Полный сброс состояния доски:
 *  - Все приоритеты очищены
 *  - Все фиксации сняты
 *  - База знаний сброшена (все клетки зеленые, вес = 1)
 *  - Ферзи убраны с доски
 *  - Список решений очищен
 */
void MainWindow::resetBoardState()
{
    // Останавливаем поиск и анимацию
    if (m_isSolving) {
        stopSolving();
    }
    stopAnimation();

    int boardSize = m_boardSizeSpinBox->value();

    // 1. Сбрасываем приоритеты и фиксации
    if (m_priorityManager) {
        m_priorityManager->reset();
    }

    // 2. Очищаем позиции ферзей на доске
    if (m_boardWidget) {
        m_boardWidget->clearQueens();
        m_boardWidget->setSolutions({});
    }

    // 3. Обновляем спинбоксы приоритетов
    if (m_boardWidget) {
        // Выключаем режим приоритетов
        if (m_priorityModeCheckBox) {
            m_priorityModeCheckBox->setChecked(false);
        }
        m_boardWidget->setPriorityMode(false);
        m_boardWidget->update();
        m_boardWidget->repaint();
    }

    // 4. Сбрасываем список решений
    m_currentSolutionsCount = 0;
    m_solutionsCountLabel->setText("Решений: 0");

    // 5. Отключаем кнопки навигации
    m_prevButton->setEnabled(false);
    m_nextButton->setEnabled(false);
    m_firstButton->setEnabled(false);
    m_showAllButton->setEnabled(false);
    m_stopAnimationButton->setEnabled(false);

    // 6. Сбрасываем решатель
    if (m_solver) {
        std::vector<std::optional<int>> emptyFixed;
        emptyFixed.resize(boardSize, std::nullopt);
        m_solver->setFixedPositions(emptyFixed);
    }

    // 7. Выводим сообщение в консоль
    appendToConsole("\n СОСТОЯНИЕ ДОСКИ СБРОШЕНО");
    appendToConsole("Все приоритеты очищены");
    appendToConsole("Все фиксации сняты");
    appendToConsole("Ферзи убраны с доски");
    appendToConsole("Список решений очищен");
    appendToConsole("Режим приоритетов выключен");

    appendToConsole("[i] Доска готова к новому поиску.");
    appendToConsole("    Вы можете задать новые приоритеты и фиксации.\n");
}