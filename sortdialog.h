#ifndef SORTDIALOG_H
#define SORTDIALOG_H

#include <QDialog>
#include <QRadioButton>
#include <QPushButton>

/**
 *  Диалоговое окно для выбора параметров сортировки решений
 *
 * Позволяет пользователю выбрать:
 * - Направление сортировки (по возрастанию или убыванию веса)
 *
 * После выбора параметров отправляет сигнал sortRequested(bool ascending)
 * для выполнения сортировки в основном окне.
 */
class SortDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     *  Конструктор диалога сортировки
     *  parent Родительский виджет
     *
     * Создает диалоговое окно с двумя radio-кнопками для выбора
     * направления сортировки и кнопками "Сортировать" и "Отмена".
     */
    explicit SortDialog(QWidget* parent = nullptr);
    ~SortDialog();

    /**
     *  Возвращает выбранное направление сортировки
     *  true - по возрастанию, false - по убыванию
     *
     * По умолчанию выбрана сортировка по возрастанию (от легких к тяжелым).
     */
    bool isAscending() const { return m_ascendingRadio->isChecked(); }

signals:
    /**
     *  Сигнал, отправляемый при нажатии кнопки "Сортировать"
     *  ascending true - по возрастанию, false - по убыванию
     *
     * Подключается к слоту onSolutionsSorted(bool ascending) в MainWindow.
     */
    void sortRequested(bool ascending);

private slots:
    /**
     *  Обработчик нажатия кнопки "Сортировать"
     *
     * Отправляет сигнал sortRequested с выбранным направлением
     * и закрывает диалог с кодом accept().
     */
    void onSortClicked();

private:
    /**
     *  Настройка пользовательского интерфейса
     *
     * Создает:
     * - Группу с radio-кнопками для выбора направления
     * - Кнопки "Сортировать" и "Отмена"
     * - Устанавливает layout
     */
    void setupUI();

private:
    QRadioButton* m_ascendingRadio;   ///< Radio-кнопка "По возрастанию" (по умолчанию)
    QRadioButton* m_descendingRadio;  ///< Radio-кнопка "По убыванию"
    QPushButton* m_sortButton;        ///< Кнопка "Сортировать"
    QPushButton* m_cancelButton;      ///< Кнопка "Отмена"
};

#endif // SORTDIALOG_H