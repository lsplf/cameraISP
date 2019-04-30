#include "selectroi.h"

SelectRoi::SelectRoi(QWidget *parent) : QLabel(parent)
{
    m_rectColor.setRgb(255,0,0,255);
    m_rectPen.setWidth(3);
    m_rectPen.setColor(m_rectColor);

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
}

SelectRoi::~SelectRoi()
{
}

void SelectRoi::onTimeout()
{
    if(m_posState.started)
    {
        this->setPixmap(*m_pixmapSrc);
        this->adjustSize();
    }
}

void SelectRoi::mousePressEvent(QMouseEvent * ev)
{
    if(ev->button() == Qt::LeftButton)
    {
        if(!m_posState.started)
        {
            m_posState.started = true;
            m_posState.startPt.x = ev->x();
            m_posState.startPt.y = ev->y();
            m_posState.mousePos.x = ev->x();
            m_posState.mousePos.y = ev->y();

            m_pixmapSrc = new QPixmap(this->size());
            m_pixmapSrc->fill(Qt::transparent);
            m_timer.start(50);

            //qDebug() << "start area Rect";
        }
    }

    QLabel::mousePressEvent(ev);
}

void SelectRoi::mouseMoveEvent(QMouseEvent * ev)
{
    if(m_posState.started)
    {
        m_posState.mousePos.x = ev->x();
        m_posState.mousePos.y = ev->y();

        delete m_pixmapSrc;
        m_pixmapSrc = new QPixmap(this->size());
        m_pixmapSrc->fill(Qt::transparent);
        QPainter painter(m_pixmapSrc);
        painter.setPen(m_rectPen);
        painter.drawRect(m_posState.toRectQt());

        //qDebug() << "moving mouse";
    }

    QLabel::mouseMoveEvent(ev);
}

void SelectRoi::mouseReleaseEvent(QMouseEvent * ev)
{
    if(ev->button() == Qt::LeftButton)
    {
        if(m_posState.started)
        {
            m_posState.started = false;
            m_posState.endPt.x = ev->x();
            m_posState.endPt.y = ev->y();

            qDebug() << "rectComplete ";
            emit rectComplete(m_posState.toRectCv());

            m_timer.stop();
            delete m_pixmapSrc;
        }
    }

    QLabel::mouseReleaseEvent(ev);
}
