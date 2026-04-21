#ifndef PRIORITYMANAGER_H
#define PRIORITYMANAGER_H

#include <QObject>
#include <QMap>
#include <vector>
#include <optional>

/**
 *  Класс для управления приоритетами столбцов и фиксированными позициями
 *
 * Хранит для каждого столбца:
 * - Приоритет (0-10, где 0 - нет приоритета, 1 - наивысший)
 * - Фиксированную позицию (опционально, строка 0-индексация)
 *
 * Используется для определения порядка перебора столбцов при поиске решений
 * и для фиксации определенных ферзей на заданных позициях.
 */
class PriorityManager : public QObject
{
    Q_OBJECT

public:
    explicit PriorityManager(QObject* parent = nullptr);

    /**
     *  Устанавливает приоритет для столбца
     *  column Столбец (0-индексация)
     *  priority Приоритет (0-10, 0 - сброс приоритета)
     */
    void setPriority(int column, int priority);

    /**
     *  Возвращает приоритет столбца
     *  column Столбец
     *  Возвращает 0 если приоритет не задан
     */
    int getPriority(int column) const;

    /**
     *  Очищает приоритет для столбца
     */
    void clearPriority(int column);

    /**
     *  Устанавливает фиксированную позицию для столбца
     *  column Столбец
     *  row Строка (0-индексация) или nullopt для снятия фиксации
     */
    void setFixedPosition(int column, std::optional<int> row);

    /**
     *  Возвращает фиксированную позицию столбца
     *  Возвращает nullopt если позиция не зафиксирована
     */
    std::optional<int> getFixedPosition(int column) const;

    /**
     *  Проверяет, зафиксирована ли позиция в столбце
     */
    bool isFixed(int column) const;

    /**
     *  Снимает фиксацию со столбца
     */
    void clearFixedPosition(int column);

    /**
     *  Возвращает столбцы в порядке убывания приоритета
     *  Сначала идут фиксированные, затем по приоритету (1-10), затем без приоритета
     */
    std::vector<int> getOrderedColumns() const;

    /**
     *  Сбрасывает все приоритеты и фиксации
     */
    void reset();

    /**
     *  Возвращает все фиксированные позиции
     */
    std::vector<std::optional<int>> getFixedPositions() const;

signals:
    /**
     *  Сигнал об изменении приоритета столбца
     *  column Столбец
     *  newPriority Новый приоритет (0 если сброшен)
     */
    void priorityChanged(int column, int newPriority);

    /**
     *  Сигнал об изменении фиксированной позиции
     *  column Столбец
     *  row Строка или nullopt если фиксация снята
     */
    void fixedPositionChanged(int column, std::optional<int> row);

private:
    QMap<int, int> m_priorities;                    ///< Приоритеты столбцов (ключ - столбец, значение - приоритет)
    QMap<int, std::optional<int>> m_fixedPositions; ///< Фиксированные позиции (ключ - столбец)
};

#endif // PRIORITYMANAGER_H