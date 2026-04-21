#include "QueenAgent.h"
#include "KnowledgeBase.h"
#include <cmath>
#include <QDebug>
#include <algorithm>

// Определяем qBound если его нет (для совместимости)
#ifndef qBound
template <typename T>
inline T qBound(const T &min, const T &value, const T &max) {
    return value < min ? min : (value > max ? max : value);
}
#endif

// ============================================================================
// Конструкторы
// ============================================================================

QueenAgent::QueenAgent(int column, KnowledgeBase* knowledgeBase,
                       QueenAgent* neighbor, QObject* parent)
    : QObject(parent)
    , m_column(column)
    , m_row(1)
    , m_neighbor(neighbor)
    , m_maxRows(8)
    , m_terminal(false)
    , m_knowledgeBase(knowledgeBase)
    , m_aggressiveness(0.5)
    , m_conflictsEncountered(0)
    , m_fixedRow(std::nullopt)  // <-- Добавьте эту строку, если её нет
{
}


QueenAgent::~QueenAgent()
{
    // Деструктор - ничего особенного не требуется
    // Соседние ферзи не удаляем (они управляются извне)
}

// ============================================================================
// Проверка атак
// ============================================================================

/**
 *  Проверяет, атакует ли текущий ферзь указанную позицию
 *  testRow Строка для проверки (1-индексация)
 *  testColumn Столбец для проверки (0-индексация)
 * Возвращает true, если атакует
 *
 * Алгоритм проверки:
 * 1. Проверяем горизонталь (одинаковые строки)
 * 2. Проверяем диагонали (|row_diff| == |col_diff|)
 * 3. Если есть сосед - рекурсивно проверяем через него
 *
 * Важно: ферзи атакуют только тех, кто находится справа.
 * Проверка идет слева направо через цепочку соседей.
 */
bool QueenAgent::canAttack(int testRow, int testColumn) const
{
    // Проверка горизонтали
    if (m_row == testRow)
        return true;

    // Проверка диагоналей
    int columnDifference = testColumn - m_column;
    if ((m_row + columnDifference == testRow) ||
        (m_row - columnDifference == testRow))
        return true;

    // Рекурсивно проверяем соседа
    if (m_neighbor != nullptr)
        return m_neighbor->canAttack(testRow, testColumn);

    return false;
}

// ============================================================================
// Базовые методы поиска (без базы знаний)
// ============================================================================

/**
 *  Проверяет, является ли текущая позиция решением
 * Возвращает true, если нет конфликтов
 *
 * Проходит по цепочке соседей и проверяет, не атакует ли кто-то текущего ферзя.
 * Так как ферзи атакуют только тех, кто справа, достаточно проверить
 * всех левых соседей на атаку текущей позиции.
 */
bool QueenAgent::findSolution()
{
    if (m_terminal) return false;

    // Проверяем конфликты со всеми левыми соседями
    QueenAgent* neighbor = m_neighbor;
    while (neighbor != nullptr) {
        if (neighbor->canAttack(m_row, m_column)) {
            return false; // Обнаружен конфликт - позиция не подходит
        }
        neighbor = neighbor->m_neighbor;
    }

    return true; // Нет конфликтов - позиция валидна
}

/**
 *  Продвигает ферзя к следующей позиции
 * Возвращает true, если удалось продвинуться
 *
 * Алгоритм перебора (аналогичен сложению в столбик):
 * 1. Пытаемся увеличить текущую строку
 * 2. Если дошли до конца (row == maxRows), то:
 *    a) Пытаемся подвинуть соседа
 *    b) Если сосед подвинулся - сбрасываемся на 1
 *    c) Если сосед не может двигаться - помечаем себя терминальным
 *
 * Это обеспечивает полный перебор всех комбинаций.
 */
bool QueenAgent::advance()
{
    if (m_terminal) return false;

    // Пробуем следующую строку
    if (m_row < m_maxRows) {
        m_row++;
        emit positionChanged(m_column, m_row);
        return true;
    }

    // Достигли дна - двигаем соседа
    if (m_neighbor != nullptr) {
        if (!m_neighbor->advance()) {
            // Сосед тоже достиг конца
            m_terminal = true;
            return false;
        }
        // Сбрасываем текущую позицию на 1
        m_row = 1;
        emit positionChanged(m_column, m_row);
        return true;
    }

    // Самый левый ферзь достиг конца - поиск завершен
    m_terminal = true;
    return false;
}

/**
 *  Сбрасывает ферзя в начальное состояние
 *
 * Используется при перезапуске поиска или после завершения.
 * Рекурсивно сбрасывает всех соседей.
 */
void QueenAgent::reset()
{
    m_row = 1;
    m_terminal = false;
    m_badRows.clear();
    m_conflictsEncountered = 0;
    emit positionChanged(m_column, m_row);

    // Рекурсивно сбрасываем соседа
    if (m_neighbor != nullptr)
        m_neighbor->reset();
}

// ============================================================================
// Методы работы с базой знаний
// ============================================================================

/**
 *  Возвращает вес текущей позиции
 * Возвращает Вес клетки из БЗ или 1 по умолчанию
 *
 * Вес используется для оценки качества решения.
 * Чем больше вес - тем "лучше" клетка (или наоборот, в зависимости от контекста).
 */
int QueenAgent::getCurrentPositionWeight() const
{
    if (m_knowledgeBase) {
        return m_knowledgeBase->getPositionWeight(m_column, m_row);
    }
    return 1;  // Вес по умолчанию
}

/**
 *  Возвращает цвет текущей позиции
 * Возвращает Цвет клетки из БЗ или Green по умолчанию
 *
 * Цвет используется для группировки решений:
 * Все ферзи в одном решении должны быть одного цвета.
 */
PositionColor QueenAgent::getCurrentPositionColor() const
{
    if (m_knowledgeBase) {
        return m_knowledgeBase->getPositionColor(m_column, m_row);
    }
    return PositionColor::Green;
}

// ============================================================================
// Расширенные методы поиска (с учетом базы знаний)
// ============================================================================

/**
 *  Проверяет позицию с учетом базы знаний
 * Возвращает true, если позиция валидна
 *
 * Проверяет:
 * 1. Отсутствие атак от соседей
 * 2. Совместимость цветов (все ферзи должны быть одного цвета)
 */
bool QueenAgent::findSolutionWithKnowledge()
{
    if (m_terminal) return false;

    // Проверяем атаки от соседей
    QueenAgent* neighbor = m_neighbor;
    while (neighbor != nullptr) {
        if (neighbor->canAttack(m_row, m_column)) {
            return false; // Конфликт атаки
        }
        neighbor = neighbor->m_neighbor;
    }

    // Проверяем цветовую совместимость
    if (!isColorCompatibleWithNeighbors()) {
        return false; // Цветовой конфликт
    }

    return true;
}

/**
 *  Продвигает ферзя с учетом базы знаний
 * Возвращает true, если удалось продвинуться
 *
 * Аналогичен базовому advance(), но использует findSolutionWithKnowledge()
 * для проверки валидности позиции.
 */
bool QueenAgent::advanceWithKnowledge()
{
    if (m_terminal) return false;

    // Если позиция фиксирована, не можем двигаться
    if (m_fixedRow.has_value()) {
        return false;
    }

    // Пробуем следующую строку
    if (m_row < m_maxRows) {
        m_row++;
        emit positionChanged(m_column, m_row);
        emit positionWeightChanged(m_column, m_row, getCurrentPositionWeight());
        return true;
    }

    // Достигли дна - двигаем соседа
    if (m_neighbor != nullptr) {
        if (!m_neighbor->advanceWithKnowledge()) {
            m_terminal = true;
            return false;
        }
        // Сбрасываем текущую позицию на 1
        m_row = 1;
        emit positionChanged(m_column, m_row);
        emit positionWeightChanged(m_column, m_row, getCurrentPositionWeight());
        return true;
    }

    m_terminal = true;
    return false;
}


// ============================================================================
// Проверка цветовых конфликтов
// ============================================================================

/**
 *  Проверяет, есть ли цветовой конфликт с соседями
 * Возвращает true, если цвет отличается от любого соседа
 *
 * Конфликт возникает, если цвет текущего ферзя отличается
 * от цвета хотя бы одного левого соседа.
 * Это противоречит правилу "все ферзи одного цвета".
 */
bool QueenAgent::hasColorConflict()
{
    return !isColorCompatibleWithNeighbors();
}

/**
 *  Проверяет цветовой конфликт с конкретным ферзем
 *  other Другой ферзь
 * Возвращает true, если цвета разные
 */
bool QueenAgent::hasColorConflictWith(QueenAgent* other) const
{
    if (other == nullptr || !m_knowledgeBase) return false;
    return getCurrentPositionColor() == other->getCurrentPositionColor();
}

/**
 *  Проверяет совместимость цвета с цветами всех соседей
 * Возвращает true, если цвет совпадает со всеми соседями
 *
 * Правило: все ферзи в решении должны быть одного цвета.
 * Это означает, что текущий цвет должен совпадать с цветом
 * левого соседа (и так рекурсивно).
 */
bool QueenAgent::isColorCompatibleWithNeighbors()
{
    if (!m_knowledgeBase) return true;

    PositionColor myColor = getCurrentPositionColor();
    QueenAgent* neighbor = m_neighbor;

    // Проверяем всех левых соседей
    while (neighbor != nullptr) {
        if (neighbor->getCurrentPositionColor() != myColor) {
            return false; // Найден ферзь другого цвета
        }
        neighbor = neighbor->m_neighbor;
    }

    return true; // Все соседи того же цвета
}

void QueenAgent::setOrderedNeighbors(const std::vector<QueenAgent*>& orderedAgents)
{
    // Устанавливаем цепочку соседей в соответствии с порядком
    for (size_t i = 0; i < orderedAgents.size(); ++i) {
        if (orderedAgents[i] == this) {
            if (i > 0) {
                m_neighbor = orderedAgents[i - 1];
            } else {
                m_neighbor = nullptr;
            }
            break;
        }
    }
    m_allNeighbors = orderedAgents;
}

void QueenAgent::setFixedRow(std::optional<int> fixedRow)
{
    m_fixedRow = fixedRow;
    if (fixedRow.has_value()) {
        m_row = fixedRow.value();
        emit positionChanged(m_column, m_row);
    }
}

