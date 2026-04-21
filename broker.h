#ifndef BROKER_H
#define BROKER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QQueue>
#include <QTimer>
#include <vector>
#include <optional>
#include <memory>
#include <mutex>
#include <set>

class KnowledgeBase;
class PriorityManager;

/**
 * @brief Структура сообщения для обмена между брокером и агентами
 *
 * Формат сериализации:
 * - type (int): тип сообщения
 * - agentId (int): ID агента
 * - row (int): строка (0-индексация)
 * - priority (int): приоритет (1-10, 0=нет)
 * - timestamp (qint64): временная метка
 * - data (QByteArray): дополнительные данные
 *
 * @note Все поля сериализуются через QDataStream
 */
struct BrokerMessage {
    /**
     * @brief Типы сообщений протокола
     */
    enum Type {
        // Регистрация и управление
        REGISTER,           ///< Регистрация агента
        UNREGISTER,         ///< Отмена регистрации

        // Поиск позиций
        REQUEST_POSITION,   ///< Запрос на проверку позиции
        POSITION_ACCEPTED,  ///< Позиция принята
        POSITION_REJECTED,  ///< Позиция отклонена
        POSITION_CONFIRMED, ///< Позиция подтверждена
        BACKTRACK,          ///< Откат к предыдущему агенту

        // Решения
        SOLUTION_FOUND,     ///< Найдено решение

        // Состояние
        GET_STATE,          ///< Запрос состояния
        STATE_RESPONSE,     ///< Ответ с состоянием

        // Служебные
        HEARTBEAT,          ///< Проверка связи
        SHUTDOWN,           ///< Команда завершения

        // Фиксация позиций
        REQUEST_FIX,        ///< Запрос на фиксацию позиции
        REQUEST_UNFIX,      ///< Запрос на снятие фиксации
        FIX_ACCEPTED,       ///< Фиксация принята
        FIX_REJECTED,       ///< Фиксация отклонена
        FIX_REMOVED,        ///< Фиксация снята

        // Управление приоритетами
        REQUEST_PRIORITY,   ///< Запрос на изменение приоритета
        PRIORITY_ACCEPTED,  ///< Приоритет принят
        PRIORITY_REJECTED   ///< Приоритет отклонен
    };

    Type type;              ///< Тип сообщения
    int agentId;            ///< ID агента (столбец)
    int row;                ///< Строка (0-индексация)
    int priority;           ///< Приоритет (1-10, 0=нет)
    qint64 timestamp;       ///< Временная метка (мс)
    QByteArray data;        ///< Дополнительные данные

    /**
     * @brief Сериализация сообщения в байтовый массив
     * @return QByteArray с сериализованными данными
     *
     * Формат: [type][agentId][row][priority][timestamp][data]
     */
    QByteArray serialize() const {
        QByteArray serialized;
        QDataStream stream(&serialized, QIODevice::WriteOnly);
        stream << static_cast<int>(type) << agentId << row << priority << timestamp << data;
        return serialized;
    }

    /**
     * @brief Десериализация сообщения из байтового массива
     * @param rawData Сериализованные данные
     * @return BrokerMessage Восстановленное сообщение
     */
    static BrokerMessage deserialize(const QByteArray& rawData) {
        BrokerMessage msg;
        QDataStream stream(rawData);
        int typeInt;
        stream >> typeInt >> msg.agentId >> msg.row >> msg.priority >> msg.timestamp >> msg.data;
        msg.type = static_cast<Type>(typeInt);
        return msg;
    }
};

/**
 * @brief Структура информации об агенте
 *
 * Хранит все данные, связанные с подключенным агентом:
 * - Сетевое соединение
 * - Текущая позиция
 * - Статус фиксации
 * - Приоритет
 * - Состояние активности
 */
struct AgentInfo {
    int id;                     ///< ID агента (столбец)
    QTcpSocket* socket;         ///< TCP-сокет для связи
    int currentRow;             ///< Текущая строка (0-индексация)
    bool isFixed;               ///< Флаг фиксации позиции
    std::optional<int> fixedRow; ///< Зафиксированная строка
    int priority;               ///< Приоритет агента (1-10, 0=нет)
    qint64 lastHeartbeat;       ///< Время последнего heartbeat
    bool isActive;              ///< Активен ли агент

    AgentInfo() : id(-1), socket(nullptr), currentRow(-1),
        isFixed(false), fixedRow(std::nullopt),
        priority(0), lastHeartbeat(0), isActive(false) {}
};

/**
 * @brief Главный класс брокера - координатора распределенной системы
 *
 * Брокер выполняет следующие функции:
 * 1. Принимает TCP-подключения от агентов
 * 2. Управляет регистрацией и аутентификацией агентов
 * 3. Координирует распределенный поиск решений задачи о ферзях
 * 4. Обрабатывает запросы на фиксацию позиций
 * 5. Управляет приоритетами агентов
 * 6. Отслеживает heartbeat для обнаружения отключений
 *
 * Алгоритм поиска:
 * - Агенты перебирают строки в своих столбцах
 * - Брокер проверяет совместимость позиций
 * - При конфликте - отказ агенту
 * - При успехе - подтверждение и переход к следующему агенту
 * - При откате - возврат к предыдущему агенту
 *
 * @see AgentClient, PriorityManager, KnowledgeBase
 */
class Broker : public QObject
{
    Q_OBJECT

public:
    explicit Broker(QObject* parent = nullptr);
    ~Broker();

    // ========================================================================
    // Управление сервером
    // ========================================================================

    /**
     * @brief Запуск сервера брокера
     * @param port Порт для прослушивания
     * @param boardSize Размер доски (N x N)
     * @return true в случае успеха
     */
    bool start(int port, int boardSize);

    /**
     * @brief Остановка сервера
     *
     * Отправляет всем агентам команду SHUTDOWN и закрывает соединения.
     */
    void stop();

    /**
     * @brief Установка размера доски
     * @param size Новый размер (N x N)
     */
    void setBoardSize(int size);

    /**
     * @brief Получение текущего размера доски
     */
    int getBoardSize() const { return m_boardSize; }

    /**
     * @brief Проверка, запущен ли сервер
     */
    bool isRunning() const { return m_server != nullptr && m_server->isListening(); }

    /**
     * @brief Получение порта сервера
     * @return Номер порта или -1, если сервер не запущен
     */
    int getPort() const;

    // ========================================================================
    // Управление внешними компонентами
    // ========================================================================

    void setKnowledgeBase(KnowledgeBase* kb);
    void setPriorityManager(PriorityManager* manager);

    // ========================================================================
    // Управление фиксациями и приоритетами
    // ========================================================================

    /**
     * @brief Установка фиксированной позиции для столбца
     * @param column Номер столбца (0-индексация)
     * @param row Строка или nullopt для снятия
     */
    void setFixedPosition(int column, std::optional<int> row);

    /**
     * @brief Установка приоритета для столбца
     * @param column Номер столбца
     * @param priority Приоритет (1-10, 0=нет)
     */
    void setPriority(int column, int priority);

    /**
     * @brief Получение фиксированной позиции столбца
     * @param column Номер столбца
     * @return Строка или nullopt
     */
    std::optional<int> getFixedPosition(int column) const {
        if (column >= 0 && column < m_boardSize) {
            return m_fixedPositions[column];
        }
        return std::nullopt;
    }

    /**
     * @brief Получение приоритета столбца
     * @param column Номер столбца
     * @return Приоритет (0 если не задан)
     */
    int getPriority(int column) const {
        if (column >= 0 && column < m_boardSize) {
            return m_priorities[column];
        }
        return 0;
    }

    // ========================================================================
    // Управление поиском
    // ========================================================================

    /**
     * @brief Начало распределенного поиска решений
     * @return true если поиск успешно запущен
     *
     * Проверяет:
     * - Наличие всех необходимых агентов
     * - Активность всех агентов
     * - Совместимость фиксированных позиций
     */
    bool startSearch();

    /**
     * @brief Остановка текущего поиска
     */
    void stopSearch();

    /**
     * @brief Получение количества подключенных агентов
     */
    int getConnectedAgentsCount() const;

    /**
     * @brief Проверка, все ли агенты подключены
     */
    bool areAllAgentsConnected() const;

    /**
     * @brief Получение всех найденных решений
     */
    const std::vector<std::vector<int>>& getSolutions() const { return m_solutions; }

    /**
     * @brief Получение количества откатов (backtrack) при поиске
     */
    int getBacktrackCount() const { return m_backtrackCount; }

    /**
     * @brief Сбор текущего состояния всех агентов
     * @return Вектор позиций (индекс - столбец, значение - строка)
     */
    std::vector<int> collectCurrentState() const;

signals:
    // ========================================================================
    // Сигналы состояния
    // ========================================================================

    void agentConnected(int agentId);           ///< Агент подключился
    void agentDisconnected(int agentId);        ///< Агент отключился
    void agentPositionChanged(int agentId, int row); ///< Изменение позиции агента

    // ========================================================================
    // Сигналы поиска
    // ========================================================================

    /**
     * @brief Найдено новое решение
     * @param solution Вектор позиций ферзей
     */
    void solutionFound(const std::vector<int>& solution);

    /**
     * @brief Поиск завершен
     * @param totalSolutions Общее количество найденных решений
     */
    void searchFinished(int totalSolutions);

    /**
     * @brief Обновление прогресса поиска
     * @param solutionsFound Количество найденных решений
     * @param backtracks Количество выполненных откатов
     */
    void progressUpdated(int solutionsFound, int backtracks);

    // ========================================================================
    // Сигналы сообщений
    // ========================================================================

    void logMessage(const QString& message);    ///< Лог-сообщение
    void error(const QString& message);         ///< Сообщение об ошибке

private slots:
    // ========================================================================
    // Слоты сервера
    // ========================================================================

    void onNewConnection();                     ///< Новое подключение
    void onClientDisconnected();                ///< Отключение клиента
    void onReadyRead();                         ///< Получение данных
    void checkHeartbeats();                     ///< Проверка heartbeat

private:
    // ========================================================================
    // Обработка сообщений
    // ========================================================================

    /**
     * @brief Обработка сообщения от агента
     * @param socket Сокет агента
     * @param message Полученное сообщение
     */
    void processMessage(QTcpSocket* socket, const BrokerMessage& message);

    /**
     * @brief Отправка сообщения агенту
     * @param agentId ID агента
     * @param message Сообщение для отправки
     */
    void sendToAgent(int agentId, const BrokerMessage& message);

    /**
     * @brief Рассылка сообщения всем агентам
     * @param message Сообщение для рассылки
     */
    void broadcastMessage(const BrokerMessage& message);

    /**
     * @brief Логирование сообщения с префиксом "[Broker]"
     * @param msg Сообщение для логирования
     */
    void log(const QString& msg);

    // ========================================================================
    // Алгоритм поиска
    // ========================================================================

    /**
     * @brief Рекурсивный поиск решения
     * @param agentIndex Индекс текущего агента в упорядоченном списке
     *
     * Реализует поиск с возвратом (backtracking):
     * - Перебирает строки для текущего агента
     * - Проверяет конфликты с предыдущими
     * - Рекурсивно переходит к следующему агенту
     * - При нахождении полного решения - сохраняет его
     */
    void runSearchRecursive(int agentIndex);

    /**
     * @brief Проверка конфликта с предыдущими агентами
     * @param agentId ID текущего агента
     * @param row Предлагаемая строка
     * @param currentIndex Индекс в упорядоченном списке
     * @return true если есть конфликт
     */
    bool hasConflictWithPrevious(int agentId, int row, int currentIndex);

    /**
     * @brief Проверка совместимости цветов с предыдущими агентами
     * @param agentId ID текущего агента
     * @param row Предлагаемая строка
     * @param currentIndex Индекс в упорядоченном списке
     * @return true если цвета совместимы
     */
    bool isColorCompatibleWithPrevious(int agentId, int row, int currentIndex);

    /**
     * @brief Проверка валидности полного решения
     * @param state Вектор позиций ферзей
     * @return true если решение корректно
     */
    bool isValidSolution(const std::vector<int>& state);

    /**
     * @brief Получение ID агента по индексу в упорядоченном списке
     * @param index Индекс (0..boardSize-1)
     * @return ID агента
     *
     * Порядок определяется приоритетами:
     * 1. Фиксированные агенты (по убыванию приоритета)
     * 2. Агенты с приоритетом (по убыванию приоритета)
     * 3. Остальные агенты (по возрастанию ID)
     */
    int getOrderedAgent(int index);

    /**
     * @brief Завершение поиска и отправка результатов
     */
    void finishSearch();

private:
    // ========================================================================
    // Приватные поля
    // ========================================================================

    // Сервер и подключения
    QTcpServer* m_server;                       ///< TCP-сервер
    QMap<int, AgentInfo> m_agents;              ///< Информация об агентах (по ID)
    QMap<QTcpSocket*, int> m_socketToAgentId;   ///< Соответствие сокет -> ID агента

    // Конфигурация доски
    int m_boardSize;                            ///< Размер доски (N x N)
    std::vector<std::optional<int>> m_fixedPositions; ///< Фиксированные позиции
    std::vector<int> m_priorities;              ///< Приоритеты столбцов

    // Результаты поиска
    std::vector<std::vector<int>> m_solutions;  ///< Найденные решения
    std::set<std::vector<int>> m_uniqueSolutions; ///< Для избежания дубликатов

    // Внешние компоненты
    KnowledgeBase* m_knowledgeBase;             ///< База знаний
    PriorityManager* m_priorityManager;         ///< Менеджер приоритетов

    // Таймеры
    QTimer* m_heartbeatTimer;                   ///< Таймер для проверки heartbeat

    // Состояние поиска
    std::atomic<bool> m_searchActive;           ///< Активен ли поиск
    std::atomic<bool> m_stopRequested;          ///< Запрошена ли остановка

    // Статистика
    int m_backtrackCount;                       ///< Количество откатов

    // Потокобезопасность
    mutable std::mutex m_mutex;                 ///< Мьютекс для потокобезопасности

    // Константы
    static constexpr int HEARTBEAT_INTERVAL = 5000;   ///< Интервал heartbeat (мс)
    static constexpr int HEARTBEAT_TIMEOUT = 15000;   ///< Таймаут heartbeat (мс)
};

#endif // BROKER_H