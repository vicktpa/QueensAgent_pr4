Для запуска кода должно быть установлено: 
1. Компилятор C++
2. QT 6
3. Cmake

Настройки для Linux (Ubuntu/Debian) - самый простой вариант: 
1. Установить компилятора и CMake: 
sudo apt install -y build-essential
sudo apt install -y cmake
sudo apt install -y g++-11
2. Установить QT 6:
sudo apt install -y qt6-base-dev
sudo apt install -y qt6-base-dev-tools
sudo apt install -y qt6-tools-dev
sudo apt install -y qt6-tools-dev-tools
sudo apt install -y libqt6core6
sudo apt install -y libqt6widgets6
sudo apt install -y libqt6gui6

Настройки для macOS: 
1. Установить Xcode Command Line Tools
2. Установить Homebrew (менеджер пакетов): /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
3. Установить CMake через Homebrew: brew install cmake
4. Установить Qt 6 через Homebrew: brew install qt6

Настройки для Windows: 
1. Установить Visual Studio 2022 (компилятор C++)
2. Установить CMake:
Скачать cmake-3.29.0-windows-x86_64.msi (или новее) с сайта https://cmake.org/download/.
При установке выбрать опцию "Add CMake to the system PATH". 
3. Установить QT 6 с сайта https://www.qt.io/download


Сборка и запуск проекта: 
1. Клонировать этот проект
2. Сборка проекта

macOS / Linux через терминал:
cd /путь_к_папке_с_проектом
mkdir build
cd build
cmake ..
make
./QueensAgent

Windows (Visual Studio) через PowerShell:
cd путь_к_папке_с_проектом
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
.\Release\QueensAgent.exe

Windows (MinGW):
cd /путь_к_папке_с_проектом
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
mingw32-make
.\QueensAgent.exe

3. Настройка CMake, если Qt не найден
Если при выполнении cmake .. возникает ошибка "Could not find Qt6", необходимо указать путь к Qt вручную.
