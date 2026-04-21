#include "Broker.h"
#include "KnowledgeBase.h"
#include "PriorityManager.h"
#include <QDataStream>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QDateTime>
#include <QDebug>
#include <algorithm>
#include <cmath>

Broker::Broker(QObject* parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_boardSize(8)
    , m_knowledgeBase(nullptr)
    , m_priorityManager(nullptr)
    , m_searchActive(false)
    , m_stopRequested(false)
    , m_backtrackCount(0)
{
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(HEARTBEAT_INTERVAL);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &Broker::checkHeartbeats);

    m_priorities.resize(m_boardSize, 0);
    m_fixedPositions.resize(m_boardSize, std::nullopt);
}

Broker::~Broker()
{
    stop();
}

void Broker::setKnowledgeBase(KnowledgeBase* kb)
{
    m_knowledgeBase = kb;
}

void Broker::setPriorityManager(PriorityManager* manager)
{
    m_priorityManager = manager;

    if (manager) {
        for (int col = 0; col < m_boardSize; ++col) {
            m_priorities[col] = manager->getPriority(col);
            m_fixedPositions[col] = manager->getFixedPosition(col);
        }
    }
}

bool Broker::start(int port, int boardSize)
{
    if (m_server) {
        stop();
    }

    m_boardSize = boardSize;
    m_priorities.resize(m_boardSize, 0);
    m_fixedPositions.resize(m_boardSize, std::nullopt);

    m_server = new QTcpServer(this);

    if (!m_server->listen(QHostAddress::Any, port)) {
        emit error(QString("Не удалось запустить сервер: %1").arg(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        return false;
    }

    connect(m_server, &QTcpServer::newConnection, this, &Broker::onNewConnection);

    m_heartbeatTimer->start();

    log(QString("Брокер запущен на порту %1, размер доски %2x%2")
            .arg(port).arg(m_boardSize));
    emit logMessage(QString("Брокер запущен на порту %1").arg(port));

    return true;
}

void Broker::stop()
{
    m_stopRequested = true;
    m_searchActive = false;

    m_heartbeatTimer->stop();

    BrokerMessage shutdownMsg;
    shutdownMsg.type = BrokerMessage::SHUTDOWN;
    shutdownMsg.timestamp = QDateTime::currentMSecsSinceEpoch();
    broadcastMessage(shutdownMsg);

    for (auto& info : m_agents) {
        if (info.socket) {
            info.socket->disconnectFromHost();
        }
    }

    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }

    m_agents.clear();
    m_socketToAgentId.clear();

    log("Брокер остановлен");
    emit logMessage("Брокер остановлен");
}

int Broker::getPort() const
{
    if (m_server && m_server->isListening()) {
        return m_server->serverPort();
    }
    return -1;
}

void Broker::setBoardSize(int size)
{
    m_boardSize = size;
    m_priorities.resize(m_boardSize, 0);
    m_fixedPositions.resize(m_boardSize, std::nullopt);
}

void Broker::setFixedPosition(int column, std::optional<int> row)
{
    if (column >= 0 && column < m_boardSize) {
        m_fixedPositions[column] = row;

        if (m_agents.contains(column)) {
            m_agents[column].isFixed = row.has_value();
            m_agents[column].fixedRow = row;
            if (row.has_value()) {
                m_agents[column].currentRow = row.value();
            }
            // Если фиксация снята, НЕ сбрасываем currentRow
            // Оставляем текущую позицию как есть

            BrokerMessage msg;
            if (row.has_value()) {
                msg.type = BrokerMessage::FIX_ACCEPTED;
                msg.row = row.value();
            } else {
                msg.type = BrokerMessage::FIX_REMOVED;
                msg.row = -1;
            }
            msg.agentId = column;
            sendToAgent(column, msg);
        }

        log(QString("Фиксированная позиция для столбца %1: %2")
                .arg(QChar('A' + column))
                .arg(row.has_value() ? QString::number(row.value() + 1) : "снята"));
    }
}

void Broker::setPriority(int column, int priority)
{
    if (column >= 0 && column < m_boardSize) {
        m_priorities[column] = priority;

        if (m_agents.contains(column)) {
            m_agents[column].priority = priority;
        }

        log(QString("Приоритет столбца %1: %2")
                .arg(QChar('A' + column)).arg(priority));
    }
}

int Broker::getConnectedAgentsCount() const
{
    int count = 0;
    for (auto it = m_agents.begin(); it != m_agents.end(); ++it) {
        if (it->isActive && it->socket &&
            it->socket->state() == QAbstractSocket::ConnectedState) {
            count++;
        }
    }
    return count;
}

bool Broker::areAllAgentsConnected() const
{
    return getConnectedAgentsCount() >= m_boardSize;
}

// ============================================================================
// НОВЫЙ АЛГОРИТМ ПОИСКА (БЕЗ ТАЙМЕРА, КАК В MAINWINDOW)
// ============================================================================

bool Broker::startSearch()
{
    if (m_searchActive) {
        log("Поиск уже активен");
        return false;
    }

    // Проверяем подключение агентов
    int connectedCount = getConnectedAgentsCount();
    log(QString("Подключено агентов: %1 из %2").arg(connectedCount).arg(m_boardSize));

    if (connectedCount < m_boardSize) {
        log(QString("ОШИБКА: Недостаточно агентов. Подключено %1 из %2")
                .arg(connectedCount).arg(m_boardSize));
        emit error(QString("Недостаточно агентов. Подключено %1 из %2")
                       .arg(connectedCount).arg(m_boardSize));
        return false;
    }

    // Проверяем, что все агенты зарегистрированы
    for (int i = 0; i < m_boardSize; ++i) {
        if (!m_agents.contains(i)) {
            log(QString("ОШИБКА: Агент %1 не зарегистрирован").arg(i));
            emit error(QString("Агент %1 не зарегистрирован").arg(QChar('A' + i)));
            return false;
        }
        if (!m_agents[i].isActive) {
            log(QString("ОШИБКА: Агент %1 неактивен").arg(i));
            emit error(QString("Агент %1 неактивен").arg(QChar('A' + i)));
            return false;
        }
    }

    // Очищаем предыдущие решения
    m_solutions.clear();
    m_uniqueSolutions.clear();
    m_backtrackCount = 0;

    // Устанавливаем начальные позиции
    for (auto it = m_agents.begin(); it != m_agents.end(); ++it) {
        if (it->isFixed && it->fixedRow.has_value()) {
            it->currentRow = it->fixedRow.value();
        } else {
            it->currentRow = 0;  // Начинаем с первой строки
        }
    }

    m_searchActive = true;
    m_stopRequested = false;

    log("Распределенный поиск начат");
    emit logMessage("Распределенный поиск начат");

    // Запускаем поиск в отдельном потоке, чтобы не блокировать UI
    QTimer::singleShot(0, this, [this]() {
        runSearchRecursive(0);
        finishSearch();
    });

    return true;
}

void Broker::runSearchRecursive(int agentIndex)
{
    if (m_stopRequested || !m_searchActive) {
        return;
    }

    if (agentIndex >= m_boardSize) {
        std::vector<int> state = collectCurrentState();

        // Проверяем, что все позиции установлены
        for (int row : state) {
            if (row < 0) return;
        }

        if (isValidSolution(state)) {
            bool colorsOk = true;
            if (m_knowledgeBase) {
                colorsOk = m_knowledgeBase->validateColorCompatibility(state);
            }

            if (colorsOk) {
                auto it = m_uniqueSolutions.find(state);
                if (it == m_uniqueSolutions.end()) {
                    m_uniqueSolutions.insert(state);
                    m_solutions.push_back(state);

                    // ВАЖНО: отправляем сигнал для КАЖДОГО решения
                    emit progressUpdated((int)m_solutions.size(), m_backtrackCount);
                    emit solutionFound(state);  // <-- ДОЛЖНО БЫТЬ

                    if (m_solutions.size() == 1) {
                        log(QString("=== ПЕРВОЕ РЕШЕНИЕ НАЙДЕНО ==="));
                    }
                }
            }
        }
        return;
    }

    // Получаем текущего агента с учетом приоритетов
    int currentAgentId = getOrderedAgent(agentIndex);

    // Если агент фиксирован - пропускаем
    if (m_agents[currentAgentId].isFixed) {
        runSearchRecursive(agentIndex + 1);
        return;
    }

    // Перебираем строки для текущего агента
    for (int row = 0; row < m_boardSize && !m_stopRequested; ++row) {
        // Проверяем конфликты с предыдущими агентами
        if (!hasConflictWithPrevious(currentAgentId, row, agentIndex)) {
            // Проверяем совместимость цветов
            bool colorsOk = true;
            if (m_knowledgeBase) {
                colorsOk = isColorCompatibleWithPrevious(currentAgentId, row, agentIndex);
            }

            if (colorsOk) {
                // Устанавливаем позицию
                m_agents[currentAgentId].currentRow = row;

                // Рекурсивно идем дальше
                runSearchRecursive(agentIndex + 1);

                // Сбрасываем позицию для следующей итерации
                m_agents[currentAgentId].currentRow = -1;
            }
        }

        m_backtrackCount++;
        // Обновляем прогресс при откатах (каждые 1000 откатов для производительности)
        if (m_backtrackCount % 1000 == 0) {
            emit progressUpdated((int)m_solutions.size(), m_backtrackCount);
        }
    }
}

bool Broker::hasConflictWithPrevious(int agentId, int row, int currentIndex)
{
    // Проверяем конфликты только с уже установленными агентами
    for (int i = 0; i < currentIndex; ++i) {
        int otherId = getOrderedAgent(i);
        int otherRow = m_agents[otherId].currentRow;

        if (otherRow < 0) continue;

        // Проверка горизонтали
        if (otherRow == row) return true;

        // Проверка диагонали
        if (std::abs(otherRow - row) == std::abs(otherId - agentId)) return true;
    }

    return false;
}

bool Broker::isColorCompatibleWithPrevious(int agentId, int row, int currentIndex)
{
    if (!m_knowledgeBase) return true;

    PositionColor currentColor = m_knowledgeBase->getPositionColor(agentId, row + 1);

    // Проверяем цвет с уже установленными агентами
    for (int i = 0; i < currentIndex; ++i) {
        int otherId = getOrderedAgent(i);
        int otherRow = m_agents[otherId].currentRow;

        if (otherRow < 0) continue;

        PositionColor otherColor = m_knowledgeBase->getPositionColor(otherId, otherRow + 1);
        if (currentColor != otherColor) return false;
    }

    return true;
}

bool Broker::isValidSolution(const std::vector<int>& state)
{
    for (int i = 0; i < m_boardSize; ++i) {
        for (int j = i + 1; j < m_boardSize; ++j) {
            if (state[i] == state[j]) return false;
            if (std::abs(state[i] - state[j]) == std::abs(i - j)) return false;
        }
    }
    return true;
}

int Broker::getOrderedAgent(int index)
{
    // Собираем порядок агентов с учетом приоритетов
    std::vector<int> orderedAgents;
    for (int i = 0; i < m_boardSize; ++i) {
        orderedAgents.push_back(i);
    }

    // Сортируем: сначала фиксированные, потом по приоритету
    std::sort(orderedAgents.begin(), orderedAgents.end(), [this](int a, int b) {
        bool fixedA = m_agents[a].isFixed;
        bool fixedB = m_agents[b].isFixed;

        if (fixedA != fixedB) return fixedA > fixedB;

        int prioA = m_priorities[a];
        int prioB = m_priorities[b];

        if (prioA > 0 && prioB > 0) return prioA < prioB;
        if (prioA > 0) return true;
        if (prioB > 0) return false;

        return a < b;
    });

    return orderedAgents[index];
}

void Broker::stopSearch()
{
    m_stopRequested = true;
    m_searchActive = false;
}

void Broker::finishSearch()
{
    m_searchActive = false;

    if (m_solutions.empty()) {
        log("=== ПОИСК ЗАВЕРШЕН: РЕШЕНИЙ НЕ НАЙДЕНО ===");
    } else {
        log(QString("=== ПОИСК ЗАВЕРШЕН: НАЙДЕНО %1 РЕШЕНИЙ ===").arg(m_solutions.size()));
        log(QString("Всего откатов: %1").arg(m_backtrackCount));
    }

    emit logMessage(QString("Поиск завершен. Найдено решений: %1").arg(m_solutions.size()));
    emit searchFinished(m_solutions.size());
}

// ============================================================================
// ОСТАЛЬНЫЕ МЕТОДЫ (без изменений)
// ============================================================================

void Broker::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();

        connect(socket, &QTcpSocket::disconnected, this, &Broker::onClientDisconnected);
        connect(socket, &QTcpSocket::readyRead, this, &Broker::onReadyRead);

        log(QString("Новое подключение от %1").arg(socket->peerAddress().toString()));
    }
}

void Broker::onClientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    int agentId = m_socketToAgentId.value(socket, -1);
    if (agentId >= 0) {
        log(QString("Агент %1 отключился").arg(QChar('A' + agentId)));
        emit agentDisconnected(agentId);
        emit logMessage(QString("Агент %1 отключился").arg(QChar('A' + agentId)));

        if (m_agents.contains(agentId)) {
            m_agents[agentId].isActive = false;
            m_agents[agentId].socket = nullptr;
        }
        m_socketToAgentId.remove(socket);
    }

    socket->deleteLater();
}

void Broker::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();

    while (data.size() >= static_cast<int>(sizeof(int))) {
        QDataStream stream(data);
        int msgSize;
        stream >> msgSize;

        if (data.size() < msgSize + static_cast<int>(sizeof(int))) {
            break;
        }

        QByteArray msgData = data.mid(sizeof(int), msgSize);
        data.remove(0, msgSize + sizeof(int));

        BrokerMessage msg = BrokerMessage::deserialize(msgData);
        processMessage(socket, msg);
    }
}

void Broker::checkHeartbeats()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (auto it = m_agents.begin(); it != m_agents.end(); ++it) {
        if (it->isActive && it->socket) {
            if (now - it->lastHeartbeat > HEARTBEAT_TIMEOUT) {
                log(QString("Таймаут heartbeat для агента %1").arg(QChar('A' + it->id)));
                it->isActive = false;
                emit agentDisconnected(it->id);
            } else {
                BrokerMessage msg;
                msg.type = BrokerMessage::HEARTBEAT;
                msg.timestamp = now;
                sendToAgent(it->id, msg);
            }
        }
    }
}

void Broker::processMessage(QTcpSocket* socket, const BrokerMessage& message)
{
    switch (message.type) {
    case BrokerMessage::REGISTER: {
        int agentId = message.agentId;

        if (agentId < 0 || agentId >= m_boardSize) {
            log(QString("Попытка регистрации с неверным ID: %1").arg(agentId));
            return;
        }

        if (m_agents.contains(agentId) && m_agents[agentId].isActive) {
            return;
        }

        AgentInfo info;
        info.id = agentId;
        info.socket = socket;
        info.currentRow = -1;
        info.isFixed = m_fixedPositions[agentId].has_value();
        info.fixedRow = m_fixedPositions[agentId];
        info.priority = m_priorities[agentId];
        info.lastHeartbeat = QDateTime::currentMSecsSinceEpoch();
        info.isActive = true;

        if (message.priority > 0) {
            m_priorities[agentId] = message.priority;
            info.priority = message.priority;
        }

        if (info.isFixed && info.fixedRow.has_value()) {
            info.currentRow = info.fixedRow.value();
        }

        m_agents[agentId] = info;
        m_socketToAgentId[socket] = agentId;

        BrokerMessage response;
        response.type = BrokerMessage::POSITION_ACCEPTED;
        response.agentId = agentId;
        response.row = info.fixedRow.value_or(-1);

        QDataStream stream(&response.data, QIODevice::WriteOnly);
        stream << m_boardSize << info.isFixed << info.priority;

        sendToAgent(agentId, response);

        log(QString("Агент %1 зарегистрирован").arg(QChar('A' + agentId)));
        emit agentConnected(agentId);
        emit logMessage(QString("Агент %1 подключился").arg(QChar('A' + agentId)));
        break;
    }

    case BrokerMessage::HEARTBEAT: {
        int agentId = message.agentId;
        if (m_agents.contains(agentId)) {
            m_agents[agentId].lastHeartbeat = QDateTime::currentMSecsSinceEpoch();
        }
        break;
    }

    case BrokerMessage::REQUEST_FIX: {
        int agentId = message.agentId;
        int requestedRow = message.row;

        if (!m_agents.contains(agentId)) {
            return;
        }

        // Проверяем, не занята ли строка другим агентом
        bool rowBusy = false;
        int conflictingAgent = -1;
        for (auto it = m_agents.begin(); it != m_agents.end(); ++it) {
            if (it.key() != agentId && it->currentRow == requestedRow && it->currentRow >= 0) {
                rowBusy = true;
                conflictingAgent = it.key();
                break;
            }
        }

        if (rowBusy) {
            BrokerMessage response;
            response.type = BrokerMessage::FIX_REJECTED;
            response.agentId = agentId;
            response.data = QString("Строка %1 уже занята агентом %2")
                                .arg(requestedRow + 1)
                                .arg(QChar('A' + conflictingAgent))
                                .toUtf8();
            sendToAgent(agentId, response);
            return;
        }

        // Проверяем диагональные конфликты
        bool diagonalConflict = false;
        for (auto it = m_agents.begin(); it != m_agents.end(); ++it) {
            if (it.key() != agentId && it->currentRow >= 0) {
                if (std::abs(it->currentRow - requestedRow) == std::abs(it.key() - agentId)) {
                    diagonalConflict = true;
                    conflictingAgent = it.key();
                    break;
                }
            }
        }

        if (diagonalConflict) {
            BrokerMessage response;
            response.type = BrokerMessage::FIX_REJECTED;
            response.agentId = agentId;
            response.data = QString("Строка %1 конфликтует по диагонали с агентом %2")
                                .arg(requestedRow + 1)
                                .arg(QChar('A' + conflictingAgent))
                                .toUtf8();
            sendToAgent(agentId, response);
            return;
        }

        // Фиксируем позицию
        m_fixedPositions[agentId] = requestedRow;
        m_agents[agentId].isFixed = true;
        m_agents[agentId].fixedRow = requestedRow;
        m_agents[agentId].currentRow = requestedRow;

        BrokerMessage response;
        response.type = BrokerMessage::FIX_ACCEPTED;
        response.agentId = agentId;
        response.row = requestedRow;
        sendToAgent(agentId, response);

        log(QString("Агент %1 зафиксировал позицию %2%3")
                .arg(QChar('A' + agentId))
                .arg(QChar('A' + agentId))
                .arg(requestedRow + 1));
        break;
    }

    case BrokerMessage::REQUEST_UNFIX: {
        int agentId = message.agentId;

        if (!m_agents.contains(agentId)) {
            return;
        }

        if (!m_agents[agentId].isFixed) {
            BrokerMessage response;
            response.type = BrokerMessage::FIX_REJECTED;
            response.agentId = agentId;
            response.data = QString("Позиция не была зафиксирована").toUtf8();
            sendToAgent(agentId, response);
            return;
        }

        // Снимаем фиксацию
        m_fixedPositions[agentId] = std::nullopt;
        m_agents[agentId].isFixed = false;
        m_agents[agentId].fixedRow = std::nullopt;
        // НЕ сбрасываем currentRow

        BrokerMessage response;
        response.type = BrokerMessage::FIX_REMOVED;
        response.agentId = agentId;
        sendToAgent(agentId, response);

        log(QString("Агент %1 снял фиксацию со столбца %2")
                .arg(QChar('A' + agentId))
                .arg(QChar('A' + agentId)));
        break;
    }

    case BrokerMessage::REQUEST_PRIORITY: {
        int agentId = message.agentId;
        int requestedPriority = message.priority;

        if (!m_agents.contains(agentId)) {
            return;
        }

        if (requestedPriority < 0 || requestedPriority > 10) {
            BrokerMessage response;
            response.type = BrokerMessage::PRIORITY_REJECTED;
            response.agentId = agentId;
            response.data = QString("Неверный приоритет: %1 (допустимо 0-10)")
                                .arg(requestedPriority).toUtf8();
            sendToAgent(agentId, response);
            return;
        }

        // Устанавливаем приоритет
        m_priorities[agentId] = requestedPriority;
        m_agents[agentId].priority = requestedPriority;

        BrokerMessage response;
        response.type = BrokerMessage::PRIORITY_ACCEPTED;
        response.agentId = agentId;
        response.priority = requestedPriority;
        sendToAgent(agentId, response);

        log(QString("Агент %1 установил приоритет %2")
                .arg(QChar('A' + agentId))
                .arg(requestedPriority));
        break;
    }

    default:
        break;
    }
}

void Broker::sendToAgent(int agentId, const BrokerMessage& message)
{
    if (!m_agents.contains(agentId) || !m_agents[agentId].socket) {
        return;
    }

    QTcpSocket* socket = m_agents[agentId].socket;
    if (socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QByteArray data = message.serialize();
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream << data.size();
    packet.append(data);

    socket->write(packet);
    socket->flush();
}

void Broker::broadcastMessage(const BrokerMessage& message)
{
    for (auto it = m_agents.begin(); it != m_agents.end(); ++it) {
        sendToAgent(it.key(), message);
    }
}

std::vector<int> Broker::collectCurrentState() const
{
    std::vector<int> state(m_boardSize, -1);
    for (auto it = m_agents.begin(); it != m_agents.end(); ++it) {
        if (it->id >= 0 && it->id < m_boardSize) {
            state[it->id] = it->currentRow;
        }
    }
    return state;
}

void Broker::log(const QString& msg)
{
    qDebug() << "[Broker]" << msg;
}