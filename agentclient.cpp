#include "AgentClient.h"
#include <QDataStream>
#include <QHostAddress>
#include <QDateTime>
#include <QDebug>
#include <QRandomGenerator>

// ============================================================================
// Конструктор и деструктор
// ============================================================================

/**
 * @brief Конструктор агента
 * @param agentId Уникальный ID агента (0-9, соответствует столбцу)
 * @param parent Родительский объект
 *
 * Инициализирует агента в состоянии DISCONNECTED.
 * Создает таймеры для heartbeat и шагов поиска.
 */
AgentClient::AgentClient(int agentId, QObject* parent)
    : QObject(parent)
    , m_agentId(agentId)
    , m_socket(nullptr)
    , m_state(DISCONNECTED)
    , m_boardSize(8)
    , m_currentRow(-1)
    , m_isFixed(false)
    , m_priority(0)
    , m_searchActive(false)
{
    // Таймер для отправки heartbeat-сообщений
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(HEARTBEAT_INTERVAL);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &AgentClient::sendHeartbeat);

    // Таймер для выполнения шагов поиска
    // Интервал 50 мс предотвращает flooding и дает время на обработку
    m_searchTimer = new QTimer(this);
    m_searchTimer->setInterval(50);
    connect(m_searchTimer, &QTimer::timeout, this, &AgentClient::performSearchStep);
}

/**
 * @brief Деструктор
 *
 * Обеспечивает корректное отключение от брокера перед уничтожением.
 */
AgentClient::~AgentClient()
{
    disconnectFromBroker();
}

// ============================================================================
// Управление подключением
// ============================================================================

/**
 * @brief Подключение к брокеру
 * @param host Адрес брокера
 * @param port Порт брокера
 *
 * Алгоритм:
 * 1. Если уже есть соединение - закрываем его
 * 2. Создаем новый TCP-сокет
 * 3. Подключаем сигналы сокета
 * 4. Переходим в состояние CONNECTING
 * 5. Инициируем подключение
 */
void AgentClient::connectToBroker(const QString& host, int port)
{
    // Закрываем существующее соединение, если есть
    if (m_socket) {
        disconnectFromBroker();
    }

    // Создаем новый сокет
    m_socket = new QTcpSocket(this);

    // Подключаем сигналы для асинхронной обработки
    connect(m_socket, &QTcpSocket::connected, this, &AgentClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &AgentClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &AgentClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &AgentClient::onSocketError);

    setState(CONNECTING);
    log(QString("Подключение к брокеру %1:%2...").arg(host).arg(port));

    // Инициируем подключение (асинхронно)
    m_socket->connectToHost(host, port);
}

/**
 * @brief Отключение от брокера
 *
 * Выполняет корректное завершение соединения:
 * - Останавливает все таймеры
 * - Прекращает поиск
 * - Закрывает сокет
 * - Переходит в состояние DISCONNECTED
 */
void AgentClient::disconnectFromBroker()
{
    // Останавливаем таймеры
    m_heartbeatTimer->stop();
    m_searchTimer->stop();
    m_searchActive = false;

    // Закрываем сокет, если он существует
    if (m_socket) {
        m_socket->disconnectFromHost();
        // Ждем корректного завершения (не более 1 секунды)
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
        delete m_socket;
        m_socket = nullptr;
    }

    setState(DISCONNECTED);
}

// ============================================================================
// Управление поиском
// ============================================================================

/**
 * @brief Начало поиска решения
 *
 * Алгоритм поиска:
 * - Агент перебирает строки в своем столбце
 * - Для каждой позиции отправляет запрос брокеру
 * - Брокер проверяет совместимость с другими агентами
 * - При конфликте - переход к следующей строке
 * - При успехе - фиксация позиции и переход к следующему агенту
 * - При откате - возврат к предыдущим позициям
 */
void AgentClient::startSearch()
{
    // Проверяем, зарегистрирован ли агент
    if (m_state != REGISTERED) {
        emit error("Агент не зарегистрирован у брокера");
        return;
    }

    m_searchActive = true;
    m_triedRows.clear();  // Очищаем историю проверенных строк

    // Если позиция не фиксирована, начинаем с первой строки
    if (!m_isFixed) {
        m_currentRow = 0;
    }

    setState(SEARCHING);
    m_searchTimer->start();  // Запускаем таймер шагов поиска

    log("Начат поиск решения");
}

/**
 * @brief Остановка поиска
 *
 * Прекращает активный поиск и возвращает агента
 * в состояние REGISTERED (если был в SEARCHING или WAITING).
 */
void AgentClient::stopSearch()
{
    m_searchActive = false;
    m_searchTimer->stop();

    if (m_state == SEARCHING || m_state == WAITING) {
        setState(REGISTERED);
    }

    log("Поиск остановлен");
}

// ============================================================================
// Управление позицией
// ============================================================================

/**
 * @brief Локальная установка текущей строки
 * @param row Новая строка (0-индексация)
 *
 * Если агент фиксирован, изменение игнорируется.
 */
void AgentClient::setCurrentRow(int row)
{
    // Фиксированный агент не может менять позицию самостоятельно
    if (m_isFixed && m_fixedRow.has_value()) {
        return;
    }

    m_currentRow = row;
    emit positionChanged(row);
}

/**
 * @brief Запрос на фиксацию позиции
 * @param row Запрашиваемая строка
 *
 * Отправляет брокеру запрос на фиксацию.
 * Брокер проверяет:
 * - Не занята ли строка другим агентом
 * - Нет ли диагональных конфликтов
 *
 * Результат приходит через сигналы fixRequestAccepted/fixRequestRejected.
 */
void AgentClient::requestFixPosition(int row)
{
    // Проверяем состояние
    if (m_state != REGISTERED) {
        emit fixRequestRejected("Агент не зарегистрирован у брокера");
        return;
    }

    // Проверяем валидность строки
    if (row < 0 || row >= m_boardSize) {
        emit fixRequestRejected(QString("Неверная строка: %1 (допустимо 1-%2)")
                                    .arg(row + 1).arg(m_boardSize));
        return;
    }

    // Формируем и отправляем сообщение
    BrokerMessage msg;
    msg.type = BrokerMessage::REQUEST_FIX;
    msg.agentId = m_agentId;
    msg.row = row;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();

    sendMessage(msg);
    log(QString("Запрос на фиксацию строки %1 отправлен брокеру").arg(row + 1));
}

/**
 * @brief Запрос на снятие фиксации
 *
 * Отправляет брокеру запрос на освобождение текущей фиксации.
 */
void AgentClient::requestUnfixPosition()
{
    if (m_state != REGISTERED) {
        emit fixRequestRejected("Агент не зарегистрирован у брокера");
        return;
    }

    if (!m_isFixed) {
        emit fixRequestRejected("Позиция не зафиксирована");
        return;
    }

    BrokerMessage msg;
    msg.type = BrokerMessage::REQUEST_UNFIX;
    msg.agentId = m_agentId;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();

    sendMessage(msg);
    log("Запрос на снятие фиксации отправлен брокеру");
}

// ============================================================================
// Слоты сокета
// ============================================================================

/**
 * @brief Обработчик успешного подключения
 *
 * При подключении:
 * 1. Логируем событие
 * 2. Отправляем регистрацию
 * 3. Запускаем heartbeat-таймер
 */
void AgentClient::onConnected()
{
    log("Подключен к брокеру");
    emit connected();

    sendRegistration();      // Отправляем регистрацию
    m_heartbeatTimer->start(); // Запускаем heartbeat
}

/**
 * @brief Обработчик отключения
 *
 * При отключении:
 * 1. Останавливаем все таймеры
 * 2. Переходим в состояние DISCONNECTED
 * 3. Оповещаем UI
 */
void AgentClient::onDisconnected()
{
    log("Отключен от брокера");
    m_heartbeatTimer->stop();
    m_searchTimer->stop();
    m_searchActive = false;

    setState(DISCONNECTED);
    emit disconnected();
}

/**
 * @brief Обработчик получения данных от брокера
 *
 * Реализует протокол с префиксом размера сообщения:
 * [размер сообщения (int)] [данные сообщения]
 *
 * Буферизирует частично полученные данные и обрабатывает
 * полные сообщения по мере их поступления.
 */
void AgentClient::onReadyRead()
{
    if (!m_socket) return;

    // Читаем все доступные данные в буфер
    m_readBuffer.append(m_socket->readAll());

    // Обрабатываем все полные сообщения в буфере
    while (m_readBuffer.size() >= static_cast<int>(sizeof(int))) {
        QDataStream stream(m_readBuffer);
        int msgSize;
        stream >> msgSize;

        // Проверяем, есть ли полное сообщение в буфере
        if (m_readBuffer.size() < msgSize + static_cast<int>(sizeof(int))) {
            break;  // Ждем остальных данных
        }

        // Извлекаем сообщение из буфера
        QByteArray msgData = m_readBuffer.mid(sizeof(int), msgSize);
        m_readBuffer.remove(0, msgSize + sizeof(int));

        // Десериализуем и обрабатываем
        BrokerMessage msg = BrokerMessage::deserialize(msgData);
        processMessage(msg);
    }
}

/**
 * @brief Обработчик ошибок сокета
 * @param socketError Код ошибки
 *
 * Логирует ошибку и отправляет сигнал для UI.
 */
void AgentClient::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    QString errorMsg = m_socket ? m_socket->errorString() : "Неизвестная ошибка";
    log(QString("Ошибка сокета: %1").arg(errorMsg));
    emit error(errorMsg);
}

// ============================================================================
// Обработка сообщений от брокера
// ============================================================================

/**
 * @brief Обработка входящих сообщений от брокера
 * @param message Полученное сообщение
 *
 * Обрабатывает все типы сообщений, которые брокер может отправить агенту.
 *
 * Типы сообщений:
 * - POSITION_ACCEPTED: Регистрация принята, получена конфигурация
 * - POSITION_REJECTED: Предложенная позиция отклонена
 * - POSITION_CONFIRMED: Позиция подтверждена брокером
 * - FIX_ACCEPTED: Запрос фиксации принят
 * - FIX_REJECTED: Запрос фиксации отклонен
 * - FIX_REMOVED: Фиксация снята
 * - REQUEST_POSITION: Запрос следующей позиции от брокера
 * - SOLUTION_FOUND: Решение найдено
 * - HEARTBEAT: Проверка связи
 * - SHUTDOWN: Команда завершения
 * - PRIORITY_ACCEPTED: Приоритет установлен
 * - PRIORITY_REJECTED: Приоритет отклонен
 */
void AgentClient::processMessage(const BrokerMessage& message)
{
    switch (message.type) {
    // ------------------------------------------------------------------------
    // POSITION_ACCEPTED - регистрация принята
    // ------------------------------------------------------------------------
    case BrokerMessage::POSITION_ACCEPTED: {
        if (message.agentId == m_agentId) {
            if (!message.data.isEmpty()) {
                QDataStream stream(message.data);
                stream >> m_boardSize >> m_isFixed >> m_priority;

                if (message.row >= 0) {
                    m_fixedRow = message.row;
                    m_currentRow = message.row;
                    m_isFixed = true;
                }

                emit configReceived(m_boardSize, m_isFixed, m_priority);
            }

            setState(REGISTERED);
            log(QString("Зарегистрирован у брокера. Доска: %1x%1, Фиксирован: %2")
                    .arg(m_boardSize).arg(m_isFixed ? "да" : "нет"));
        }
        break;
    }

    // ------------------------------------------------------------------------
    // POSITION_REJECTED - позиция отклонена, пробуем следующую
    // ------------------------------------------------------------------------
    case BrokerMessage::POSITION_REJECTED: {
        if (message.agentId == m_agentId) {
            if (m_searchActive && !m_isFixed) {
                m_triedRows.push_back(m_currentRow);  // Запоминаем неудачную строку
                requestNextPosition();  // Переходим к следующей
            }
        }
        break;
    }

    // ------------------------------------------------------------------------
    // POSITION_CONFIRMED - позиция подтверждена
    // ------------------------------------------------------------------------
    case BrokerMessage::POSITION_CONFIRMED: {
        if (message.agentId == m_agentId) {
            m_currentRow = message.row;
            emit positionChanged(m_currentRow);
        }
        break;
    }

    // ------------------------------------------------------------------------
    // FIX_ACCEPTED - фиксация принята
    // ------------------------------------------------------------------------
    case BrokerMessage::FIX_ACCEPTED: {
        if (message.agentId == m_agentId) {
            m_isFixed = true;
            m_fixedRow = message.row;
            m_currentRow = message.row;
            emit fixRequestAccepted(message.row);
            log(QString("✅ Фиксация принята! Строка %1 зафиксирована").arg(message.row + 1));
            emit configReceived(m_boardSize, true, m_priority);
        }
        break;
    }

    // ------------------------------------------------------------------------
    // FIX_REJECTED - фиксация отклонена
    // ------------------------------------------------------------------------
    case BrokerMessage::FIX_REJECTED: {
        if (message.agentId == m_agentId) {
            QString reason = QString::fromUtf8(message.data);
            emit fixRequestRejected(reason);
            log(QString("❌ Фиксация отклонена: %1").arg(reason));
        }
        break;
    }

    // ------------------------------------------------------------------------
    // FIX_REMOVED - фиксация снята
    // ------------------------------------------------------------------------
    case BrokerMessage::FIX_REMOVED: {
        if (message.agentId == m_agentId) {
            m_isFixed = false;
            m_fixedRow = std::nullopt;
            // ВАЖНО: НЕ сбрасываем m_currentRow - оставляем текущую позицию
            // Это позволяет агенту продолжить с текущей позиции после снятия фиксации

            log(QString("🔓 Фиксация снята со столбца %1").arg(QChar('A' + m_agentId)));

            // Отправляем обновленную конфигурацию
            emit configReceived(m_boardSize, false, m_priority);

            // Дополнительно сообщаем, что фиксация снята (-1 означает снятие)
            emit fixRequestAccepted(-1);
        }
        break;
    }

    // ------------------------------------------------------------------------
    // REQUEST_POSITION - запрос следующей позиции от брокера
    // ------------------------------------------------------------------------
    case BrokerMessage::REQUEST_POSITION: {
        if (m_state == SEARCHING) {
            performSearchStep();
        }
        break;
    }

    // ------------------------------------------------------------------------
    // SOLUTION_FOUND - решение найдено
    // ------------------------------------------------------------------------
    case BrokerMessage::SOLUTION_FOUND: {
        log("Решение найдено!");
        emit solutionFound();
        break;
    }

    // ------------------------------------------------------------------------
    // HEARTBEAT - проверка связи
    // ------------------------------------------------------------------------
    case BrokerMessage::HEARTBEAT: {
        // Отвечаем на heartbeat
        BrokerMessage response;
        response.type = BrokerMessage::HEARTBEAT;
        response.agentId = m_agentId;
        response.timestamp = QDateTime::currentMSecsSinceEpoch();
        sendMessage(response);
        break;
    }

    // ------------------------------------------------------------------------
    // SHUTDOWN - команда завершения от брокера
    // ------------------------------------------------------------------------
    case BrokerMessage::SHUTDOWN: {
        log("Получена команда завершения от брокера");
        disconnectFromBroker();
        break;
    }

    // ------------------------------------------------------------------------
    // PRIORITY_ACCEPTED - приоритет установлен
    // ------------------------------------------------------------------------
    case BrokerMessage::PRIORITY_ACCEPTED: {
        if (message.agentId == m_agentId) {
            m_priority = message.priority;
            emit priorityChanged(m_priority);
            log(QString("✅ Приоритет установлен: %1 %2")
                    .arg(m_priority)
                    .arg(m_priority == 1 ? "(наивысший)" :
                             m_priority == 10 ? "(низший)" : ""));
            emit configReceived(m_boardSize, m_isFixed, m_priority);
        }
        break;
    }

    // ------------------------------------------------------------------------
    // PRIORITY_REJECTED - приоритет отклонен
    // ------------------------------------------------------------------------
    case BrokerMessage::PRIORITY_REJECTED: {
        if (message.agentId == m_agentId) {
            QString reason = QString::fromUtf8(message.data);
            log(QString("❌ Установка приоритета отклонена: %1").arg(reason));
            emit error(reason);
        }
        break;
    }

    default:
        break;
    }
}

// ============================================================================
// Отправка сообщений
// ============================================================================

/**
 * @brief Отправка сообщения брокеру
 * @param message Сообщение для отправки
 *
 * Формат пакета:
 * [размер сообщения (int)] [сериализованное сообщение]
 */
void AgentClient::sendMessage(const BrokerMessage& message)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QByteArray data = message.serialize();
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream << data.size();  // Префикс размера
    packet.append(data);    // Данные

    m_socket->write(packet);
    m_socket->flush();
}

/**
 * @brief Отправка регистрационного сообщения
 *
 * Отправляется сразу после установки TCP-соединения.
 * Содержит ID агента и его текущий приоритет.
 */
void AgentClient::sendRegistration()
{
    BrokerMessage msg;
    msg.type = BrokerMessage::REGISTER;
    msg.agentId = m_agentId;
    msg.priority = m_priority;  // Отправляем текущий приоритет
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();
    sendMessage(msg);
}

/**
 * @brief Отправка heartbeat-сообщения
 *
 * Регулярно отправляется для поддержания соединения.
 * Брокер ожидает heartbeat в течение таймаута.
 */
void AgentClient::sendHeartbeat()
{
    if (m_state >= REGISTERED) {
        BrokerMessage msg;
        msg.type = BrokerMessage::HEARTBEAT;
        msg.agentId = m_agentId;
        msg.timestamp = QDateTime::currentMSecsSinceEpoch();
        sendMessage(msg);
    }
}

// ============================================================================
// Логика поиска
// ============================================================================

/**
 * @brief Выполнение шага поиска
 *
 * Отправляет брокеру запрос на проверку текущей позиции.
 * Переходит в состояние WAITING и ожидает ответа.
 * Через 10 мс возвращается в SEARCHING, если ответ не получен.
 */
void AgentClient::performSearchStep()
{
    if (!m_searchActive || m_state != SEARCHING) {
        return;
    }

    if (m_isFixed) {
        return;  // Фиксированный агент не участвует в поиске
    }

    setState(WAITING);

    BrokerMessage msg;
    msg.type = BrokerMessage::REQUEST_POSITION;
    msg.agentId = m_agentId;
    msg.row = m_currentRow;
    sendMessage(msg);

    // Таймаут для возврата в состояние поиска
    QTimer::singleShot(10, this, [this]() {
        if (m_state == WAITING) {
            setState(SEARCHING);
        }
    });
}

/**
 * @brief Запрос следующей позиции при отказе
 *
 * При получении отказа от брокера:
 * 1. Переходит к следующей непроверенной строке
 * 2. Если все строки проверены - выполняет откат (backtrack)
 *
 * Откат означает, что все позиции в текущем столбце не подходят,
 * нужно вернуться к предыдущему агенту.
 */
void AgentClient::requestNextPosition()
{
    if (m_isFixed) return;

    // Ищем следующую непроверенную строку
    int nextRow = m_currentRow + 1;
    while (nextRow < m_boardSize) {
        if (std::find(m_triedRows.begin(), m_triedRows.end(), nextRow) == m_triedRows.end()) {
            break;
        }
        nextRow++;
    }

    if (nextRow < m_boardSize) {
        // Есть следующая строка - переходим к ней
        m_currentRow = nextRow;
        emit positionChanged(m_currentRow);
        setState(SEARCHING);
    } else {
        // Все строки проверены - откат к предыдущему агенту
        BrokerMessage msg;
        msg.type = BrokerMessage::BACKTRACK;
        msg.agentId = m_agentId;
        sendMessage(msg);

        m_currentRow = -1;
        emit positionChanged(-1);

        log("Откат - все позиции проверены");
    }
}

// ============================================================================
// Вспомогательные методы
// ============================================================================

/**
 * @brief Проверка, является ли текущая позиция решением
 * @return true если ферзь размещен и агент зарегистрирован
 */
bool AgentClient::isSolution() const
{
    return m_currentRow >= 0 && m_state == REGISTERED;
}

/**
 * @brief Установка состояния с отправкой сигнала
 * @param newState Новое состояние
 *
 * Отправляет сигнал только при реальном изменении состояния.
 */
void AgentClient::setState(State newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(newState);
    }
}

/**
 * @brief Логирование сообщения
 * @param msg Сообщение для логирования
 *
 * Добавляет префикс с ID агента (A, B, C, ...).
 */
void AgentClient::log(const QString& msg)
{
    QString fullMsg = QString("[Агент %1] %2").arg(QChar('A' + m_agentId)).arg(msg);
    qDebug() << fullMsg;
    emit logMessage(fullMsg);
}

/**
 * @brief Запрос на установку приоритета
 * @param priority Новый приоритет (0-10, 1 - наивысший)
 *
 * Отправляет брокеру запрос на изменение приоритета агента.
 */
void AgentClient::requestSetPriority(int priority)
{
    if (m_state != REGISTERED) {
        emit error("Агент не зарегистрирован у брокера");
        return;
    }

    if (priority < 0 || priority > 10) {
        emit error("Приоритет должен быть от 0 до 10");
        return;
    }

    BrokerMessage msg;
    msg.type = BrokerMessage::REQUEST_PRIORITY;
    msg.agentId = m_agentId;
    msg.priority = priority;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();

    sendMessage(msg);
    log(QString("Запрос на установку приоритета %1 отправлен брокеру").arg(priority));
}