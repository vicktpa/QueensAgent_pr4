#include "KnowledgeBase.h"
#include <QFile>
#include <QTextStream>
#include <QRandomGenerator>
#include <QSqlRecord>

// ============================================================================
// Конструктор и деструктор
// ============================================================================

KnowledgeBase::KnowledgeBase(QObject* parent)
    : QObject(parent)
    , m_boardSize(0)           // Размер доски пока не установлен
{}

KnowledgeBase::~KnowledgeBase(){
    // Закрываем соединение с БД при уничтожении объекта
    if (m_database.isOpen()) {
        m_database.close();
    }
}

// ============================================================================
// Инициализация базы данных
// ============================================================================

/**
 *  Инициализирует SQLite базу данных
 *  dbPath Путь к файлу БД (по умолчанию ":memory:" для временной БД)
 * Возвращает true в случае успеха
 *
 * Создает соединение с SQLite и создает необходимые таблицы.
 */
bool KnowledgeBase::initializeDatabase(const QString& dbPath)
{
    // Добавляем новое соединение с SQLite
    m_database = QSqlDatabase::addDatabase("QSQLITE");
    m_database.setDatabaseName(dbPath);

    // Открываем базу данных
    if (!m_database.open()) {
        emit errorOccurred("Не удалось открыть базу данных: " + m_database.lastError().text());
        return false;
    }

    return createTables();
}

/**
 *  Создает таблицы в базе данных
 * Возвращает true в случае успеха
 *
 * Создает таблицу position_knowledge со следующими полями:
 * - id: первичный ключ
 * - board_size: размер доски
 * - column_num: номер столбца (0-индексация)
 * - row_num: номер строки (1-индексация)
 * - weight: вес клетки (1-100)
 * - color: цвет клетки (Red/Green/Blue)
 *
 * Также создает индексы для ускорения запросов.
 */
bool KnowledgeBase::createTables()
{
    QSqlQuery query(m_database);

    // SQL для создания таблицы
    QString createTableSQL =
        "CREATE TABLE IF NOT EXISTS position_knowledge ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "board_size INTEGER NOT NULL, "
        "column_num INTEGER NOT NULL, "
        "row_num INTEGER NOT NULL, "
        "weight INTEGER NOT NULL DEFAULT 1, "
        "color TEXT NOT NULL DEFAULT 'Green', "
        "UNIQUE(board_size, column_num, row_num)"  // Уникальность: одна запись на клетку
        ")";

    if (!query.exec(createTableSQL)) {
        emit errorOccurred("Ошибка создания таблицы: " + query.lastError().text());
        return false;
    }

    // Создаем индексы для ускорения поиска
    query.exec("CREATE INDEX IF NOT EXISTS idx_position ON position_knowledge(board_size, column_num, row_num)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_weight ON position_knowledge(weight)");

    return true;
}

// ============================================================================
// Загрузка и сохранение в файл
// ============================================================================

/**
 *  Загружает базу знаний из текстового файла
 *  filePath Путь к файлу
 * Возвращает true в случае успеха
 *
 * Формат файла:
 * #BOARD_SIZE:8
 * #Комментарий
 * column,row,weight,color
 *
 * Пример:
 * 0,1,15,Green
 * 0,2,8,Red
 */
bool KnowledgeBase::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred("Не удалось открыть файл: " + filePath);
        return false;
    }

    QTextStream stream(&file);
    QString line = stream.readLine();

    // Читаем размер доски из первой строки
    if (!line.startsWith("#BOARD_SIZE:")) {
        emit errorOccurred("Неверный формат файла: отсутствует размер доски");
        return false;
    }
    m_boardSize = line.mid(12).toInt();  // Извлекаем число после "#BOARD_SIZE:"

    // Удаляем старые данные для этого размера доски
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM position_knowledge WHERE board_size = :size");
    query.bindValue(":size", m_boardSize);
    query.exec();

    // Очищаем кэш
    m_cache.clear();

    // Читаем и парсим строки файла
    int lineNum = 0;
    while (!stream.atEnd()) {
        line = stream.readLine();
        lineNum++;

        // Пропускаем пустые строки и комментарии
        if (line.isEmpty() || line.startsWith("#")) continue;

        // Разбиваем строку на части
        QStringList parts = line.split(",");
        if (parts.size() != 4) {
            qWarning() << "Пропущена строка" << lineNum << ": неверный формат";
            continue;
        }

        int col = parts[0].toInt();
        int row = parts[1].toInt();
        int weight = parts[2].toInt();
        QString colorStr = parts[3];

        // Преобразуем строку цвета в перечисление
        PositionColor color;
        if (colorStr == "Red") color = PositionColor::Red;
        else if (colorStr == "Green") color = PositionColor::Green;
        else if (colorStr == "Blue") color = PositionColor::Blue;
        else continue;  // Неизвестный цвет - пропускаем

        // Вставляем данные в БД
        insertPositionData(col, row, weight, colorStr);

        // Сохраняем в кэш
        PositionInfo info{ weight, color };
        m_cache[makeKey(col, row)] = info;
    }

    file.close();
    emit knowledgeUpdated();
    return true;
}

/**
 *  Сохраняет базу знаний в текстовый файл
 *  filePath Путь к файлу
 * Возвращает true в случае успеха
 */
bool KnowledgeBase::saveToFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred("Не удалось сохранить файл: " + filePath);
        return false;
    }

    QTextStream stream(&file);

    // Записываем заголовок файла
    stream << "#BOARD_SIZE:" << m_boardSize << "\n";
    stream << "#Формат: column,row,weight,color\n";
    stream << "#Цвета: Red, Green, Blue\n";
    stream << "#----------------------------------------\n";

    // Запрашиваем все данные из БД для текущего размера доски
    QSqlQuery query(m_database);
    query.prepare("SELECT column_num, row_num, weight, color FROM position_knowledge "
                  "WHERE board_size = :size ORDER BY column_num, row_num");
    query.bindValue(":size", m_boardSize);

    if (!query.exec()) {
        emit errorOccurred("Ошибка чтения из базы данных: " + query.lastError().text());
        return false;
    }

    // Записываем каждую запись в файл
    while (query.next()) {
        int col = query.value(0).toInt();
        int row = query.value(1).toInt();
        int weight = query.value(2).toInt();
        QString color = query.value(3).toString();

        stream << col << "," << row << "," << weight << "," << color << "\n";
    }

    file.close();
    return true;
}

// ============================================================================
// Работа с данными (вставка, получение)
// ============================================================================

/**
 *  Вставляет или обновляет данные о клетке в БД
 *  column Столбец
 *  row Строка
 *  weight Вес
 *  color Цвет в виде строки
 * Возвращает true в случае успеха
 *
 * Использует INSERT OR REPLACE для обновления существующей записи
 * или вставки новой.
 */
bool KnowledgeBase::insertPositionData(int column, int row, int weight, const QString& color)
{
    QSqlQuery query(m_database);
    query.prepare("INSERT OR REPLACE INTO position_knowledge "
                  "(board_size, column_num, row_num, weight, color) "
                  "VALUES (:size, :col, :row, :weight, :color)");
    query.bindValue(":size", m_boardSize);
    query.bindValue(":col", column);
    query.bindValue(":row", row);
    query.bindValue(":weight", weight);
    query.bindValue(":color", color);

    return query.exec();
}

/**
 *  Возвращает вес клетки
 *  column Столбец (0-индексация)
 *  row Строка (1-индексация)
 * Возвращает вес клетки или 1, если данные не найдены
 *
 * Сначала проверяет кэш, затем обращается к БД.
 */
int KnowledgeBase::getPositionWeight(int column, int row) const
{
    // Проверяем кэш
    QString key = makeKey(column, row);
    if (m_cache.contains(key)) {
        return m_cache[key].weight;
    }

    // Если нет в кэше - идем в БД
    QSqlQuery query(m_database);
    query.prepare("SELECT weight FROM position_knowledge "
                  "WHERE board_size = :size AND column_num = :col AND row_num = :row");
    query.bindValue(":size", m_boardSize);
    query.bindValue(":col", column);
    query.bindValue(":row", row);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    // Значение по умолчанию, если данных нет
    return 1;
}

/**
 *  Возвращает цвет клетки
 *  column Столбец
 *  row Строка
 * Возвращает цвет клетки или Green по умолчанию
 */
PositionColor KnowledgeBase::getPositionColor(int column, int row) const
{
    // Проверяем кэш
    QString key = makeKey(column, row);
    if (m_cache.contains(key)) {
        return m_cache[key].color;
    }

    // Если нет в кэше - идем в БД
    QSqlQuery query(m_database);
    query.prepare("SELECT color FROM position_knowledge "
                  "WHERE board_size = :size AND column_num = :col AND row_num = :row");
    query.bindValue(":size", m_boardSize);
    query.bindValue(":col", column);
    query.bindValue(":row", row);

    if (query.exec() && query.next()) {
        QString colorStr = query.value(0).toString();
        if (colorStr == "Red") return PositionColor::Red;
        if (colorStr == "Green") return PositionColor::Green;
        if (colorStr == "Blue") return PositionColor::Blue;
    }

    // Значение по умолчанию
    return PositionColor::Green;
}

/**
 *  Возвращает QColor для визуализации
 *  color Цвет из перечисления
 * Возвращает QColor с альфа-каналом для полупрозрачности
 */
QColor KnowledgeBase::getColorValue(PositionColor color) const
{
    switch (color) {
    case PositionColor::Red:   return QColor(255, 100, 100, 150);  // Красный с прозрачностью
    case PositionColor::Green: return QColor(100, 255, 100, 150);  // Зеленый с прозрачностью
    case PositionColor::Blue:  return QColor(100, 100, 255, 150);  // Синий с прозрачностью
    default: return QColor(200, 200, 200, 100);  // Серый по умолчанию
    }
}

/**
 *  Возвращает полную информацию о клетке
 *  column Столбец
 *  row Строка
 * Возвращает структура с весом и цветом
 */
KnowledgeBase::PositionInfo KnowledgeBase::getPositionInfo(int column, int row) const
{
    PositionInfo info;
    info.weight = getPositionWeight(column, row);
    info.color = getPositionColor(column, row);
    return info;
}

/**
 *  Устанавливает вес клетки
 *  column Столбец
 *  row Строка
 *  weight Новый вес
 */
void KnowledgeBase::setPositionWeight(int column, int row, int weight)
{
    // Получаем текущий цвет
    PositionColor color = getPositionColor(column, row);
    QString colorStr = (color == PositionColor::Red) ? "Red" :
                           ((color == PositionColor::Green) ? "Green" : "Blue");

    // Сохраняем в БД
    if (insertPositionData(column, row, weight, colorStr)) {
        // Обновляем кэш
        m_cache[makeKey(column, row)] = {weight, color};
        emit knowledgeUpdated();
    }
}

/**
 *  Устанавливает цвет клетки
 *  column Столбец
 *  row Строка
 *  color Новый цвет
 */
void KnowledgeBase::setPositionColor(int column, int row, PositionColor color)
{
    // Получаем текущий вес
    int weight = getPositionWeight(column, row);
    QString colorStr = (color == PositionColor::Red) ? "Red" :
                           ((color == PositionColor::Green) ? "Green" : "Blue");

    // Сохраняем в БД
    if (insertPositionData(column, row, weight, colorStr)) {
        // Обновляем кэш
        m_cache[makeKey(column, row)] = {weight, color};
        emit knowledgeUpdated();
    }
}

// ============================================================================
// Генерация данных
// ============================================================================

/**
 *  Генерирует случайные данные для всех клеток
 *  boardSize Размер доски
 *
 * Каждая клетка получает:
 * - Вес: случайное число от 1 до 20
 * - Цвет: равномерное распределение (33% каждый)
 */
void KnowledgeBase::generateRandomKnowledge(int boardSize)
{
    m_boardSize = boardSize;

    // Удаляем старые данные
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM position_knowledge WHERE board_size = :size");
    query.bindValue(":size", m_boardSize);
    query.exec();
    m_cache.clear();

    QRandomGenerator* gen = QRandomGenerator::global();

    // Генерируем данные для каждой клетки
    for (int col = 0; col < boardSize; ++col) {
        for (int row = 1; row <= boardSize; ++row) {
            int weight = gen->bounded(1, 21);  // Вес от 1 до 20

            // Случайный цвет (равномерное распределение)
            int colorRand = gen->bounded(100);
            PositionColor color;
            if (colorRand < 33) {
                color = PositionColor::Red;
            } else if (colorRand < 66) {
                color = PositionColor::Green;
            } else {
                color = PositionColor::Blue;
            }

            QString colorStr = (color == PositionColor::Red) ? "Red" :
                                   ((color == PositionColor::Green) ? "Green" : "Blue");

            insertPositionData(col, row, weight, colorStr);
            m_cache[makeKey(col, row)] = {weight, color};
        }
    }

    emit knowledgeUpdated();
}

/**
 *  Генерирует распределение только из двух цветов (50/50)
 *  boardSize Размер доски
 *
 * Используются только зеленый и красный цвета.
 * Полезно для тестирования, когда нужно только два варианта.
 */
void KnowledgeBase::generateTwoColorDistribution(int boardSize)
{
    m_boardSize = boardSize;

    // Удаляем старые данные
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM position_knowledge WHERE board_size = :size");
    query.bindValue(":size", m_boardSize);
    query.exec();
    m_cache.clear();

    QRandomGenerator* gen = QRandomGenerator::global();

    for (int col = 0; col < boardSize; ++col) {
        for (int row = 1; row <= boardSize; ++row) {
            int weight = gen->bounded(1, 21);  // Вес от 1 до 20

            // Только красный или зеленый (50/50)
            int colorRand = gen->bounded(100);
            PositionColor color;
            QString colorStr;

            if (colorRand < 50) {
                color = PositionColor::Green;
                colorStr = "Green";
            } else {
                color = PositionColor::Red;
                colorStr = "Red";
            }

            insertPositionData(col, row, weight, colorStr);
            m_cache[makeKey(col, row)] = {weight, color};
        }
    }

    emit knowledgeUpdated();
}

/**
 *  Генерирует структурированные данные на основе позиции
 *  boardSize Размер доски
 *
 * Вес зависит от положения клетки:
 * - Базовый вес: 5 + (col + row) % 15
 * - Главная диагональ (col == row-1): +10
 * - Побочная диагональ (col + row == boardSize): +5
 *
 * Цвет зависит от веса:
 * - вес < 10: красный
 * - вес < 20: зеленый
 * - вес >= 20: синий
 */
void KnowledgeBase::generateStructuredKnowledge(int boardSize)
{
    m_boardSize = boardSize;

    // Удаляем старые данные
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM position_knowledge WHERE board_size = :size");
    query.bindValue(":size", m_boardSize);
    query.exec();
    m_cache.clear();

    for (int col = 0; col < boardSize; ++col) {
        for (int row = 1; row <= boardSize; ++row) {
            // Вычисляем базовый вес
            int weight = 5 + (col + row) % 15;

            // Диагональные бонусы
            if (col == row - 1) weight += 10;           // Главная диагональ
            if (col + row == boardSize) weight += 5;    // Побочная диагональ

            // Определяем цвет на основе веса
            PositionColor color;
            if (weight < 10) color = PositionColor::Red;
            else if (weight < 20) color = PositionColor::Green;
            else color = PositionColor::Blue;

            QString colorStr = (color == PositionColor::Red) ? "Red" :
                                   ((color == PositionColor::Green) ? "Green" : "Blue");

            insertPositionData(col, row, weight, colorStr);
            m_cache[makeKey(col, row)] = {weight, color};
        }
    }

    emit knowledgeUpdated();
}

// ============================================================================
// Анализ решений и статистика
// ============================================================================

/**
 *  Вычисляет суммарный вес решения
 *  solution Вектор позиций ферзей (индекс - столбец, значение - строка)
 * Возвращает сумма весов всех клеток с ферзями
 */
int KnowledgeBase::getTotalWeightForSolution(const std::vector<int>& solution) const
{
    int totalWeight = 0;
    for (size_t col = 0; col < solution.size(); ++col) {
        if (solution[col] != -1) {
            int row = solution[col] + 1;  // Преобразуем в 1-индексацию
            totalWeight += getPositionWeight(col, row);
        }
    }
    return totalWeight;
}

/**
 *  Возвращает распределение цветов в базе знаний
 * Возвращает QMap, где ключ - цвет, значение - количество клеток
 */
QMap<PositionColor, int> KnowledgeBase::getColorDistribution() const
{
    QMap<PositionColor, int> distribution;
    distribution[PositionColor::Red] = 0;
    distribution[PositionColor::Green] = 0;
    distribution[PositionColor::Blue] = 0;

    // Запрашиваем из БД количество клеток каждого цвета
    QSqlQuery query(m_database);
    query.prepare("SELECT color, COUNT(*) FROM position_knowledge "
                  "WHERE board_size = :size GROUP BY color");
    query.bindValue(":size", m_boardSize);

    if (query.exec()) {
        while (query.next()) {
            QString colorStr = query.value(0).toString();
            int count = query.value(1).toInt();

            if (colorStr == "Red") distribution[PositionColor::Red] = count;
            else if (colorStr == "Green") distribution[PositionColor::Green] = count;
            else if (colorStr == "Blue") distribution[PositionColor::Blue] = count;
        }
    }

    return distribution;
}

/**
 *  Возвращает средний вес по всем клеткам
 * Возвращает средний вес (целое число)
 */
int KnowledgeBase::getAverageWeight() const
{
    QSqlQuery query(m_database);
    query.prepare("SELECT AVG(weight) FROM position_knowledge WHERE board_size = :size");
    query.bindValue(":size", m_boardSize);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

/**
 *  Проверяет корректность базы знаний
 * Возвращает true, если для текущего размера доски есть все клетки
 *
 * Ожидаемое количество записей: boardSize * boardSize
 */
bool KnowledgeBase::isValid() const
{
    if (m_boardSize == 0) return false;

    QSqlQuery query(m_database);
    query.prepare("SELECT COUNT(*) FROM position_knowledge WHERE board_size = :size");
    query.bindValue(":size", m_boardSize);

    if (query.exec() && query.next()) {
        int expectedCount = m_boardSize * m_boardSize;
        int actualCount = query.value(0).toInt();
        return actualCount == expectedCount;
    }
    return false;
}

/**
 *  Очищает базу знаний и устанавливает новый размер доски
 *  boardSize Новый размер доски
 */
void KnowledgeBase::clearAndSetBoardSize(int boardSize)
{
    m_boardSize = boardSize;

    // Удаляем все данные для этого размера доски
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM position_knowledge WHERE board_size = :size");
    query.bindValue(":size", m_boardSize);
    query.exec();

    // Очищаем кэш
    m_cache.clear();
}

// В public секцию добавить:
/**
     *  Проверяет, совместимы ли цвета в решении
     *  solution Вектор позиций ферзей
     *  Возвращает true, если все ферзи одного цвета
     */
bool KnowledgeBase::validateColorCompatibility(const std::vector<int>& solution) const {
    if (solution.empty()) return true;

    int firstCol = -1;
    for (size_t i = 0; i < solution.size(); ++i) {
        if (solution[i] != -1) {
            firstCol = static_cast<int>(i);
            break;
        }
    }
    if (firstCol == -1) return true;

    PositionColor requiredColor = getPositionColor(firstCol, solution[firstCol] + 1);

    for (size_t col = 0; col < solution.size(); ++col) {
        if (solution[col] != -1) {
            PositionColor color = getPositionColor(static_cast<int>(col), solution[col] + 1);
            if (color != requiredColor) {
                return false;
            }
        }
    }
    return true;
}