#ifndef KNOWLEDGEBASE_H
#define KNOWLEDGEBASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMap>
#include <QColor>
#include <vector>

/**
 *  Перечисление возможных цветов для клеток в базе знаний
 *
 * Каждый цвет используется для группировки решений -
 * все ферзи в решении должны быть одного цвета.
 */
enum class PositionColor {
    Red,    // Красный цвет
    Green,  // Зеленый цвет (значение по умолчанию)
    Blue    // Синий цвет
};

/**
 *  Класс для управления базой знаний о клетках шахматной доски
 *
 * База знаний хранит для каждой клетки (столбец, строка):
 * - Вес клетки (1-100) - используется для сортировки решений
 * - Цвет клетки (Red/Green/Blue) - все ферзи решения должны быть одного цвета
 *
 * Данные хранятся в SQLite базе данных и кэшируются для быстрого доступа.
 */
class KnowledgeBase : public QObject
{
    Q_OBJECT

public:
    explicit KnowledgeBase(QObject* parent = nullptr);
    ~KnowledgeBase();

    // ========================================================================
    // Инициализация и работа с файлами
    // ========================================================================

    /**
     *  Инициализирует SQLite базу данных
     *  dbPath Путь к файлу базы данных (":memory:" для временной БД)
     *  Возвращает true в случае успеха, false при ошибке
     */
    bool initializeDatabase(const QString& dbPath = ":memory:");

    /**
     *  Загружает базу знаний из текстового файла
     *  filePath Путь к файлу
     *  Возвращает true в случае успеха, false при ошибке
     *
     * Формат файла:
     * #BOARD_SIZE:8
     * #Комментарий
     * column,row,weight,color
     */
    bool loadFromFile(const QString& filePath);

    /**
     *  Сохраняет базу знаний в текстовый файл
     *  filePath Путь к файлу
     *  Возвращаетtrue в случае успеха, false при ошибке
     */
    bool saveToFile(const QString& filePath);

    // ========================================================================
    // Получение данных
    // ========================================================================

    /**
     *  Возвращает вес клетки (1-100) или 1 по умолчанию
     *  column Столбец (0-индексация)
     *  row Строка (1-индексация, как в шахматах)
     */
    int getPositionWeight(int column, int row) const;

    /**
     *  Возвращает цвет клетки (Green по умолчанию)
     *  column Столбец (0-индексация)
     *  row Строка (1-индексация)
     */
    PositionColor getPositionColor(int column, int row) const;

    /**
     *  Возвращает QColor для отображения
     *  color Цвет из перечисления PositionColor
     */
    QColor getColorValue(PositionColor color) const;

    /**
     *  Структура для хранения полной информации о клетке
     */
    struct PositionInfo {
        int weight;           // Вес клетки
        PositionColor color;  // Цвет клетки
    };

    /**
     *  Возвращает полную информацию о клетке (структура PositionInfo)
     *  column Столбец
     *  row Строка
     */
    PositionInfo getPositionInfo(int column, int row) const;

    // ========================================================================
    // Установка данных
    // ========================================================================

    /**
     *  Устанавливает вес клетки
     *  column Столбец
     *  row Строка
     *  weight Новый вес (1-100)
     */
    void setPositionWeight(int column, int row, int weight);

    /**
     *  Устанавливает цвет клетки
     *  column Столбец
     *  row Строка
     *  color Новый цвет
     */
    void setPositionColor(int column, int row, PositionColor color);

    // ========================================================================
    // Генерация данных
    // ========================================================================

    /**
     *  Генерирует случайные веса и цвета для всех клеток
     *  boardSize Размер доски
     *
     * Распределение цветов: равномерное (33% каждый цвет)
     * Веса: случайные от 1 до 20
     */
    void generateRandomKnowledge(int boardSize);

    /**
     *  Генерирует распределение только из двух цветов (50/50)
     *  boardSize Размер доски
     *
     * Используются только зеленый и красный цвета.
     */
    void generateTwoColorDistribution(int boardSize);

    /**
     *  Генерирует структурированные данные на основе позиции
     *  boardSize Размер доски
     *
     * Вес зависит от положения клетки (диагонали имеют больший вес).
     */
    void generateStructuredKnowledge(int boardSize);

    // ========================================================================
    // Анализ решений
    // ========================================================================

    /**
     *  Вычисляет суммарный вес решения
     *  solution Вектор позиций ферзей
     */
    int getTotalWeightForSolution(const std::vector<int>& solution) const;

    /**
     *  Возвращает распределение цветов в базе знаний
     */
    QMap<PositionColor, int> getColorDistribution() const;

    /**
     *  Возвращает средний вес по всем клеткам
     */
    int getAverageWeight() const;

    // ========================================================================
    // Вспомогательные методы
    // ========================================================================

    /**
     *  Проверяет, корректно ли инициализирована база знаний
     *  Возвращает true, если для текущего размера доски есть все клетки
     */
    bool isValid() const;

    int getBoardSize() const { return m_boardSize; }
    void setBoardSize(int size) { m_boardSize = size; }

    /**
     *  Очищает базу знаний и устанавливает новый размер доски
     *  boardSize Новый размер доски
     */
    void clearAndSetBoardSize(int boardSize);

    /**
     *  Возвращает ссылку на объект базы данных
     */
    QSqlDatabase& getDatabase() { return m_database; }

    /**
     *  Проверяет, совместимы ли цвета в решении
     *  solution Вектор позиций ферзей
     *  Возвращает true, если все ферзи одного цвета
     */
    bool validateColorCompatibility(const std::vector<int>& solution) const;

signals:
    /**  Сигнал, испускаемый при обновлении базы знаний */
    void knowledgeUpdated();

    /**  Сигнал, испускаемый при возникновении ошибки */
    void errorOccurred(const QString& error);

private:
    // ========================================================================
    // Приватные методы
    // ========================================================================

    /**
     *  Создает необходимые таблицы в базе данных
     *  Возвращает true в случае успеха
     */
    bool createTables();

    /**
     *  Вставляет или обновляет данные о клетке в БД
     *  column Столбец
     *  row Строка
     *  weight Вес
     *  color Цвет в виде строки ("Red", "Green", "Blue")
     *  Возвращает true в случае успеха
     */
    bool insertPositionData(int column, int row, int weight, const QString& color);

    /**
     *  Формирует ключ для кэша
     *  column Столбец
     *  row Строка
     *  Возвращает строковый ключ вида "column,row"
     */
    QString makeKey(int column, int row) const { return QString("%1,%2").arg(column).arg(row); }

private:
    // ========================================================================
    // Приватные поля
    // ========================================================================

    QSqlDatabase m_database;                    // Объект SQLite базы данных
    int m_boardSize;                            // Текущий размер доски

    /**
     *  Кэш данных для быстрого доступа
     *
     * Ключ: строка "column,row"
     * Значение: PositionInfo {weight, color}
     * Используется для избежания частых запросов к БД
     */
    mutable QMap<QString, PositionInfo> m_cache;
};

#endif // KNOWLEDGEBASE_H