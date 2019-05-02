#include "selectroi.h"

SelectRoi::SelectRoi(QWidget *parent) :
    QLabel(parent), m_pixmapSrc(Q_NULLPTR), m_update(false), m_showAreas(false)
{
    m_roiColor.setRgb(255,0,0,255);
    m_roiPen.setWidth(3);
    m_roiPen.setColor(m_roiColor);

    m_drawColor.setRgb(0,0,255,255);
    m_drawPen.setWidth(2);
    m_drawPen.setColor(m_drawColor);

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));

    for(int cnt=0; cnt<3; cnt++)
    {
        m_roiUsed[cnt] = false;
    }
}

SelectRoi::~SelectRoi()
{
    if(m_timer.isActive())
    {
        m_timer.stop();
    }
}

void SelectRoi::onTimeout()
{
    if(m_update)
    {
        m_update = false;

        if(m_pixmapSrc != Q_NULLPTR)
        {
            delete m_pixmapSrc; m_pixmapSrc = Q_NULLPTR;
        }

        m_pixmapSrc = new QPixmap(this->size());
        m_pixmapSrc->fill(Qt::transparent);
        QPainter painter(m_pixmapSrc);

        if(m_showAreas)
        {
            painter.setPen(m_drawPen);
            for(int cnt=0; cnt<3; cnt++)
            {
                if(m_roiUsed[cnt])
                {
                    painter.drawRect(m_roiRect[cnt].x, m_roiRect[cnt].y,
                                     m_roiRect[cnt].width, m_roiRect[cnt].height);
                }
            }
        }

        if(m_posState.started)
        {
            painter.setPen(m_roiPen);
            painter.drawRect(m_posState.toRectQt());
        }

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

            if(!m_timer.isActive())
            {
                m_timer.start(50);
            }

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

        m_update = true;
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

            if(!m_showAreas)
            {
                m_timer.stop();

                this->setPixmap(QPixmap());
                this->adjustSize();
            }
            else
            {
                m_update = true;
            }
        }
    }

    QLabel::mouseReleaseEvent(ev);
}

void SelectRoi::setCropRect(int cropIdx, const cv::Rect &selectedRect)
{
    if(cropIdx >= 3)
    {
        return;
    }
    m_roiRect[cropIdx] = selectedRect;
    m_roiUsed[cropIdx] = true;
    if(m_showAreas)
    {
        m_update = true;
    }
}

void SelectRoi::deleteRoi(int roiIdx)
{
    m_roiUsed[roiIdx] = false;
    if(m_showAreas)
    {
        m_update = true;
    }
}

void SelectRoi::showRois(bool show)
{
    m_showAreas = show;
    if(show)
    {
        m_update = true;
        m_timer.start(50);
    }
    else
    {
        m_timer.stop();
        this->setPixmap(QPixmap());
        this->adjustSize();
    }
}
