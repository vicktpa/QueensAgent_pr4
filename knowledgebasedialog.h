#ifndef KNOWLEDGEBASEDIALOG_H
#define KNOWLEDGEBASEDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include "KnowledgeBase.h"

/**
 *  Диалоговое окно для управления базой знаний
 *
 * Позволяет:
 * - Просматривать все клетки доски в виде таблицы
 * - Редактировать вес и цвет каждой клетки
 * - Загружать и сохранять базу знаний в файл
 * - Генерировать различные типы распределений
 *
 * Каждая ячейка таблицы отображает вес и цвет клетки.
 * Для редактирования нужно дважды кликнуть по ячейке.
 */
class KnowledgeBaseDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     *  Конструктор
     *  knowledgeBase Указатель на объект базы знаний
     *  boardSize Размер доски
     *  parent Родительский виджет
     */
    explicit KnowledgeBaseDialog(KnowledgeBase* knowledgeBase, int boardSize,
                                 QWidget* parent = nullptr);
    ~KnowledgeBaseDialog();

private slots:
    // Действия с файлами
    void loadFromFile();      // Загрузка из файла
    void saveToFile();        // Сохранение в файл

    // Генерация данных
    void generateRandom();    // Случайное распределение (3 цвета)
    void generateTwoColor();  // Двухцветное распределение (50/50)
    void generateThreeColor(); // Трехцветное распределение
    void generateAllGreen();  // Все клетки зеленые

    // Управление диалогом
    void applyChanges();      // Применить изменения
    void cancelChanges();     // Отменить изменения (закрыть)

    // Обработка изменений в таблице
    void onCellChanged(int row, int column);  // При изменении ячейки
    void updateStatistics();                   // Обновление статистики

private:
    /**
     *  Настройка пользовательского интерфейса
     *
     * Создает таблицу, кнопки и поля для отображения статистики.
     */
    void setupUI();

    /**
     *  Загружает данные из базы знаний в таблицу
     *
     * Для каждой клетки отображает вес и цвет.
     */
    void loadDataToTable();

    /**
     *  Сохраняет данные из таблицы в базу знаний
     */
    void saveDataFromTable();

    /**
     *  Обновляет цвета фона ячеек в соответствии с выбранным цветом
     */
    void updateTableColors();

private:
    KnowledgeBase* m_knowledgeBase;  // Указатель на базу знаний
    int m_boardSize;                  // Размер доски

    // Элементы интерфейса
    QTableWidget* m_tableWidget;      // Таблица для отображения клеток

    // Кнопки управления
    QPushButton* m_loadButton;
    QPushButton* m_saveButton;
    QPushButton* m_randomButton;      // Не используется в текущей версии
    QPushButton* m_twoColorButton;
    QPushButton* m_threeColorButton;
    QPushButton* m_allGreenButton;
    QPushButton* m_applyButton;
    QPushButton* m_cancelButton;

    // Поля для отображения статистики
    QLabel* m_statsLabel;
    QLabel* m_avgWeightLabel;      // Средний вес
    QLabel* m_redCountLabel;       // Количество красных клеток
    QLabel* m_greenCountLabel;     // Количество зеленых клеток
    QLabel* m_blueCountLabel;      // Количество синих клеток
};

#endif // KNOWLEDGEBASEDIALOG_H