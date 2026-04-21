# ============================================================================
# Минимальная версия CMake и проект
# ============================================================================

# Требуется CMake версии 3.16 или выше (для Qt6 поддержки)
cmake_minimum_required(VERSION 3.16)

# Определяем проект с именем pr1_4 и версией 0.1
# LANGUAGES CXX указывает, что используется только C++
project(pr1_4 VERSION 0.1 LANGUAGES CXX)

# ============================================================================
# Настройки компиляции C++
# ============================================================================

# Используем стандарт C++17 (требуется для std::optional, std::variant и т.д.)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)   # Обязательно использовать C++17
set(CMAKE_CXX_EXTENSIONS OFF)         # Отключаем расширения компилятора

# ============================================================================
# Автоматическая обработка Qt-специфичных файлов
# ============================================================================

# AUTOMOC: автоматически запускает moc (Meta-Object Compiler) для файлов с Q_OBJECT
# AUTORCC: автоматически обрабатывает .qrc файлы ресурсов
# AUTOUIC: автоматически обрабатывает .ui файлы форм
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ============================================================================
# Поиск компонентов Qt6
# ============================================================================

# Ищем необходимые модули Qt6:
# - Core: базовые классы (QObject, QString, контейнеры)
# - Widgets: GUI компоненты (окна, кнопки, таблицы)
# - Gui: графические классы (QPainter, QColor, QImage)
# - Sql: работа с базами данных (SQLite)
# - Network: сетевое взаимодействие (TCP-сокеты)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Sql Network)

# Стандартная настройка Qt-проекта (устанавливает DEFINES и т.д.)
qt_standard_project_setup()

# ============================================================================
# Общие исходные файлы (используются и брокером, и агентом)
# ============================================================================

set(COMMON_SOURCES
    queenagent.cpp           # Класс ферзя-агента для локального поиска
    chessboardwidget.cpp     # Виджет шахматной доски
    queenssolver.cpp         # Локальный решатель задачи о ферзях
    knowledgebase.cpp        # База знаний (SQLite)
    knowledgebasedialog.cpp  # Диалог управления базой знаний
    sortdialog.cpp           # Диалог сортировки решений
    prioritymanager.cpp      # Менеджер приоритетов и фиксаций
    priorityspinbox.cpp      # Специализированный спинбокс для приоритетов
)

set(COMMON_HEADERS
    queenagent.h
    chessboardwidget.h
    queenssolver.h
    knowledgebase.h
    knowledgebasedialog.h
    sortdialog.h
    prioritymanager.h
    priorityspinbox.h
)

# ============================================================================
# БРОКЕР - координатор распределенной системы
# ============================================================================

add_executable(QueensBroker
    main.cpp                 # Точка входа (поддерживает оба режима)
    broker.cpp               # Логика брокера
    brokerwindow.cpp         # Главное окно брокера
    ${COMMON_SOURCES}        # Общие исходники
    broker.h                 # Заголовочный файл брокера
    brokerwindow.h           # Заголовочный файл окна брокера
    ${COMMON_HEADERS}        # Общие заголовки
)

# Устанавливаем имя выходного файла
set_target_properties(QueensBroker PROPERTIES
    OUTPUT_NAME "queens-broker"
)

# Определяем макрос BROKER_MODE для условной компиляции
# Это позволяет использовать один main.cpp для обоих режимов
target_compile_definitions(QueensBroker PRIVATE BROKER_MODE)

# Подключаем необходимые библиотеки Qt
target_link_libraries(QueensBroker
    PRIVATE Qt6::Core       # Базовые классы Qt
    PRIVATE Qt6::Widgets    # Виджеты GUI
    PRIVATE Qt6::Gui        # Графические классы
    PRIVATE Qt6::Sql        # Базы данных SQL
    PRIVATE Qt6::Network    # Сетевое взаимодействие
)

# ============================================================================
# Платформо-зависимые настройки для macOS
# ============================================================================

if(APPLE)
    set_target_properties(QueensBroker PROPERTIES
        MACOSX_BUNDLE TRUE   # Создаем .app бандл для macOS
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.queens.broker"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    )
elseif(WIN32)
    # Для Windows создаем обычный .exe без консольного окна
    set_target_properties(QueensBroker PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# ============================================================================
# АГЕНТ - исполнитель, подключающийся к брокеру
# ============================================================================

add_executable(QueensAgent
    main.cpp                 # Точка входа (поддерживает оба режима)
    agentclient.cpp          # Клиент для связи с брокером
    agentwindow.cpp          # Главное окно агента
    agentclient.h
    agentwindow.h
)

# Устанавливаем имя выходного файла
set_target_properties(QueensAgent PROPERTIES
    OUTPUT_NAME "queens-agent"
)

# Определяем макрос AGENT_MODE для условной компиляции
target_compile_definitions(QueensAgent PRIVATE AGENT_MODE)

# Агенту не нужны Sql и Widgets (использует только Core, Gui, Network)
target_link_libraries(QueensAgent
    PRIVATE Qt6::Core
    PRIVATE Qt6::Widgets
    PRIVATE Qt6::Gui
    PRIVATE Qt6::Network
)

# Платформо-зависимые настройки для агента (аналогично брокеру)
if(APPLE)
    set_target_properties(QueensAgent PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.queens.agent"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    )
elseif(WIN32)
    set_target_properties(QueensAgent PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# ============================================================================
# Установка (опционально)
# ============================================================================

# Определяет куда копировать собранные файлы при установке
install(TARGETS QueensBroker QueensAgent
    BUNDLE DESTINATION .          # Для macOS: .app в корень установки
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)