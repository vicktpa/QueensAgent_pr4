#include "QueensSolver.h"
#include <QThread>
#include <QElapsedTimer>
#include <set>
#include <algorithm>
#include <cmath>
#include <QDebug>

const std::map<int, int> QueensSolver::s_expectedSolutions = {
    {1, 1}, {2, 0}, {3, 0}, {4, 2}, {5, 10}, {6, 4}, {7, 40},
    {8, 92}, {9, 352}, {10, 724}, {11, 2680}, {12, 14200},
    {13, 73712}, {14, 365596}, {15, 2279184}
};

QueensSolver::QueensSolver(int boardSize, QObject* parent)
    : QObject(parent)
    , m_boardSize(boardSize)
    , m_stopRequested(false)
    , m_isSolving(false)
    , m_knowledgeBase(nullptr)
    , m_useKnowledgeBase(false)
    , m_priorityManager(nullptr)
    , m_solutionCount(0)
    , m_expectedTotal(0)
{
    m_fixedPositions.resize(m_boardSize, std::nullopt);
    m_agentOrder.resize(m_boardSize);
    for (int i = 0; i < m_boardSize; ++i) {
        m_agentOrder[i] = i;
    }
    m_currentBoard.resize(m_boardSize, -1);
}

QueensSolver::QueensSolver(int boardSize, KnowledgeBase* knowledgeBase, QObject* parent)
    : QObject(parent)
    , m_boardSize(boardSize)
    , m_stopRequested(false)
    , m_isSolving(false)
    , m_knowledgeBase(knowledgeBase)
    , m_useKnowledgeBase(true)
    , m_priorityManager(nullptr)
    , m_solutionCount(0)
    , m_expectedTotal(0)
{
    m_fixedPositions.resize(m_boardSize, std::nullopt);
    m_agentOrder.resize(m_boardSize);
    for (int i = 0; i < m_boardSize; ++i) {
        m_agentOrder[i] = i;
    }
    m_currentBoard.resize(m_boardSize, -1);

    // Предварительно кэшируем цвета и веса для быстрого доступа
    if (m_knowledgeBase) {
        m_colorCache.resize(m_boardSize, std::vector<bool>(m_boardSize, true));
        m_weightCache.resize(m_boardSize, std::vector<int>(m_boardSize, 1));

        for (int col = 0; col < m_boardSize; ++col) {
            for (int row = 0; row < m_boardSize; ++row) {
                m_weightCache[col][row] = m_knowledgeBase->getPositionWeight(col, row + 1);
            }
        }
    }
}

QueensSolver::~QueensSolver() {
    stop();
}

// ============================================================================
// ОПТИМИЗИРОВАННЫЙ АЛГОРИТМ ПОИСКА С БИТОВЫМИ МАСКАМИ
// ============================================================================

void QueensSolver::solveOptimized(int row, unsigned long long cols,
                                  unsigned long long diag1,
                                  unsigned long long diag2)
{
    if (m_stopRequested) return;

    if (row == m_boardSize) {
        // Найдено решение
        m_solutionCount++;

        std::vector<int> solution = m_currentBoard;
        m_allSolutions.push_back(solution);

        if (m_useKnowledgeBase && m_knowledgeBase) {
            SolutionWithCost sol;
            sol.positions = solution;
            sol.colorMap = getColorsForSolution(solution);
            calculateCostsForSolution(sol);
            m_solutionsWithCost.push_back(sol);
        }

        emit solutionFound(solution);

        // Обновляем прогресс каждые 10 решений
        if (m_solutionCount % 10 == 0 || m_solutionCount == m_expectedTotal) {
            emit progressUpdated(m_solutionCount, m_expectedTotal);
        }

        return;
    }

    // Пропускаем фиксированные строки
    if (row < static_cast<int>(m_fixedPositions.size()) &&
        m_fixedPositions[row].has_value()) {
        int fixedRow = m_fixedPositions[row].value();

        // Проверяем, не конфликтует ли фиксированная позиция
        unsigned long long bit = 1ULL << fixedRow;
        if (!(cols & bit) && !(diag1 & (bit << row)) && !(diag2 & (bit >> row))) {
            m_currentBoard[row] = fixedRow;
            solveOptimized(row + 1, cols | bit, diag1 | (bit << row), diag2 | (bit >> row));
        }
        return;
    }

    // Получаем доступные позиции (битовые маски)
    unsigned long long available = m_allSolutionsMask & ~(cols | diag1 | diag2);

    // Перебираем доступные позиции
    while (available && !m_stopRequested) {
        // Извлекаем младший установленный бит (быстрее чем цикл)
        unsigned long long bit = available & -available;
        available ^= bit;

        // Получаем индекс строки из бита
        int r = __builtin_ctzll(bit);  // Быстрое получение позиции бита

        // Проверка цвета (если используется KnowledgeBase)
        if (m_useKnowledgeBase && m_knowledgeBase) {
            if (!isColorCompatible(row, r, m_currentBoard)) {
                continue;
            }
        }

        m_currentBoard[row] = r;
        solveOptimized(row + 1, cols | bit, (diag1 | bit) << 1, (diag2 | bit) >> 1);
    }

    m_currentBoard[row] = -1;
}

// ============================================================================
// ОПТИМИЗИРОВАННЫЙ ПОИСК С ФИКСИРОВАННЫМИ ПОЗИЦИЯМИ
// ============================================================================

void QueensSolver::solveWithFixed(int col)
{
    if (m_stopRequested) return;

    // Пропускаем фиксированные столбцы
    while (col < m_boardSize && m_fixedPositions[col].has_value()) {
        col++;
    }

    if (col == m_boardSize) {
        // Все столбцы заполнены
        if (isValidSolution(m_currentBoard)) {
            m_solutionCount++;
            std::vector<int> solution = m_currentBoard;
            m_allSolutions.push_back(solution);
            emit solutionFound(solution);

            if (m_solutionCount % 10 == 0) {
                emit progressUpdated(m_solutionCount, m_expectedTotal);
            }
        }
        return;
    }

    // Используем порядок из приоритетов
    int actualCol = (col < static_cast<int>(m_agentOrder.size())) ? m_agentOrder[col] : col;

    // Пропускаем если уже фиксирован
    if (m_fixedPositions[actualCol].has_value()) {
        solveWithFixed(col + 1);
        return;
    }

    // Перебираем строки
    for (int row = 0; row < m_boardSize && !m_stopRequested; row++) {
        if (isSafe(actualCol, row, m_currentBoard)) {
            if (m_useKnowledgeBase && !isColorCompatible(actualCol, row, m_currentBoard)) {
                continue;
            }

            m_currentBoard[actualCol] = row;
            solveWithFixed(col + 1);
            m_currentBoard[actualCol] = -1;
        }
    }
}

// ============================================================================
// ОСНОВНОЙ МЕТОД ПОИСКА
// ============================================================================

void QueensSolver::solve()
{
    if (m_isSolving) return;

    m_stopRequested = false;
    m_isSolving = true;
    m_allSolutions.clear();
    m_solutionsWithCost.clear();
    m_solutionCount = 0;

    updateAgentOrder();

    // Проверяем фиксированные позиции
    if (!areFixedPositionsValid()) {
        emit error("Фиксированные позиции несовместимы!");
        emit finished(0);
        m_isSolving = false;
        return;
    }

    m_expectedTotal = getExpectedSolutionCount(m_boardSize);

    // Инициализируем маску всех возможных позиций
    m_allSolutionsMask = (1ULL << m_boardSize) - 1;
    m_currentBoard.assign(m_boardSize, -1);

    QElapsedTimer timer;
    timer.start();

    bool hasFixed = false;
    for (int i = 0; i < m_boardSize; ++i) {
        if (m_fixedPositions[i].has_value()) {
            hasFixed = true;
            m_currentBoard[i] = m_fixedPositions[i].value();
        }
    }

    if (hasFixed) {
        // Используем поиск с фиксированными позициями
        solveWithFixed(0);
    } else {
        // Используем оптимизированный поиск с битовыми масками
        solveOptimized(0, 0, 0, 0);
    }

    qint64 elapsed = timer.elapsed();
    qDebug() << "Поиск завершен за" << elapsed << "мс, найдено решений:" << m_solutionCount;

    // Сохраняем решения с весами
    if (m_useKnowledgeBase && m_knowledgeBase && !hasFixed) {
        for (const auto& sol : m_allSolutions) {
            SolutionWithCost solutionWithCost;
            solutionWithCost.positions = sol;
            solutionWithCost.colorMap = getColorsForSolution(sol);
            calculateCostsForSolution(solutionWithCost);
            m_solutionsWithCost.push_back(solutionWithCost);
        }
        emit allSolutionsWithCostReady(m_solutionsWithCost);
    }

    if (!m_allSolutions.empty()) {
        emit allSolutionsFound(m_allSolutions);
    }

    emit finished(m_allSolutions.size());
    m_isSolving = false;
}

// ============================================================================
// БЫСТРАЯ ПРОВЕРКА БЕЗОПАСНОСТИ
// ============================================================================

bool QueensSolver::isSafe(int col, int row, const std::vector<int>& board) const
{
    // Проверяем только уже размещенных ферзей
    for (int otherCol = 0; otherCol < col; ++otherCol) {
        int otherRow = board[otherCol];
        if (otherRow == -1) continue;

        // Быстрая проверка
        if (otherRow == row) return false;
        if (std::abs(otherRow - row) == std::abs(otherCol - col)) return false;
    }
    return true;
}

bool QueensSolver::isColorCompatible(int col, int row, const std::vector<int>& board) const
{
    if (!m_knowledgeBase) return true;

    PositionColor currentColor = m_knowledgeBase->getPositionColor(col, row + 1);

    // Проверяем только уже размещенных ферзей
    for (int prevCol = 0; prevCol < col; prevCol++) {
        if (board[prevCol] != -1) {
            PositionColor prevColor = m_knowledgeBase->getPositionColor(prevCol, board[prevCol] + 1);
            if (currentColor != prevColor) {
                return false;
            }
        }
    }
    return true;
}

// ============================================================================
// ОСТАЛЬНЫЕ МЕТОДЫ (без изменений)
// ============================================================================

void QueensSolver::setFixedPositions(const std::vector<std::optional<int>>& fixedPositions)
{
    m_fixedPositions = fixedPositions;
}

void QueensSolver::setPriorityManager(PriorityManager* manager)
{
    m_priorityManager = manager;
    updateAgentOrder();
}

void QueensSolver::updateAgentOrder()
{
    m_agentOrder.clear();
    for (int i = 0; i < m_boardSize; ++i) {
        m_agentOrder.push_back(i);
    }

    if (!m_priorityManager) return;

    std::sort(m_agentOrder.begin(), m_agentOrder.end(), [this](int a, int b) {
        int prioA = m_priorityManager->getPriority(a);
        int prioB = m_priorityManager->getPriority(b);

        bool fixedA = m_fixedPositions[a].has_value();
        bool fixedB = m_fixedPositions[b].has_value();

        if (fixedA && !fixedB) return true;
        if (!fixedA && fixedB) return false;
        if (prioA == 0 && prioB > 0) return false;
        if (prioA > 0 && prioB == 0) return true;
        if (prioA > 0 && prioB > 0) return prioA < prioB;

        return a < b;
    });
}

bool QueensSolver::areFixedPositionsValid() const
{
    for (int col1 = 0; col1 < m_boardSize; ++col1) {
        if (!m_fixedPositions[col1].has_value()) continue;
        int row1 = m_fixedPositions[col1].value();

        for (int col2 = col1 + 1; col2 < m_boardSize; ++col2) {
            if (!m_fixedPositions[col2].has_value()) continue;
            int row2 = m_fixedPositions[col2].value();

            if (row1 == row2) return false;
            if (std::abs(row1 - row2) == std::abs(col1 - col2)) return false;
        }
    }
    return true;
}

bool QueensSolver::isValidSolution(const std::vector<int>& board) const
{
    for (int col1 = 0; col1 < m_boardSize; ++col1) {
        if (board[col1] == -1) return false;

        for (int col2 = col1 + 1; col2 < m_boardSize; ++col2) {
            if (board[col2] == -1) return false;

            if (board[col1] == board[col2]) return false;
            if (std::abs(board[col1] - board[col2]) == std::abs(col1 - col2)) return false;
        }
    }
    return true;
}

void QueensSolver::stop()
{
    m_stopRequested = true;
    while (m_isSolving) {
        QThread::msleep(10);
    }
}

int QueensSolver::getExpectedSolutionCount(int n) const
{
    auto it = s_expectedSolutions.find(n);
    return (it != s_expectedSolutions.end()) ? it->second : 0;
}

void QueensSolver::calculateCostsForSolution(SolutionWithCost& solution) const
{
    if (!m_knowledgeBase) return;

    solution.totalWeight = 0;
    for (int col = 0; col < static_cast<int>(solution.positions.size()); ++col) {
        if (solution.positions[col] != -1) {
            solution.totalWeight += m_weightCache[col][solution.positions[col]];
        }
    }
}

std::map<int, PositionColor> QueensSolver::getColorsForSolution(const std::vector<int>& solution) const
{
    std::map<int, PositionColor> colors;
    if (!m_knowledgeBase) return colors;

    for (int col = 0; col < static_cast<int>(solution.size()); ++col) {
        if (solution[col] != -1) {
            colors[col] = m_knowledgeBase->getPositionColor(col, solution[col] + 1);
        }
    }
    return colors;
}

QString QueensSolver::formatSolution(const std::vector<int>& solution, int solutionNumber) const
{
    QString result;
    if (solutionNumber > 0) {
        result = QString("\nРешение #%1: ").arg(solutionNumber);
    }
    for (int i = 0; i < static_cast<int>(solution.size()); ++i) {
        if (solution[i] != -1) {
            result += QString("%1%2 ").arg(QChar('A' + i)).arg(solution[i] + 1);
        }
    }
    return result;
}

void QueensSolver::sortSolutionsByCost(bool ascending, int sortType)
{
    Q_UNUSED(sortType)

    if (!m_useKnowledgeBase || m_solutionsWithCost.empty()) return;

    if (ascending) {
        std::sort(m_solutionsWithCost.begin(), m_solutionsWithCost.end(),
                  [](const SolutionWithCost& a, const SolutionWithCost& b) {
                      return a.totalWeight < b.totalWeight;
                  });
    } else {
        std::sort(m_solutionsWithCost.begin(), m_solutionsWithCost.end(),
                  [](const SolutionWithCost& a, const SolutionWithCost& b) {
                      return a.totalWeight > b.totalWeight;
                  });
    }

    m_allSolutions.clear();
    for (const auto& sol : m_solutionsWithCost) {
        m_allSolutions.push_back(sol.positions);
    }

    emit sortingCompleted(m_solutionsWithCost.size(), ascending);
}

void QueensSolver::recalculateAllWeights()
{
    if (!m_useKnowledgeBase || !m_knowledgeBase) return;

    for (auto& sol : m_solutionsWithCost) {
        calculateCostsForSolution(sol);
    }

    emit sortingCompleted(m_solutionsWithCost.size(), true);
}