#include "priorityspinbox.h"
#include <QToolTip>
#include <QMouseEvent>

PrioritySpinBox::PrioritySpinBox(QWidget* parent)
    : QSpinBox(parent)
    , m_isFixed(false)
    , m_column(-1)
    , m_row(-1)
{
    setRange(0, 10);
    setValue(0);
    setButtonSymbols(QSpinBox::NoButtons);
    setAlignment(Qt::AlignCenter);
    setFixedSize(45, 22);
    setToolTip("Приоритет (0=нет, 1=наивысший)\nДвойной клик - открыть редактор");

    // Делаем спинбокс только для чтения (нельзя менять стрелками)
    setReadOnly(true);
    setFocusPolicy(Qt::NoFocus);

    setStyleSheet(
        "QSpinBox {"
        "   background-color: rgba(0,0,0,180);"
        "   color: white;"
        "   border: 1px solid #c0c0c0;"
        "   border-radius: 3px;"
        "   font-weight: bold;"
        "   font-size: 10px;"
        "}"
        );
}

void PrioritySpinBox::setPriorityValue(int priority)
{
    setValue(priority);
    update();
}

void PrioritySpinBox::setFixed(bool fixed)
{
    m_isFixed = fixed;

    if (fixed) {
        setToolTip("ЗАФИКСИРОВАНО! Приоритет: " + QString::number(value()) + "\nДвойной клик - открыть редактор");
    } else {
        setToolTip("Приоритет: " + QString::number(value()) + "\nДвойной клик - открыть редактор");
    }

    update();
}

void PrioritySpinBox::setPosition(int col, int row)
{
    m_column = col;
    m_row = row;
}

void PrioritySpinBox::setFixedRow(int row)
{
    m_row = row;
    update();
}

void PrioritySpinBox::paintEvent(QPaintEvent* event)
{
    QSpinBox::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_isFixed) {
        painter.setPen(Qt::white);
        painter.setFont(QFont("Segoe UI", 8));
        painter.drawText(rect().adjusted(2, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, "🔒");
    }

    if (value() > 0 && value() <= 3) {
        painter.setPen(QColor(255, 215, 0));
        painter.setFont(QFont("Segoe UI", 8));
        painter.drawText(rect().adjusted(0, 0, -2, 0), Qt::AlignRight | Qt::AlignVCenter, "★");
    }
}

void PrioritySpinBox::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event)

    if (m_column >= 0) {
        emit editRequested(m_column, m_row);
    }

    event->accept();
}