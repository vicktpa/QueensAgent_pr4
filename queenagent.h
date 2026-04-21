#ifndef QUEENAGENT_H
#define QUEENAGENT_H

#include <QObject>
#include <vector>
#include <optional>

#include "KnowledgeBase.h"

// Forward declaration
class KnowledgeBase;
enum class PositionColor;

/**
 *  Класс, представляющий одного ферзя как автономного агента
 *
 * Реализует распределенный поиск решений задачи о ферзях.
 * Каждый ферзь (агент) отвечает за свой столбец и взаимодействует
 * с соседним ферзем (слева) для нахождения непротиворечивых позиций.
 *
 * Архитектура:
 * - Ферзи выстроены в цепочку (связный список)
 * - Каждый ферзь знает только о своем левом соседе
 * - Поиск происходит путем "переполнения" - когда ферзь достигает дна,
 *   он двигает соседа и сбрасывается наверх
 *
 * Поддержка базы знаний:
 * - Учитывает вес клеток (для оценки решений)
 * - Учитывает цвет клеток (все ферзи решения должны быть одного цвета)
 *
 * Поддержка фиксации:
 * - Если позиция зафиксирована, ферзь не двигается при поиске
 */
class QueenAgent : public QObject
{
    Q_OBJECT

public:
    // ========================================================================
    // Конструкторы
    // ========================================================================

    QueenAgent(int column, QueenAgent* neighbor = nullptr, QObject* parent = nullptr);
    QueenAgent(int column, KnowledgeBase* knowledgeBase,
               QueenAgent* neighbor = nullptr, QObject* parent = nullptr);
    ~QueenAgent();

    // ========================================================================
    // Базовые геттеры и сеттеры
    // ========================================================================

    int getRow() const { return m_row; }
    int getColumn() const { return m_column; }
    QueenAgent* getNeighbor() const { return m_neighbor; }
    void setNeighbor(QueenAgent* neighbor) { m_neighbor = neighbor; }
    void setMaxRows(int maxRows) { m_maxRows = maxRows; }
    bool isTerminal() const { return m_terminal; }
    void setTerminal(bool terminal) { m_terminal = terminal; }
    int getMaxRows() const { return m_maxRows; }
    void setRow(int row) { m_row = row; }

    // ========================================================================
    // Проверка атак
    // ========================================================================

    bool canAttack(int testRow, int testColumn) const;

    // ========================================================================
    // Основные методы поиска (без базы знаний)
    // ========================================================================

    bool findSolution();
    bool advance();
    void reset();

    // ========================================================================
    // Расширенные методы (с поддержкой базы знаний)
    // ========================================================================

    void setKnowledgeBase(KnowledgeBase* kb) { m_knowledgeBase = kb; }
    KnowledgeBase* getKnowledgeBase() const { return m_knowledgeBase; }
    bool hasKnowledgeBase() const { return m_knowledgeBase != nullptr; }

    int getCurrentPositionWeight() const;
    PositionColor getCurrentPositionColor() const;
    bool findSolutionWithKnowledge();
    bool advanceWithKnowledge();
    bool hasColorConflict();
    bool hasColorConflictWith(QueenAgent* other) const;

    // ========================================================================
    // Индивидуализация агента и фиксация позиции
    // ========================================================================

    void setAggressiveness(double agg) { m_aggressiveness = qBound(0.0, agg, 1.0); }
    double getAggressiveness() const { return m_aggressiveness; }

    void setOrderedNeighbors(const std::vector<QueenAgent*>& orderedAgents);

    /**
     *  Устанавливает фиксированную строку для агента
     *  fixedRow Строка (0-индексация) или nullopt для снятия фиксации
     */
    void setFixedRow(std::optional<int> fixedRow);

    /**
     *  Возвращает фиксированную строку или nullopt
     */
    std::optional<int> getFixedRow() const { return m_fixedRow; }

    /**
     *  Проверяет, зафиксирована ли позиция агента
     */
    bool isFixed() const { return m_fixedRow.has_value(); }

signals:
    void positionChanged(int column, int row);                              ///< Изменение позиции
    void colorConflictDetected(int column, int row, PositionColor color);   ///< Обнаружен цветовой конфликт
    void positionWeightChanged(int column, int row, int weight);            ///< Изменение веса позиции

private:
    bool isColorCompatibleWithNeighbors();

private:
    // Базовые свойства
    int m_column;           ///< Номер столбца (фиксирован, 0-индексация)
    int m_row;              ///< Текущая строка (1-индексация, от 1 до m_maxRows)
    QueenAgent* m_neighbor; ///< Указатель на соседнего ферзя (слева)
    int m_maxRows;          ///< Максимальное количество строк (размер доски)
    bool m_terminal;        ///< Флаг терминального состояния (достигли конца)

    // Расширенные свойства
    KnowledgeBase* m_knowledgeBase;     ///< Указатель на базу знаний
    double m_aggressiveness;            ///< Агрессивность агента (0-1)
    std::vector<int> m_badRows;         ///< "Память" - строки, которые уже пробовал
    int m_conflictsEncountered;         ///< Счётчик конфликтов

    std::optional<int> m_fixedRow;      ///< Фиксированная строка (если есть)
    std::vector<QueenAgent*> m_allNeighbors; ///< Для быстрого доступа ко всем соседям
};

#endif // QUEENAGENT_H