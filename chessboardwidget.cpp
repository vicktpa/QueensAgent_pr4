#include "ChessBoardWidget.h"
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QMouseEvent>
#include <QInputDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <cmath>

// ============================================================================
// Конструктор и основные методы
// ============================================================================

ChessBoardWidget::ChessBoardWidget(QWidget* parent)
    : QWidget(parent)
    , m_boardSize(8)           // Размер доски по умолчанию 8x8
    , m_cellSize(60)           // Начальный размер клетки 60 пикселей
    , m_currentSolutionIndex(0) // Начинаем с первого решения
    , m_priorityManager(nullptr)
    , m_priorityMode(false)
    , m_knowledgeBase(nullptr)  // База знаний пока не подключена
    , m_editMode(false)         // Режим редактирования выключен
{
    // Устанавливаем минимальный размер виджета для корректного отображения
    setMinimumSize(400, 400);
    // Политика расширения - виджет может растягиваться
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // Включаем отслеживание движения мыши для изменения курсора
    setMouseTracking(true);
}

/**
 *  Устанавливает новый размер доски
 *  size Новый размер (N для доски N x N)
 *
 * При изменении размера доски сбрасываются все позиции ферзей,
 * так как старые позиции могут быть некорректными для нового размера.
 */
void ChessBoardWidget::setBoardSize(int size){
    m_boardSize = size;
    // Инициализируем вектор позиций ферзей значением -1 (нет ферзя)
    m_queenPositions.assign(m_boardSize, -1);
    // Запрашиваем перерисовку
    update();
}

/**
 *  Устанавливает позиции ферзей из вектора
 *  positions Вектор, где индекс - столбец, значение - строка ферзя
 *
 * Если переданный вектор короче размера доски, недостающие столбцы
 * заполняются значением -1 (нет ферзя).
 */
void ChessBoardWidget::setQueenPositions(const std::vector<int>& positions)
{
    if (positions.size() >= static_cast<size_t>(m_boardSize)) {
        m_queenPositions = positions;
        m_queenPositions.resize(m_boardSize, -1);
    } else {
        m_queenPositions.assign(m_boardSize, -1);
        for (size_t i = 0; i < positions.size(); ++i)
            if (i < static_cast<size_t>(m_boardSize))
                m_queenPositions[i] = positions[i];
    }

    // Принудительно перерисовываем
    update();
    repaint();
}

/**
 *  Очищает все ферзи с доски
 */
void ChessBoardWidget::clearQueens(){
    m_queenPositions.assign(m_boardSize, -1);
    update();
}

/**
 *  Переход к следующему решению (по кольцу)
 */
void ChessBoardWidget::nextSolution(){
    if (m_allSolutions.empty()) return;
    // Инкрементируем индекс по модулю количества решений
    m_currentSolutionIndex = (m_currentSolutionIndex + 1) % m_allSolutions.size();
    setQueenPositions(m_allSolutions[m_currentSolutionIndex]);
}

/**
 *  Переход к предыдущему решению (по кольцу)
 */
void ChessBoardWidget::previousSolution(){
    if (m_allSolutions.empty()) return;
    // Декрементируем индекс с учетом модуля
    m_currentSolutionIndex = (m_currentSolutionIndex - 1 + m_allSolutions.size()) % m_allSolutions.size();
    setQueenPositions(m_allSolutions[m_currentSolutionIndex]);
}

/**
 *  Устанавливает все найденные решения
 *  solutions Вектор всех решений (каждое решение - вектор позиций ферзей)
 *
 * Сохраняет все решения и отображает первое из них.
 */
void ChessBoardWidget::setSolutions(const std::vector<std::vector<int>>& solutions){
    m_allSolutions = solutions;
    m_currentSolutionIndex = 0;
    if (!solutions.empty())
        setQueenPositions(solutions[0]);
    update();
}

/**
 *  Обновляет отображение с пересчетом размера клеток
 *
 * Вызывается при изменении размера окна или принудительном обновлении.
 */
void ChessBoardWidget::refreshDisplay(){
    // Пересчитываем размер клеток при изменении размера виджета
    int widgetSize = qMin(width(), height());
    if (widgetSize > 0) {
        m_cellSize = widgetSize / m_boardSize;
    }
    update();  // Вызываем перерисовку
}

// ============================================================================
// Обработка событий мыши (для режима редактирования)
// ============================================================================

/**
 *  Обработчик нажатия кнопки мыши
 *
 * В режиме редактирования при клике на клетку открывает диалог
 * для изменения веса и цвета этой клетки в базе знаний.
 */
void ChessBoardWidget::mousePressEvent(QMouseEvent* event){
    if (event->button() == Qt::LeftButton && m_editMode) {
        QPair<int, int> cell = getCellAtPosition(event->pos());
        int col = cell.first;
        int row = cell.second;

        if (col >= 0 && col < m_boardSize && row >= 0 && row < m_boardSize) {
            showEditCellDialog(col, row);
        }
    }
    QWidget::mousePressEvent(event);
}

/**
 *  Обработчик движения мыши
 *
 * В режиме редактирования меняет курсор на "указывающий"
 * при наведении на любую клетку доски.
 */
void ChessBoardWidget::mouseMoveEvent(QMouseEvent* event){
    if (m_editMode) {
        QPair<int, int> cell = getCellAtPosition(event->pos());
        if (cell.first >= 0) {
            setCursor(Qt::PointingHandCursor);  // Рука при наведении на клетку
        } else {
            setCursor(Qt::ArrowCursor);         // Обычная стрелка вне доски
        }
    }
    QWidget::mouseMoveEvent(event);
}

/**
 *  Определяет клетку по координатам мыши
 *  pos Позиция курсора в координатах виджета
 *  Результат: пара (столбец, строка) или (-1, -1) если вне доски
 */
QPair<int, int> ChessBoardWidget::getCellAtPosition(const QPoint& pos) const{
    int col = pos.x() / m_cellSize;
    int row = pos.y() / m_cellSize;

    if (col >= 0 && col < m_boardSize && row >= 0 && row < m_boardSize) {
        return qMakePair(col, row);
    }
    return qMakePair(-1, -1);
}

/**
 *  Показывает диалог редактирования клетки
 *  col Столбец клетки (0-индексация)
 *  row Строка клетки (0-индексация)
 *
 * Диалог позволяет изменить вес (1-100) и цвет (красный/зеленый/синий)
 * выбранной клетки в базе знаний.
 */
void ChessBoardWidget::showEditCellDialog(int col, int row){
    // Проверяем, что база знаний инициализирована
    if (!m_knowledgeBase) {
        QMessageBox::warning(this, "Ошибка", "База знаний не инициализирована");
        return;
    }

    // В базе знаний строки нумеруются с 1, поэтому преобразуем
    int actualRow = row + 1;
    int currentWeight = m_knowledgeBase->getPositionWeight(col, actualRow);
    PositionColor currentColor = m_knowledgeBase->getPositionColor(col, actualRow);

    // Создаем диалоговое окно
    QDialog dialog(this);
    dialog.setWindowTitle(QString("Редактирование клетки %1%2")
                              .arg(QChar('A' + col))
                              .arg(row + 1));
    dialog.setModal(true);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    // Информационная метка
    QLabel* infoLabel = new QLabel(QString("Клетка: %1%2")
                                       .arg(QChar('A' + col))
                                       .arg(row + 1), &dialog);
    layout->addWidget(infoLabel);

    // Поле для ввода веса
    QHBoxLayout* weightLayout = new QHBoxLayout();
    QLabel* weightLabel = new QLabel("Вес (1-100):", &dialog);
    QSpinBox* weightSpinBox = new QSpinBox(&dialog);
    weightSpinBox->setRange(1, 100);
    weightSpinBox->setValue(currentWeight);
    weightLayout->addWidget(weightLabel);
    weightLayout->addWidget(weightSpinBox);
    layout->addLayout(weightLayout);

    // Выбор цвета
    QHBoxLayout* colorLayout = new QHBoxLayout();
    QLabel* colorLabel = new QLabel("Цвет метки:", &dialog);
    QComboBox* colorCombo = new QComboBox(&dialog);
    colorCombo->addItem("Красный", QVariant::fromValue(PositionColor::Red));
    colorCombo->addItem("Зеленый", QVariant::fromValue(PositionColor::Green));
    colorCombo->addItem("Синий", QVariant::fromValue(PositionColor::Blue));

    // Устанавливаем текущий цвет
    int currentIndex = 0;
    switch (currentColor) {
    case PositionColor::Red: currentIndex = 0; break;
    case PositionColor::Green: currentIndex = 1; break;
    case PositionColor::Blue: currentIndex = 2; break;
    }
    colorCombo->setCurrentIndex(currentIndex);

    colorLayout->addWidget(colorLabel);
    colorLayout->addWidget(colorCombo);
    layout->addLayout(colorLayout);

    // Область предпросмотра цвета
    QLabel* previewLabel = new QLabel("Предпросмотр:", &dialog);
    QWidget* previewWidget = new QWidget(&dialog);
    previewWidget->setFixedSize(50, 50);

    // Лямбда-функция для обновления предпросмотра
    auto updatePreview = [previewWidget](PositionColor color) {
        QColor displayColor;
        switch (color) {
        case PositionColor::Red: displayColor = QColor(255, 100, 100); break;
        case PositionColor::Green: displayColor = QColor(100, 255, 100); break;
        case PositionColor::Blue: displayColor = QColor(100, 100, 255); break;
        default: displayColor = QColor(200, 200, 200); break;
        }
        previewWidget->setStyleSheet(QString("background-color: rgb(%1, %2, %3); border: 1px solid black;")
                                         .arg(displayColor.red())
                                         .arg(displayColor.green())
                                         .arg(displayColor.blue()));
    };

    updatePreview(currentColor);

    QHBoxLayout* previewLayout = new QHBoxLayout();
    previewLayout->addWidget(previewLabel);
    previewLayout->addWidget(previewWidget);
    previewLayout->addStretch();
    layout->addLayout(previewLayout);

    // Обновляем предпросмотр при изменении выбора цвета
    connect(colorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [updatePreview, colorCombo](int index) {
                Q_UNUSED(index)
                PositionColor color = colorCombo->currentData().value<PositionColor>();
                updatePreview(color);
            });

    // Кнопки OK/Отмена
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okButton = new QPushButton("Применить", &dialog);
    QPushButton* cancelButton = new QPushButton("Отмена", &dialog);
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);

    // Обработчик кнопки Применить
    connect(okButton, &QPushButton::clicked, [&]() {
        int newWeight = weightSpinBox->value();
        PositionColor newColor = colorCombo->currentData().value<PositionColor>();

        // Сохраняем изменения в базу знаний
        m_knowledgeBase->setPositionWeight(col, actualRow, newWeight);
        m_knowledgeBase->setPositionColor(col, actualRow, newColor);

        // Сигнализируем об изменениях
        emit cellWeightChanged(col, actualRow, newWeight);
        emit cellColorChanged(col, actualRow, newColor);

        dialog.accept();
        update();  // Перерисовываем доску
    });

    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
}

// ============================================================================
// Отрисовка виджета
// ============================================================================

/**
 *  Обработчик события перерисовки
 *
 * Выполняет полную перерисовку доски в следующем порядке:
 * 1. Шахматная доска
 * 2. Наложение данных из базы знаний (веса и цвета)
 * 3. Ферзи
 */
void ChessBoardWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    drawBoard(painter);

    if (m_knowledgeBase) {
        drawKnowledgeOverlay(painter);
    }

    drawQueens(painter);
    drawPriorityIndicators(painter);  // Это правильно, один вызов
}

/**
 *  Обработчик изменения размера окна
 *
 * Пересчитывает размер клетки и вызывает перерисовку.
 */
void ChessBoardWidget::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event)
    int widgetSize = qMin(width(), height());
    if (widgetSize > 0 && m_boardSize > 0) {
        m_cellSize = widgetSize / m_boardSize;
    }
    updatePrioritySpinBoxes();  // Добавьте эту строку
    update();
}

/**
 *  Рисует шахматную доску
 *  painter Объект QPainter для рисования
 *
 * Клетки чередуются: светлые (240,217,181) и темные (181,136,99).
 */
void ChessBoardWidget::drawBoard(QPainter& painter){
    for (int i = 0; i < m_boardSize; ++i) {
        for (int j = 0; j < m_boardSize; ++j) {
            QRect rect = getCellRect(i, j);
            // Чередуем цвета клеток
            if ((i + j) % 2 == 0)
                painter.fillRect(rect, QColor(240, 217, 181));  // Светлая клетка
            else
                painter.fillRect(rect, QColor(181, 136, 99));   // Темная клетка
            // Рисуем границу клетки
            painter.setPen(QPen(Qt::black, 1));
            painter.drawRect(rect);
        }
    }
}

/**
 *  Рисует наложение данных из базы знаний
 *  painter Объект QPainter для рисования
 *
 * Для каждой клетки:
 * - Заливает полупрозрачным цветом в зависимости от цвета в БЗ
 * - Отображает вес клетки в верхней половине клетки
 */
void ChessBoardWidget::drawKnowledgeOverlay(QPainter& painter){
    if (!m_knowledgeBase) return;

    painter.setPen(Qt::black);

    for (int col = 0; col < m_boardSize; ++col) {
        for (int row = 0; row < m_boardSize; ++row) {
            QRect rect = getCellRect(col, row);
            int actualRow = row + 1;  // В БЗ строки с 1

            // Получаем данные из БЗ
            int weight = m_knowledgeBase->getPositionWeight(col, actualRow);
            PositionColor color = m_knowledgeBase->getPositionColor(col, actualRow);

            // Рисуем полупрозрачный цветовой фон
            QColor overlayColor = m_knowledgeBase->getColorValue(color);
            overlayColor.setAlpha(80);  // Прозрачность 80/255
            painter.fillRect(rect, overlayColor);

            // Рисуем вес в верхней половине клетки
            painter.setPen(Qt::black);
            QString weightText = QString::number(weight);

            QFont weightFont = painter.font();
            weightFont.setPointSize(qMax(10, m_cellSize / 6));  // Динамический размер шрифта
            weightFont.setBold(true);
            painter.setFont(weightFont);

            // Вес располагается в верхней половине клетки
            QRect weightRect = rect;
            weightRect.setHeight(rect.height() / 2);
            painter.drawText(weightRect, Qt::AlignCenter, weightText);

            // Для цвета используем меньший шрифт
            QFont colorFont = painter.font();
            colorFont.setPointSize(qMax(6, m_cellSize / 12));
            colorFont.setBold(false);
            painter.setFont(colorFont);
        }
    }
}

/**
 *  Рисует всех ферзей на доске
 *  painter Объект QPainter для рисования
 *
 * Проходит по всем столбцам и, если в столбце есть ферзь,
 * вызывает метод отрисовки одного ферзя.
 */
void ChessBoardWidget::drawQueens(QPainter& painter)
{
    // Сначала рисуем обычных ферзей из m_queenPositions
    for (int col = 0; col < m_boardSize; ++col) {
        if (col < static_cast<int>(m_queenPositions.size()) && m_queenPositions[col] >= 0) {
            int row = m_queenPositions[col];
            if (row < m_boardSize) {
                drawQueenAt(painter, col, row);
            }
        }
    }

    // Затем проверяем фиксированные позиции, которые могли быть не в m_queenPositions
    if (m_priorityManager) {
        for (int col = 0; col < m_boardSize; ++col) {
            if (m_priorityManager->isFixed(col)) {
                auto fixedRow = m_priorityManager->getFixedPosition(col);
                if (fixedRow.has_value() && fixedRow.value() >= 0 && fixedRow.value() < m_boardSize) {
                    // Проверяем, не нарисован ли уже ферзь в этой позиции
                    bool alreadyDrawn = false;
                    if (col < static_cast<int>(m_queenPositions.size()) &&
                        m_queenPositions[col] == fixedRow.value()) {
                        alreadyDrawn = true;
                    }

                    // Если еще не нарисован - рисуем
                    if (!alreadyDrawn) {
                        drawQueenAt(painter, col, fixedRow.value());
                    }
                }
            }
        }
    }
}

/**
 *  Возвращает прямоугольник клетки в координатах виджета
 *  col Столбец (0-индексация)
 *  row Строка (0-индексация)
 */
QRect ChessBoardWidget::getCellRect(int col, int row) const{
    int x = col * m_cellSize;
    int y = row * m_cellSize;
    return QRect(x, y, m_cellSize, m_cellSize);
}

/**
 * Рисует одного ферзя в указанной клетке
 * painter Объект QPainter для рисования
 * col Столбец ферзя
 * row Строка ферзя
 *
 * Ферзь отображается как черный круг, занимающий 30% размера клетки.
 */
void ChessBoardWidget::drawQueenAt(QPainter& painter, int col, int row)
{
    QRect rect = getCellRect(col, row);
    int centerX = rect.center().x();
    int centerY = rect.center().y();
    int size = m_cellSize * 0.3;  // Размер ферзя относительно клетки

    painter.save();  // Сохраняем состояние painter'а

    // Проверяем, зафиксирован ли ферзь
    bool isFixed = false;
    int priority = 0;

    if (m_priorityManager) {
        isFixed = m_priorityManager->isFixed(col);
        priority = m_priorityManager->getPriority(col);
    }

    if (isFixed) {
        // Зафиксированный ферзь - белый
        painter.setBrush(QBrush(Qt::white));
    } else {
        // Обычный ферзь - черный
        painter.setBrush(QBrush(Qt::black));
    }
    painter.setPen(QPen(Qt::black, 1));

    painter.drawEllipse(centerX - size/2, centerY - size/2, size, size);

    // Добавляем номер приоритета в центре ферзя
    if (priority > 0) {
        painter.setPen(QPen(isFixed ? Qt::black : Qt::white, 1));

        // Размер шрифта зависит от размера ферзя и длины числа
        int fontSize = size * 0.6;
        if (priority >= 10) {
            fontSize = size * 0.45;  // Двузначные числа чуть меньше
        }

        QFont font = painter.font();
        font.setPointSize(fontSize);
        font.setBold(true);
        painter.setFont(font);

        QString priorityText = QString::number(priority);
        painter.drawText(QRect(centerX - size/2, centerY - size/2, size, size),
                         Qt::AlignCenter, priorityText);
    }

    painter.restore();  // Восстанавливаем состояние
}

void ChessBoardWidget::setPriorityManager(PriorityManager* manager)
{
    m_priorityManager = manager;
    updatePrioritySpinBoxes();
    update();
}

void ChessBoardWidget::setPriorityMode(bool enabled)
{
    m_priorityMode = enabled;
    updatePrioritySpinBoxes();
    update();
}

void ChessBoardWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_priorityMode) {
        QPair<int, int> cell = getCellAtPosition(event->pos());
        if (cell.first >= 0 && cell.second >= 0) {
            showPriorityEditDialog(cell.first, cell.second);
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

/*
void ChessBoardWidget::drawPriorities(QPainter& painter)
{
    if (!m_priorityManager || !m_priorityMode) return;

    for (int col = 0; col < m_boardSize; ++col) {
        QRect rect = getCellRect(col, 0); // Верхняя клетка столбца
        int priority = m_priorityManager->getPriority(col);
        bool isFixed = m_priorityManager->isFixed(col);

        if (priority > 0 || isFixed) {
            // Рисуем индикатор над клеткой
            QRect indicatorRect(rect.x(), rect.y() - 20, rect.width(), 20);
            painter.fillRect(indicatorRect, QColor(50, 50, 50, 200));

            QString text;
            if (isFixed) {
                text = QString("🔒 P%1").arg(priority);
            } else if (priority > 0) {
                text = QString("P%1").arg(priority);
            }

            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 10, QFont::Bold));
            painter.drawText(indicatorRect, Qt::AlignCenter, text);
        }
    }
}
*/

void ChessBoardWidget::showPriorityEditDialog(int col, int row)
{
    if (!m_priorityManager) return;

    QDialog dialog(this);
    dialog.setWindowTitle(QString("Редактирование приоритета - Столбец %1")
                              .arg(QChar('A' + col)));
    dialog.setModal(true);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    // Информация о столбце
    QLabel* infoLabel = new QLabel(QString("Столбец: %1").arg(QChar('A' + col)), &dialog);
    infoLabel->setFont(QFont("Arial", 12, QFont::Bold));
    layout->addWidget(infoLabel);

    // Текущий приоритет
    int currentPriority = m_priorityManager->getPriority(col);
    QLabel* currentPrioLabel = new QLabel(QString("Текущий приоритет: %1")
                                              .arg(currentPriority == 0 ? "не задан" : QString::number(currentPriority)),
                                          &dialog);
    layout->addWidget(currentPrioLabel);

    // Разделительная линия
    QFrame* line = new QFrame(&dialog);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);

    // Спинбокс для приоритета
    QHBoxLayout* priorityLayout = new QHBoxLayout();
    QLabel* priorityLabel = new QLabel("Приоритет:", &dialog);
    QSpinBox* prioritySpin = new QSpinBox(&dialog);
    prioritySpin->setRange(0, 10);
    prioritySpin->setValue(currentPriority);
    prioritySpin->setToolTip("0 - нет приоритета\n1 - наивысший приоритет");
    priorityLayout->addWidget(priorityLabel);
    priorityLayout->addWidget(prioritySpin);
    priorityLayout->addStretch();
    layout->addLayout(priorityLayout);

    // Выбор строки для фиксации
    QHBoxLayout* rowLayout = new QHBoxLayout();
    QLabel* rowLabel = new QLabel("Строка для фиксации:", &dialog);
    QSpinBox* rowSpin = new QSpinBox(&dialog);
    rowSpin->setRange(0, m_boardSize - 1);
    rowSpin->setValue(row);
    rowSpin->setPrefix("Строка ");
    rowSpin->setToolTip("0-" + QString::number(m_boardSize - 1) + " (0-индексация)");
    rowLayout->addWidget(rowLabel);
    rowLayout->addWidget(rowSpin);
    rowLayout->addStretch();
    layout->addLayout(rowLayout);

    // Чекбокс для фиксации позиции
    bool isFixed = m_priorityManager->isFixed(col);
    QCheckBox* fixCheckbox = new QCheckBox(QString("Фиксировать ферзя в клетке %1%2")
                                               .arg(QChar('A' + col))
                                               .arg(row + 1),
                                           &dialog);
    fixCheckbox->setChecked(isFixed);
    layout->addWidget(fixCheckbox);

    // Обновление текста чекбокса при изменении строки
    connect(rowSpin, QOverload<int>::of(&QSpinBox::valueChanged), [fixCheckbox, col](int newRow) {
        fixCheckbox->setText(QString("Фиксировать ферзя в клетке %1%2")
                                 .arg(QChar('A' + col))
                                 .arg(newRow + 1));
    });

    // Кнопки
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okButton = new QPushButton("Применить", &dialog);
    QPushButton* cancelButton = new QPushButton("Отмена", &dialog);
    okButton->setDefault(true);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, [&]() {
        int newPriority = prioritySpin->value();
        int newRow = rowSpin->value();
        bool newFixed = fixCheckbox->isChecked();

        // Применяем приоритет
        if (newPriority == 0) {
            m_priorityManager->clearPriority(col);
        } else {
            m_priorityManager->setPriority(col, newPriority);
        }

        // Применяем фиксацию
        if (newFixed) {
            m_priorityManager->setFixedPosition(col, newRow);
            // Обновляем позицию ферзя в m_queenPositions
            if (col < static_cast<int>(m_queenPositions.size())) {
                m_queenPositions[col] = newRow;
            }
        } else {
            m_priorityManager->clearFixedPosition(col);
        }

        // Обновляем отображение спинбокса
        if (m_prioritySpinBoxes.contains(col)) {
            PrioritySpinBox* spinBox = m_prioritySpinBoxes[col];
            spinBox->setPriorityValue(newPriority);
            spinBox->setFixed(newFixed);
            spinBox->setFixedRow(newRow);
        }

        dialog.accept();

        // Отправляем сигнал о изменении приоритетов
        emit prioritiesChanged();

        // Принудительно перерисовываем доску
        update();
        repaint();
    });

    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
}

void ChessBoardWidget::drawPriorityIndicators(QPainter& painter)
{
    if (!m_priorityManager || !m_priorityMode) return;

    for (int col = 0; col < m_boardSize; ++col) {
        int priority = m_priorityManager->getPriority(col);
        bool isFixed = m_priorityManager->isFixed(col);
        auto fixedPos = m_priorityManager->getFixedPosition(col);

        if (priority > 0 || isFixed) {
            QRect rect = getCellRect(col, 0);
            // Индикатор над верхней клеткой столбца
            QRect indicatorRect(rect.x(), rect.y() - 22, rect.width(), 20);

            // Фон индикатора
            QColor bgColor = isFixed ? QColor(255, 80, 0, 220) : QColor(0, 0, 0, 200);
            painter.fillRect(indicatorRect, bgColor);

            // Граница
            painter.setPen(QPen(isFixed ? QColor(255, 100, 100) : QColor(150, 150, 150), 1));
            painter.drawRect(indicatorRect);

            // Текст
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 9, QFont::Bold));

            QString text;
            if (isFixed) {
                text = QString("🔒 F%1").arg(fixedPos.value() + 1);
            } else if (priority > 0) {
                text = QString("P%1").arg(priority);
                if (priority <= 3) text += " ★";
            }

            painter.drawText(indicatorRect, Qt::AlignCenter, text);
        }
    }
}

void ChessBoardWidget::updatePrioritySpinBoxes()
{
    // Очищаем старые спинбоксы
    for (auto* spinBox : m_prioritySpinBoxes) {
        spinBox->deleteLater();
    }
    m_prioritySpinBoxes.clear();

    if (!m_priorityManager || !m_priorityMode) return;

    for (int col = 0; col < m_boardSize; ++col) {
        PrioritySpinBox* spinBox = new PrioritySpinBox(this);
        QRect rect = getCellRect(col, 0);
        int x = rect.x() + (rect.width() - spinBox->width()) / 2;
        int y = rect.y() - 25;
        spinBox->setGeometry(x, y, spinBox->width(), spinBox->height());
        spinBox->setPosition(col, 0);
        spinBox->setPriorityValue(m_priorityManager->getPriority(col));
        spinBox->setFixed(m_priorityManager->isFixed(col));

        // Получаем фиксированную строку для отображения
        auto fixedPos = m_priorityManager->getFixedPosition(col);
        if (fixedPos.has_value()) {
            spinBox->setFixedRow(fixedPos.value());
        }

        // Подключаем сигнал запроса редактирования
        connect(spinBox, &PrioritySpinBox::editRequested, this, &ChessBoardWidget::onEditRequested);

        m_prioritySpinBoxes[col] = spinBox;
        spinBox->show();
    }

    update();
}

bool ChessBoardWidget::isQueenFixed(int col) const
{
    if (!m_priorityManager) return false;
    return m_priorityManager->isFixed(col);
}

void ChessBoardWidget::onEditRequested(int column, int row)
{
    // Определяем текущую строку для фиксации
    int currentRow = row;

    // Если строка не задана (row == 0), используем текущую позицию ферзя в этом столбце
    if (currentRow == 0 && column < static_cast<int>(m_queenPositions.size())) {
        currentRow = m_queenPositions[column];
    }

    // Если все еще нет строки, используем центр доски
    if (currentRow < 0) {
        currentRow = m_boardSize / 2;
    }

    // Открываем диалог редактирования
    showPriorityEditDialog(column, currentRow);
}