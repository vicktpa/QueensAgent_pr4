#include "prioritymanager.h"
#include <algorithm>
#include <QDebug>

// ============================================================================
// Конструктор
// ============================================================================

/**
 *  Конструктор PriorityManager
 *  parent Родительский объект Qt
 *
 *  Создает пустой менеджер без приоритетов и фиксаций
 */
PriorityManager::PriorityManager(QObject* parent)
    : QObject(parent)
{
    // m_priorities и m_fixedPositions изначально пусты
}

// ============================================================================
// Управление приоритетами
// ============================================================================

/**
 *  Устанавливает приоритет для указанного столбца
 *  column Номер столбца (0-индексация)
 *  priority Значение приоритета (1-10)
 *
 *  Если priority <= 0, приоритет очищается
 *  Меньшее значение = более высокий приоритет
 */
void PriorityManager::setPriority(int column, int priority)
{
    if (priority <= 0) {
        // Если приоритет 0 или отрицательный - очищаем
        clearPriority(column);
    } else {
        // Сохраняем приоритет в QMap
        m_priorities[column] = priority;
        // Оповещаем об изменении
        emit priorityChanged(column, priority);

        qDebug() << "PriorityManager: set priority" << priority
                 << "for column" << char('A' + column);
    }
}

/**
 *  Возвращает текущий приоритет столбца
 *  column Номер столбца (0-индексация)
 *  Возвращает Приоритет (1-10) или 0, если приоритет не задан
 */
int PriorityManager::getPriority(int column) const
{
    // value() возвращает значение по ключу, или defaultValue если ключ не найден
    return m_priorities.value(column, 0);
}

/**
 *  Очищает приоритет для указанного столбца
 *  column Номер столбца (0-индексация)
 */
void PriorityManager::clearPriority(int column)
{
    if (m_priorities.contains(column)) {
        m_priorities.remove(column);
        // Оповещаем, что приоритет сброшен (priority = 0)
        emit priorityChanged(column, 0);

        qDebug() << "PriorityManager: cleared priority for column"
                 << char('A' + column);
    }
}

// ============================================================================
// Управление фиксированными позициями
// ============================================================================

/**
 *  Устанавливает фиксированную позицию для столбца
 *  column Номер столбца (0-индексация)
 *  row Строка (0-индексация) или std::nullopt для снятия фиксации
 *
 *  Фиксированная позиция означает, что ферзь в этом столбце
 *  не будет двигаться при поиске решений
 */
void PriorityManager::setFixedPosition(int column, std::optional<int> row)
{
    m_fixedPositions[column] = row;
    emit fixedPositionChanged(column, row);

    if (row.has_value()) {
        qDebug() << "PriorityManager: fixed column" << char('A' + column)
        << "at row" << row.value();
    } else {
        qDebug() << "PriorityManager: unfixed column" << char('A' + column);
    }
}

/**
 *  Возвращает фиксированную позицию для столбца
 *  column Номер столбца (0-индексация)
 *  Возвращает std::optional<int> - строка или std::nullopt
 */
std::optional<int> PriorityManager::getFixedPosition(int column) const
{
    // value() для optional возвращает std::nullopt если ключ не найден
    return m_fixedPositions.value(column, std::nullopt);
}

/**
 *  Проверяет, зафиксирована ли позиция в указанном столбце
 *  column Номер столбца (0-индексация)
 *  Возвращает true если позиция зафиксирована
 */
bool PriorityManager::isFixed(int column) const
{
    // Позиция фиксирована, если:
    // 1. Ключ существует в QMap
    // 2. Значение содержит строку (не std::nullopt)
    return m_fixedPositions.contains(column) &&
           m_fixedPositions[column].has_value();
}

/**
 *  Снимает фиксацию с указанного столбца
 *  column Номер столбца (0-индексация)
 */
void PriorityManager::clearFixedPosition(int column)
{
    if (m_fixedPositions.contains(column)) {
        m_fixedPositions.remove(column);
        emit fixedPositionChanged(column, std::nullopt);

        qDebug() << "PriorityManager: cleared fixed position for column"
                 << char('A' + column);
    }
}

// ============================================================================
// Получение упорядоченных столбцов
// ============================================================================

/**
 *  Возвращает столбцы в порядке, определяемом приоритетами
 *  Возвращает Вектор номеров столбцов, отсортированных по приоритету
 *
 *  Правила сортировки:
 *  1. Фиксированные позиции ВСЕГДА первые
 *  2. Затем столбцы с приоритетом (1 - наивысший, 10 - низший)
 *  3. Затем столбцы без приоритета (в обычном порядке)
 *
 *  Используется в QueensSolver для определения порядка перебора
 */
std::vector<int> PriorityManager::getOrderedColumns() const
{
    // Создаем вектор со всеми столбцами (0..9 для максимального размера 10)
    std::vector<int> columns;
    for (int i = 0; i < 10; ++i) {
        columns.push_back(i);
    }

    // Сортируем столбцы согласно правилам приоритетов
    std::sort(columns.begin(), columns.end(), [this](int a, int b) {
        bool aFixed = isFixed(a);
        bool bFixed = isFixed(b);

        // Правило 1: Фиксированные позиции имеют наивысший приоритет
        if (aFixed != bFixed) {
            return aFixed > bFixed;  // true > false, фиксированные идут первыми
        }

        // Оба либо фиксированы, либо нет - сравниваем приоритеты
        int aPrio = getPriority(a);
        int bPrio = getPriority(b);

        // Правило 2: Сравнение заданных приоритетов
        if (aPrio > 0 && bPrio > 0) {
            return aPrio < bPrio;  // Меньшее число = выше приоритет
        }

        // Правило 3: Приоритетные идут перед неприоритетными
        if (aPrio > 0 && bPrio == 0) return true;   // a имеет приоритет, b нет -> a первый
        if (aPrio == 0 && bPrio > 0) return false;  // b имеет приоритет, a нет -> b первый

        // Правило 4: Оба без приоритета - сохраняем исходный порядок
        return a < b;
    });

    return columns;
}

// ============================================================================
// Сброс состояния
// ============================================================================

/**
 *  Сбрасывает все приоритеты и фиксации
 *
 *  Очищает оба QMap'а, возвращая менеджер в начальное состояние
 *  Используется при полном сбросе состояния доски
 */
void PriorityManager::reset()
{
    m_priorities.clear();
    m_fixedPositions.clear();

    qDebug() << "PriorityManager: reset - all priorities and fixed positions cleared";

    // Примечание: сигналы не отправляются, так как это массовый сброс
    // При необходимости можно добавить сигнал reset()
}


/**
     *  Возвращает все фиксированные позиции
     */
std::vector<std::optional<int>> PriorityManager::getFixedPositions() const {
    std::vector<std::optional<int>> positions;
    for (int i = 0; i < 10; ++i) {
        positions.push_back(getFixedPosition(i));
    }
    return positions;
}