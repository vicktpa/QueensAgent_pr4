#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>

#ifdef BROKER_MODE
#include "BrokerWindow.h"
#endif

#ifdef AGENT_MODE
#include "AgentWindow.h"
#endif

/**
 * @brief Главная функция программы
 * @param argc Количество аргументов командной строки
 * @param argv Массив аргументов командной строки
 * @return Код возврата (0 - успех)
 *
 * Программа поддерживает два режима работы:
 *
 * 1. Режим БРОКЕРА (координатор):
 *    - Запускает сервер, который координирует работу агентов
 *    - Управляет поиском решений задачи о ферзях
 *    - Отображает шахматную доску и найденные решения
 *    - Позволяет управлять базой знаний и приоритетами
 *
 * 2. Режим АГЕНТА (исполнитель):
 *    - Подключается к брокеру
 *    - Отвечает за один столбец доски
 *    - Перебирает возможные позиции ферзя
 *    - Отправляет запросы брокеру на проверку совместимости
 *
 * Использование:
 *   ./queens-broker --mode broker
 *   ./queens-agent --mode agent --id 0 --host 127.0.0.1 --port 12345
 *
 * Аргументы командной строки:
 *   --mode, -m       Режим работы: broker или agent
 *   --id, -i         ID агента (0-9, для режима agent)
 *   --host, -H       Адрес брокера (по умолчанию 127.0.0.1)
 *   --port, -p       Порт брокера (по умолчанию 12345)
 *   --help, -h       Показать справку
 *   --version, -v    Показать версию
 */
int main(int argc, char *argv[])
{
    // Инициализация Qt-приложения
    QApplication app(argc, argv);
    app.setApplicationName("Distributed Queens Solver");
    app.setApplicationVersion("2.0");

    // ========================================================================
    // Парсинг аргументов командной строки
    // ========================================================================

    QCommandLineParser parser;
    parser.setApplicationDescription("Распределенный решатель задачи о ферзях");
    parser.addHelpOption();
    parser.addVersionOption();

    // Опция для выбора режима работы
    QCommandLineOption modeOption(
        QStringList() << "m" << "mode",
        "Режим работы: broker или agent",
        "mode"
        );
    parser.addOption(modeOption);

    // Опция для ID агента (только для режима agent)
    QCommandLineOption agentIdOption(
        QStringList() << "i" << "id",
        "ID агента (столбец, 0-9)",
        "id"
        );
    parser.addOption(agentIdOption);

    // Опция для адреса брокера
    QCommandLineOption hostOption(
        QStringList() << "H" << "host",
        "Адрес брокера (по умолчанию 127.0.0.1)",
        "host",
        "127.0.0.1"
        );
    parser.addOption(hostOption);

    // Опция для порта брокера
    QCommandLineOption portOption(
        QStringList() << "p" << "port",
        "Порт брокера (по умолчанию 12345)",
        "port",
        "12345"
        );
    parser.addOption(portOption);

    // Разбор аргументов
    parser.process(app);

    // Определение режима работы
    QString mode = parser.value(modeOption).toLower();

    // Если режим не указан, определяем по скомпилированному файлу
    if (mode.isEmpty()) {
#ifdef BROKER_MODE
        mode = "broker";
#elif defined(AGENT_MODE)
        mode = "agent";
#else
        // Ошибка: режим не указан и не определен при компиляции
        QMessageBox::critical(nullptr, "Ошибка",
                              "Не указан режим работы.\n"
                              "Используйте --mode broker для запуска брокера\n"
                              "Используйте --mode agent --id N для запуска агента\n\n"
                              "Примеры:\n"
                              "  ./queens-broker --mode broker\n"
                              "  ./queens-agent --mode agent --id 0\n"
                              "  ./queens-agent --mode agent --id 1 --host 192.168.1.100 --port 12345");
        return 1;
#endif
    }

    // ========================================================================
    // Запуск в режиме БРОКЕРА
    // ========================================================================

    if (mode == "broker") {
#ifdef BROKER_MODE
        // Создаем и показываем главное окно брокера
        BrokerWindow brokerWindow;
        brokerWindow.show();
        return app.exec();  // Запускаем цикл обработки событий
#else
        QMessageBox::critical(nullptr, "Ошибка",
                              "Данный исполняемый файл собран без поддержки режима брокера.\n"
                              "Используйте queens-broker или пересоберите с BROKER_MODE.");
        return 1;
#endif
    }

    // ========================================================================
    // Запуск в режиме АГЕНТА
    // ========================================================================

    else if (mode == "agent") {
#ifdef AGENT_MODE
        // Проверяем, указан ли ID агента
        if (!parser.isSet(agentIdOption)) {
            QMessageBox::critical(nullptr, "Ошибка",
                                  "Для режима agent необходимо указать ID через --id N");
            return 1;
        }

        // Получаем параметры из командной строки
        int agentId = parser.value(agentIdOption).toInt();
        QString host = parser.value(hostOption);
        int port = parser.value(portOption).toInt();

        // Проверяем валидность ID агента
        if (agentId < 0 || agentId > 9) {
            QMessageBox::warning(nullptr, "Ошибка",
                                 "ID агента должен быть от 0 до 9");
            return 1;
        }

        // Создаем и показываем окно агента
        AgentWindow agentWindow(agentId);
        agentWindow.setBrokerAddress(host, port);
        agentWindow.show();
        return app.exec();  // Запускаем цикл обработки событий
#else
        QMessageBox::critical(nullptr, "Ошибка",
                              "Данный исполняемый файл собран без поддержки режима агента.\n"
                              "Используйте queens-agent или пересоберите с AGENT_MODE.");
        return 1;
#endif
    }

    // ========================================================================
    // Неизвестный режим
    // ========================================================================

    else {
        QMessageBox::critical(nullptr, "Ошибка",
                              QString("Неизвестный режим '%1'. Используйте 'broker' или 'agent'").arg(mode));
        return 1;
    }
}