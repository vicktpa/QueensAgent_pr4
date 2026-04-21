#ifndef CHESSBOARDWIDGET_H
#define CHESSBOARDWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QMap>
#include <vector>
#include <optional>

#include "KnowledgeBase.h"
#include "prioritymanager.h"
#include "priorityspinbox.h"

/**
 * Виджет для отображения шахматной доски и ферзей
 *
 * Отвечает за визуализацию шахматной доски, отображение ферзей,
 * наложение визуализации базы знаний (веса и цвета клеток),
 * а также обработку событий мыши для редактирования базы знаний
 * и управления приоритетами агентов.
 *
 * Особенности отображения:
 * - Обычные ферзи: черные круги
 * - Зафиксированные ферзи: белые круги с черной обводкой
 * - Приоритет: отображается числом в центре ферзя
 * - Индикаторы над столбцами: показывают приоритет и статус фиксации
 */
class ChessBoardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChessBoardWidget(QWidget* parent = nullptr);

    // Управление размером доски
    void setBoardSize(int size);
    int getBoardSize() const { return m_boardSize; }

    // Управление позициями ферзей
    void setQueenPositions(const std::vector<int>& positions);
    void clearQueens();

    /**
     *  Возвращает текущие позиции ферзей
     *  Индекс - столбец, значение - строка (-1 если нет ферзя)
     */
    const std::vector<int>& getQueenPositions() const { return m_queenPositions; }

    // Управление решениями
    const std::vector<std::vector<int>>& getAllSolutions() const { return m_allSolutions; }
    int getCurrentSolutionIndex() const { return m_currentSolutionIndex; }
    void setCurrentSolutionIndex(int index) { m_currentSolutionIndex = index; }

    // Управление базой знаний
    void setKnowledgeBase(KnowledgeBase* kb) { m_knowledgeBase = kb; }
    void setEditMode(bool enabled) { m_editMode = enabled; }
    bool isEditMode() const { return m_editMode; }

    // Обновление отображения
    void refreshDisplay();

    // Управление приоритетами
    void setPriorityManager(PriorityManager* manager);
    void setPriorityMode(bool enabled);
    bool isPriorityMode() const { return m_priorityMode; }

    /**
     *  Получить текущие фиксированные позиции из приоритет-менеджера
     *  Возвращает вектор optional<int> где значение - строка (0-индексация)
     */
    std::vector<std::optional<int>> getFixedPositions() const;

public slots:
    // Навигация по решениям
    void nextSolution();
    void previousSolution();
    void setSolutions(const std::vector<std::vector<int>>& solutions);

protected:
    // Переопределенные методы Qt
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

signals:
    // Сигналы для взаимодействия с базой знаний
    void cellClicked(int col, int row);
    void cellWeightChanged(int col, int row, int newWeight);
    void cellColorChanged(int col, int row, PositionColor newColor);

    /**
     *  Сигнал об изменении приоритетов или фиксаций
     *  Отправляется после применения изменений в диалоге редактирования
     */
    void prioritiesChanged();

private slots:
    /**
     *  Обработчик запроса на редактирование приоритета
     *  Вызывается при двойном клике по PrioritySpinBox
     *  column Столбец для редактирования
     *  row Текущая строка (может быть 0)
     */
    void onEditRequested(int column, int row);

private:
    // Методы отрисовки
    void drawBoard(QPainter& painter);           ///< Отрисовка шахматной доски
    void drawQueens(QPainter& painter);          ///< Отрисовка ферзей
    void drawKnowledgeOverlay(QPainter& painter); ///< Отрисовка данных из БЗ (веса/цвета)
    void drawPriorityIndicators(QPainter& painter); ///< Отрисовка индикаторов приоритетов
    QRect getCellRect(int col, int row) const;    ///< Получение прямоугольника клетки
    void drawQueenAt(QPainter& painter, int col, int row); ///< Отрисовка одного ферзя

    // Вспомогательные методы
    QPair<int, int> getCellAtPosition(const QPoint& pos) const; ///< Определение клетки по координатам
    void showEditCellDialog(int col, int row);    ///< Диалог редактирования клетки БЗ
    void showPriorityEditDialog(int col, int row); ///< Диалог редактирования приоритета
    void updatePrioritySpinBoxes();               ///< Обновление спинбоксов приоритетов

    /**
     *  Применяет изменения приоритетов и отправляет сигнал prioritiesChanged
     *  Вызывается после закрытия диалога редактирования приоритета
     */
    void applyPriorityChanges();

    /**
     *  Проверяет, зафиксирован ли ферзь в указанном столбце
     *  col Столбец для проверки
     *  Возвращает true если позиция зафиксирована
     */
    bool isQueenFixed(int col) const;

private:
    // Данные о доске и решениях
    int m_boardSize;                    ///< Размер доски (N x N)
    int m_cellSize;                     ///< Размер одной клетки в пикселях
    std::vector<int> m_queenPositions;  ///< Позиции ферзей (индекс - столбец, значение - строка)
    std::vector<std::vector<int>> m_allSolutions; ///< Все найденные решения
    int m_currentSolutionIndex;         ///< Индекс текущего отображаемого решения

    // База знаний и режимы
    KnowledgeBase* m_knowledgeBase;     ///< Указатель на базу знаний
    bool m_editMode;                    ///< Режим редактирования базы знаний

    // Управление приоритетами
    PriorityManager* m_priorityManager; ///< Менеджер приоритетов и фиксаций
    bool m_priorityMode;                ///< Режим отображения приоритетов
    QMap<int, PrioritySpinBox*> m_prioritySpinBoxes; ///< Спинбоксы приоритетов для каждого столбца
};

#endif // CHESSBOARDWIDGET_H