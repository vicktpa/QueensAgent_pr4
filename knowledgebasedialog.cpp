#include "KnowledgeBaseDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QColorDialog>

// ============================================================================
// Конструктор и деструктор
// ============================================================================

KnowledgeBaseDialog::KnowledgeBaseDialog(KnowledgeBase* knowledgeBase, int boardSize, QWidget* parent)
    : QDialog(parent)
    , m_knowledgeBase(knowledgeBase)
    , m_boardSize(boardSize)
{
    setupUI();                // Создаем интерфейс
    loadDataToTable();        // Загружаем данные из БЗ
    updateStatistics();       // Обновляем статистику
    setWindowTitle("База знаний - Управление весами и цветами");
    resize(700, 600);         // Устанавливаем размер окна
}

KnowledgeBaseDialog::~KnowledgeBaseDialog()
{
}

// ============================================================================
// Инициализация интерфейса
// ============================================================================

/**
 *  Создает и настраивает пользовательский интерфейс диалога
 *
 * Структура интерфейса:
 * - Таблица N x N для отображения всех клеток
 * - Панель со статистикой (средний вес, распределение цветов)
 * - Панель с кнопками управления
 * - Информационная строка
 */
void KnowledgeBaseDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ========================================================================
    // Таблица для отображения позиций
    // ========================================================================
    m_tableWidget = new QTableWidget(m_boardSize, m_boardSize, this);

    // Устанавливаем заголовки столбцов (A, B, C, ...)
    QStringList colHeaders;
    for (int i = 0; i < m_boardSize; ++i) {
        colHeaders << QString(char('A' + i));
    }
    m_tableWidget->setHorizontalHeaderLabels(colHeaders);

    // Устанавливаем заголовки строк (1, 2, 3, ...)
    QStringList rowHeaders;
    for (int i = 1; i <= m_boardSize; ++i) {
        rowHeaders << QString::number(i);
    }
    m_tableWidget->setVerticalHeaderLabels(rowHeaders);

    // Растягиваем заголовки для равномерного распределения пространства
    m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Создаем пустые ячейки таблицы
    for (int row = 0; row < m_boardSize; ++row) {
        for (int col = 0; col < m_boardSize; ++col) {
            QTableWidgetItem* item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            m_tableWidget->setItem(row, col, item);
        }
    }

    // Подключаем сигнал изменения ячейки
    connect(m_tableWidget, &QTableWidget::cellChanged, this, &KnowledgeBaseDialog::onCellChanged);

    mainLayout->addWidget(m_tableWidget);

    // ========================================================================
    // Панель статистики
    // ========================================================================
    QHBoxLayout* statsLayout = new QHBoxLayout();
    m_statsLabel = new QLabel("Статистика:", this);
    m_avgWeightLabel = new QLabel("Средний вес: --", this);
    m_redCountLabel = new QLabel("Красных: --", this);
    m_greenCountLabel = new QLabel("Зеленых: --", this);
    m_blueCountLabel = new QLabel("Синих: --", this);

    statsLayout->addWidget(m_statsLabel);
    statsLayout->addWidget(m_avgWeightLabel);
    statsLayout->addWidget(m_redCountLabel);
    statsLayout->addWidget(m_greenCountLabel);
    statsLayout->addWidget(m_blueCountLabel);
    statsLayout->addStretch();

    mainLayout->addLayout(statsLayout);

    // ========================================================================
    // Кнопки управления
    // ========================================================================
    QGridLayout* buttonLayout = new QGridLayout();

    m_loadButton = new QPushButton("Загрузить", this);
    m_saveButton = new QPushButton("Сохранить", this);
    m_allGreenButton = new QPushButton("Все зеленые", this);
    m_twoColorButton = new QPushButton("2 цвета (50/50)", this);
    m_threeColorButton = new QPushButton("3 цвета", this);
    m_applyButton = new QPushButton("Применить", this);
    m_cancelButton = new QPushButton("Закрыть", this);

    // Располагаем кнопки в сетке 2x4
    buttonLayout->addWidget(m_loadButton, 0, 0);
    buttonLayout->addWidget(m_saveButton, 0, 1);
    buttonLayout->addWidget(m_allGreenButton, 0, 2);
    buttonLayout->addWidget(m_twoColorButton, 0, 3);
    buttonLayout->addWidget(m_threeColorButton, 1, 0);
    buttonLayout->addWidget(m_applyButton, 1, 2);
    buttonLayout->addWidget(m_cancelButton, 1, 3);

    mainLayout->addLayout(buttonLayout);

    // ========================================================================
    // Информационная строка
    // ========================================================================
    QLabel* infoLabel = new QLabel("Для редактирования: дважды кликните по ячейке и измените значение (вес/цвет)", this);
    infoLabel->setStyleSheet("color: gray; font-size: 10px;");
    mainLayout->addWidget(infoLabel);

    // ========================================================================
    // Подключение сигналов кнопок
    // ========================================================================
    connect(m_loadButton, &QPushButton::clicked, this, &KnowledgeBaseDialog::loadFromFile);
    connect(m_saveButton, &QPushButton::clicked, this, &KnowledgeBaseDialog::saveToFile);
    connect(m_allGreenButton, &QPushButton::clicked, this, &KnowledgeBaseDialog::generateAllGreen);
    connect(m_twoColorButton, &QPushButton::clicked, this, &KnowledgeBaseDialog::generateTwoColor);
    connect(m_threeColorButton, &QPushButton::clicked, this, &KnowledgeBaseDialog::generateThreeColor);
    connect(m_applyButton, &QPushButton::clicked, this, &KnowledgeBaseDialog::applyChanges);
    connect(m_cancelButton, &QPushButton::clicked, this, &KnowledgeBaseDialog::cancelChanges);
}

// ============================================================================
// Загрузка и сохранение данных
// ============================================================================

/**
 *  Загружает данные из базы знаний в таблицу
 *
 * Для каждой клетки:
 * - Получает вес и цвет из БЗ
 * - Формирует текст "вес\nцвет"
 * - Устанавливает цвет фона в соответствии с цветом клетки
 * - Сохраняет данные в UserRole для последующего использования
 */
void KnowledgeBaseDialog::loadDataToTable()
{
    if (!m_knowledgeBase || !m_knowledgeBase->isValid()) return;

    for (int row = 0; row < m_boardSize; ++row) {
        int actualRow = row + 1;  // В БЗ строки с 1
        for (int col = 0; col < m_boardSize; ++col) {
            int weight = m_knowledgeBase->getPositionWeight(col, actualRow);
            PositionColor color = m_knowledgeBase->getPositionColor(col, actualRow);

            // Определяем строковое представление цвета и цвет фона
            QString colorStr;
            QColor bgColor;
            switch (color) {
            case PositionColor::Red:
                colorStr = "Красный";
                bgColor = QColor(255, 200, 200);  // Светло-красный
                break;
            case PositionColor::Green:
                colorStr = "Зеленый";
                bgColor = QColor(200, 255, 200);  // Светло-зеленый
                break;
            case PositionColor::Blue:
                colorStr = "Синий";
                bgColor = QColor(200, 200, 255);  // Светло-синий
                break;
            }

            // Заполняем ячейку
            QTableWidgetItem* item = m_tableWidget->item(row, col);
            if (item) {
                item->setText(QString("%1\n%2").arg(weight).arg(colorStr));
                item->setBackground(bgColor);
                // Сохраняем оригинальные данные для отслеживания изменений
                item->setData(Qt::UserRole, static_cast<int>(color));
                item->setData(Qt::UserRole + 1, weight);
            }
        }
    }

    updateTableColors();
}

/**
 *  Сохраняет данные из таблицы в базу знаний
 *
 * Проходит по всем ячейкам таблицы, парсит текст
 * (вес и цвет) и обновляет БЗ.
 */
void KnowledgeBaseDialog::saveDataFromTable()
{
    if (!m_knowledgeBase) return;

    for (int row = 0; row < m_boardSize; ++row) {
        int actualRow = row + 1;
        for (int col = 0; col < m_boardSize; ++col) {
            QTableWidgetItem* item = m_tableWidget->item(row, col);
            if (item) {
                // Парсим текст ячейки (формат: "вес\nцвет")
                QString text = item->text();
                QStringList parts = text.split('\n');

                // Обновляем вес
                if (parts.size() >= 1) {
                    int weight = parts[0].toInt();
                    m_knowledgeBase->setPositionWeight(col, actualRow, weight);
                }

                // Обновляем цвет
                if (parts.size() >= 2) {
                    PositionColor color;
                    if (parts[1].contains("Красный")) color = PositionColor::Red;
                    else if (parts[1].contains("Зеленый")) color = PositionColor::Green;
                    else if (parts[1].contains("Синий")) color = PositionColor::Blue;
                    else color = PositionColor::Green;  // По умолчанию

                    m_knowledgeBase->setPositionColor(col, actualRow, color);
                }
            }
        }
    }
}

// ============================================================================
// Обработчики событий таблицы
// ============================================================================

/**
 *  Обработчик изменения содержимого ячейки
 *  row Номер строки
 *  column Номер столбца
 *
 * При изменении ячейки обновляет цвета фона и статистику.
 */
void KnowledgeBaseDialog::onCellChanged(int row, int column)
{
    Q_UNUSED(row)
    Q_UNUSED(column)
    updateTableColors();   // Обновляем цвета ячеек
    updateStatistics();    // Пересчитываем статистику
}

/**
 *  Обновляет цвета фона всех ячеек в соответствии с выбранным цветом
 *
 * Для каждой ячейки парсит текст, определяет цвет и устанавливает
 * соответствующий цвет фона.
 */
void KnowledgeBaseDialog::updateTableColors()
{
    for (int row = 0; row < m_boardSize; ++row) {
        for (int col = 0; col < m_boardSize; ++col) {
            QTableWidgetItem* item = m_tableWidget->item(row, col);
            if (item) {
                QString text = item->text();
                QStringList parts = text.split('\n');

                // Определяем цвет фона на основе текста ячейки
                QColor bgColor;
                if (parts.size() >= 2) {
                    if (parts[1].contains("Красный")) bgColor = QColor(255, 200, 200);
                    else if (parts[1].contains("Зеленый")) bgColor = QColor(200, 255, 200);
                    else if (parts[1].contains("Синий")) bgColor = QColor(200, 200, 255);
                    else bgColor = QColor(200, 200, 200);
                }
                item->setBackground(bgColor);
            }
        }
    }
}

// ============================================================================
// Статистика
// ============================================================================

/**
 *  Обновляет отображение статистики
 *
 * Вычисляет и отображает:
 * - Средний вес по всем клеткам
 * - Количество клеток каждого цвета
 */
void KnowledgeBaseDialog::updateStatistics()
{
    if (!m_knowledgeBase) return;

    int avgWeight = m_knowledgeBase->getAverageWeight();
    auto distribution = m_knowledgeBase->getColorDistribution();

    m_avgWeightLabel->setText(QString("Средний вес: %1").arg(avgWeight));
    m_redCountLabel->setText(QString("Красных: %1").arg(distribution[PositionColor::Red]));
    m_greenCountLabel->setText(QString("Зеленых: %1").arg(distribution[PositionColor::Green]));
    m_blueCountLabel->setText(QString("Синих: %1").arg(distribution[PositionColor::Blue]));
}

// ============================================================================
// Действия с файлами
// ============================================================================

void KnowledgeBaseDialog::loadFromFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Загрузить базу знаний",
                                                    QString(), "Text files (*.txt);;All files (*)");
    if (fileName.isEmpty()) return;

    if (m_knowledgeBase->loadFromFile(fileName)) {
        loadDataToTable();
        updateStatistics();
        QMessageBox::information(this, "Успех", "База знаний загружена");
    } else {
        QMessageBox::warning(this, "Ошибка", "Не удалось загрузить базу знаний");
    }
}

void KnowledgeBaseDialog::saveToFile()
{
    // Сначала сохраняем текущие данные из таблицы в БЗ
    saveDataFromTable();

    QString fileName = QFileDialog::getSaveFileName(this, "Сохранить базу знаний",
                                                    QString("knowledge_N%1.txt").arg(m_boardSize),
                                                    "Text files (*.txt);;All files (*)");
    if (fileName.isEmpty()) return;

    if (m_knowledgeBase->saveToFile(fileName)) {
        QMessageBox::information(this, "Успех", "База знаний сохранена");
    } else {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить базу знаний");
    }
}

// ============================================================================
// Генерация данных
// ============================================================================

void KnowledgeBaseDialog::generateRandom()
{
    m_knowledgeBase->generateRandomKnowledge(m_boardSize);
    loadDataToTable();
    updateStatistics();
}

void KnowledgeBaseDialog::generateTwoColor()
{
    m_knowledgeBase->generateTwoColorDistribution(m_boardSize);
    loadDataToTable();
    updateStatistics();
    QMessageBox::information(this, "Успех",
                             "Сгенерировано 2 цвета: 50% зеленых, 50% красных");
}

void KnowledgeBaseDialog::generateThreeColor()
{
    m_knowledgeBase->generateRandomKnowledge(m_boardSize);
    loadDataToTable();
    updateStatistics();
    QMessageBox::information(this, "Успех",
                             "Сгенерировано 3 цвета: равномерное распределение");
}

void KnowledgeBaseDialog::generateAllGreen()
{
    // Устанавливаем всем клеткам вес 50 и зеленый цвет
    for (int col = 0; col < m_boardSize; ++col) {
        for (int row = 1; row <= m_boardSize; ++row) {
            m_knowledgeBase->setPositionWeight(col, row, 50);
            m_knowledgeBase->setPositionColor(col, row, PositionColor::Green);
        }
    }
    loadDataToTable();
    updateStatistics();
    QMessageBox::information(this, "Успех",
                             "Все клетки установлены в ЗЕЛЕНЫЙ цвет с весом 50");
}

// ============================================================================
// Применение и отмена изменений
// ============================================================================

void KnowledgeBaseDialog::applyChanges()
{
    saveDataFromTable();      // Сохраняем изменения из таблицы в БЗ
    updateStatistics();       // Обновляем статистику
    QMessageBox::information(this, "Успех", "Изменения применены");
}

void KnowledgeBaseDialog::cancelChanges()
{
    loadDataToTable();        // Перезагружаем данные из БЗ (отменяем изменения)
    updateStatistics();       // Обновляем статистику
    close();                  // Закрываем диалог
}