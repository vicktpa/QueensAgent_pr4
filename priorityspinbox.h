#ifndef PRIORITYSPINBOX_H
#define PRIORITYSPINBOX_H

#include <QSpinBox>
#include <QPainter>

/**
 *  Специализированный спинбокс для отображения и редактирования приоритетов
 *
 * Особенности:
 * - Только для чтения (нельзя менять стрелками)
 * - Двойной клик открывает диалог редактирования
 * - Визуально отображает приоритет и статус фиксации
 * - Для высокого приоритета (1-3) показывает звездочку
 * - Для фиксированных позиций показывает замок и оранжевый фон
 */
class PrioritySpinBox : public QSpinBox
{
    Q_OBJECT

public:
    explicit PrioritySpinBox(QWidget* parent = nullptr);

    /**
     *  Устанавливает статус фиксации
     *  Меняет внешний вид спинбокса (оранжевый фон, замок)
     */
    void setFixed(bool fixed);

    bool isFixed() const { return m_isFixed; }

    /**
     *  Устанавливает позицию (столбец и строку) для этого спинбокса
     */
    void setPosition(int col, int row);

    /**
     *  Устанавливает строку для фиксации
     */
    void setFixedRow(int row);

    /**
     *  Программно устанавливает значение приоритета
     *  (без эмита сигналов изменения)
     */
    void setPriorityValue(int priority);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

signals:
    /**
     *  Сигнал запроса на редактирование приоритета
     *  Отправляется при двойном клике
     *  column Столбец
     *  row Текущая строка
     */
    void editRequested(int column, int row);

private:
    bool m_isFixed;     ///< Зафиксирована ли позиция
    int m_column;       ///< Номер столбца (0-индексация)
    int m_row;          ///< Номер строки (0-индексация)
};

#endif // PRIORITYSPINBOX_H