#ifndef SELECTROI_H
#define SELECTROI_H

#include <QWidget>
#include <QMouseEvent>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QTimer>
#include <QDebug>

#include <opencv2/opencv.hpp>

struct SelectionState {
  cv::Point startPt, endPt, mousePos;
  bool started = false, done = false;

  cv::Rect toRectCv() {
    return cv::Rect (
      std::min(this->startPt.x, this->mousePos.x),
      std::min(this->startPt.y, this->mousePos.y),
      std::abs(this->startPt.x-this->mousePos.x),
      std::abs(this->startPt.y-this->mousePos.y));
  }

  QRect toRectQt() {
    return QRect (
      std::min(this->startPt.x, this->mousePos.x),
      std::min(this->startPt.y, this->mousePos.y),
      std::abs(this->startPt.x-this->mousePos.x),
      std::abs(this->startPt.y-this->mousePos.y));
  }
};

class SelectRoi : public QLabel
{
    Q_OBJECT
public:
    explicit SelectRoi(QWidget *parent = nullptr);
    ~SelectRoi();
    void setCropRect(int cropIdx, const cv::Rect &selectedRect);
    void deleteRoi(int roiIdx);
    void showRois(bool show);

signals:
    void rectComplete(const cv::Rect &selectedRect);

public slots:
    void onTimeout();

protected:
    void mousePressEvent(QMouseEvent * ev);
    void mouseMoveEvent(QMouseEvent * ev);
    void mouseReleaseEvent(QMouseEvent * ev);

private:

    QTimer m_timer;
    SelectionState m_posState;

    QPixmap * m_pixmapSrc;
    bool      m_update;

    QColor m_roiColor;
    QPen   m_roiPen;

    QColor m_drawColor;
    QPen   m_drawPen;

    cv::Rect m_roiRect[3];
    bool     m_roiUsed[3];

    bool m_showAreas;
};

#endif // SELECTROI_H
