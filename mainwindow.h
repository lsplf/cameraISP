#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QString>
#include <QTimer>
#include <QTime>
#include <QImage>
#include <QPixmap>
#include <QGraphicsPixmapItem>
#include <QDebug>
#include <QThread>
#include <QLabel>
#include <QStackedLayout>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio/videoio.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

#include "camerartp.h"
#include "selectroi.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void onRoiSelected(const cv::Rect &selectedRect);
    void onTimeout();

    void on_cameraStart_clicked();
    void on_cameraStop_clicked();
    void on_cameraCapture_clicked();

    void on_refApply_1_toggled(bool checked);
    void on_refApply_2_toggled(bool checked);
    void on_refApply_3_toggled(bool checked);
    void on_refDelete_1_clicked();
    void on_refDelete_2_clicked();
    void on_refDelete_3_clicked();

    void on_setFrameRate_clicked();
    void on_setThreshold_clicked();

    void on_doRecord_toggled(bool checked);
    void on_doCapture_toggled(bool checked);

    void on_showArea_toggled(bool checked);

private:
    bool detectDiff(cv::Mat &ref, cv::Mat &comp);

    Ui::MainWindow *ui;
    QStackedLayout * m_stackedLayout;
    QLabel    * m_cameraOutput;
    SelectRoi * m_selectRoi;
    CameraRtp * m_cameraRtp;

    QTimer           m_videoTimer;
    QPixmap          m_pixmap;

    cv::VideoCapture m_videoCapture;
    cv::VideoWriter m_videoWriter;

    cv::Mat  m_roiImage[3];
    cv::Rect m_roiRect[3];
    bool     m_roiEnable[3];
    int m_threshold;

    bool m_cameraWorking;
    bool m_start;
    bool m_capture;
    bool m_record;
    bool m_crop;
    int m_cropIdx;

    QString m_capturePath;
    QString m_recordPath;
    QTime   m_debugTime;
    QTime   m_fpsTime;

    QString m_debugMsg;

    bool setFilePath(QString &file_path)
    {
        QTime path_time;  path_time.start();
        QFileInfo info(file_path);
        if(!info.dir().exists())
        {
            m_debugMsg = QString("directory not exists <%1>").arg(info.dir().dirName());
            return false;
        }

        QString fileName = file_path;
        int nameCount = 0;
        do
        {
            info.setFile(fileName);
            if(!info.exists())
            {
                if(nameCount > 0)
                {
                    m_debugMsg = QString("file name changed to <%1>").arg(info.fileName());
                }
                else
                {
                    m_debugMsg = "";
                }
                break;
            }
            QStringList base = info.baseName().split("_");
            QString baseName = ((info.baseName().at(0) == '_') ? "_" : "") + base.at(0);
            if(base.length() > 1)
            {
                bool ok;
                QString temp = base.at(base.length()-1);
                int tempCnt = temp.toInt(&ok);
                if(ok)
                {
                    nameCount = tempCnt+1;
                }
            }

            for(int cnt=1; cnt<base.length()-1; cnt++)
            {
                baseName += "_" + base.at(cnt);
            }
            fileName = info.canonicalPath() + QDir::separator() +
                       baseName + QString("_%1").arg(nameCount, 3, 10, QChar('0'));
            if (!info.completeSuffix().isEmpty())
            {
                fileName += "." + info.completeSuffix();
            }
            nameCount++;
        } while(1);

        file_path = fileName;

        qDebug() << "setPath: " << path_time.elapsed();
        return true;
    }
};

#endif // MAINWINDOW_H
