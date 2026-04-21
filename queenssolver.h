#ifndef QUEENSSOLVER_H
#define QUEENSSOLVER_H

#include <QObject>
#include <vector>
#include <atomic>
#include <map>
#include <set>
#include <optional>
#include <unordered_set>
#include <bitset>
#include "KnowledgeBase.h"
#include "PriorityManager.h"

struct SolutionWithCost {
    std::vector<int> positions;
    int totalWeight = 0;
    std::map<int, PositionColor> colorMap;
};

class QueensSolver : public QObject
{
    Q_OBJECT

public:
    explicit QueensSolver(int boardSize, QObject* parent = nullptr);
    explicit QueensSolver(int boardSize, KnowledgeBase* knowledgeBase, QObject* parent = nullptr);
    ~QueensSolver();

    int getBoardSize() const { return m_boardSize; }
    void setFixedPositions(const std::vector<std::optional<int>>& fixedPositions);
    void setPriorityManager(PriorityManager* manager);
    void recalculateAllWeights();

public slots:
    void solve();
    void stop();
    void sortSolutionsByCost(bool ascending = true, int sortType = 0);

    const std::vector<std::vector<int>>& getAllSolutions() const { return m_allSolutions; }
    const std::vector<SolutionWithCost>& getSolutionsWithCost() const { return m_solutionsWithCost; }

signals:
    void solutionFound(const std::vector<int>& solution);
    void solutionWithCostFound(const SolutionWithCost& solution);
    void progressUpdated(int current, int total);
    void finished(int totalSolutions);
    void error(const QString& error);
    void solutionPrinted(const QString& solutionText);
    void allSolutionsFound(const std::vector<std::vector<int>>& solutions);
    void allSolutionsWithCostReady(const std::vector<SolutionWithCost>& solutions);
    void sortingCompleted(int count, bool ascending);

private:
    // Оптимизированный рекурсивный поиск с битовыми масками
    void solveOptimized(int row, unsigned long long cols, unsigned long long diag1, unsigned long long diag2);

    // Поиск с учетом фиксированных позиций
    void solveWithFixed(int col);

    // Проверка позиции
    bool isSafe(int col, int row, const std::vector<int>& board) const;
    bool isColorCompatible(int col, int row, const std::vector<int>& board) const;

    QString formatSolution(const std::vector<int>& solution, int solutionNumber = 0) const;
    int getExpectedSolutionCount(int n) const;
    bool validateColorCompatibility(const std::vector<int>& solution) const;
    void calculateCostsForSolution(SolutionWithCost& solution) const;
    std::map<int, PositionColor> getColorsForSolution(const std::vector<int>& solution) const;
    bool areFixedPositionsValid() const;
    int getFirstNonFixedColumn() const;
    bool isValidSolution(const std::vector<int>& board) const;
    void updateAgentOrder();

private:
    int m_boardSize;
    std::vector<std::vector<int>> m_allSolutions;
    std::atomic<bool> m_stopRequested;
    std::atomic<bool> m_isSolving;

    KnowledgeBase* m_knowledgeBase;
    std::vector<SolutionWithCost> m_solutionsWithCost;
    bool m_useKnowledgeBase;

    std::vector<std::optional<int>> m_fixedPositions;
    std::vector<int> m_agentOrder;
    PriorityManager* m_priorityManager;

    // Кэш для оптимизации
    std::vector<std::vector<bool>> m_colorCache;
    std::vector<std::vector<int>> m_weightCache;

    // Текущее состояние для оптимизированного поиска
    std::vector<int> m_currentBoard;
    unsigned long long m_allSolutionsMask;
    int m_solutionCount;
    int m_expectedTotal;

    static const std::map<int, int> s_expectedSolutions;
};

#endif // QUEENSSOLVER_H