#include "mainwindow.h"
#include "ui_mainwindow.h"

#define VIDEO_WIDTH      640
#define VIDEO_HEIGHT     480

#define CAMERA_PATH   "nvarguscamerasrc ! video/x-raw(memory:NVMM),width=640,height=480,format=NV12 ! nvvidconv ! appsink"
#define CAMERA_FLAG   cv::CAP_GSTREAMER


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_cameraWorking(false),
    m_threshold(10), m_capture(false), m_record(false),
    m_crop(false), m_cropIdx(0)
{
    m_roiEnable[0] = false;
    m_roiEnable[1] = false;
    m_roiEnable[2] = false;
    ui->setupUi(this);

    m_selectRoi = new SelectRoi();
    m_selectRoi->setBackgroundRole(QPalette::Base);
    m_selectRoi->setScaledContents(true);

    m_cameraOutput = new QLabel();
    m_cameraOutput->setAlignment(Qt::AlignCenter);
    m_cameraOutput->setBackgroundRole(QPalette::Base);

    m_stackedLayout = new QStackedLayout();
    m_stackedLayout->setStackingMode(QStackedLayout::StackAll);
    m_stackedLayout->addWidget(m_cameraOutput);
    m_stackedLayout->addWidget(m_selectRoi);

    ui->cameraPanel->setLayout(m_stackedLayout);
    m_cameraOutput->lower();

    /*
    m_cameraRtp = new CameraRtp();
    m_cameraRtp->set_camera();
    ui->gridLayout->addWidget(m_cameraRtp, 0, 6);
    */

    m_videoCapture = cv::VideoCapture(CAMERA_PATH, CAMERA_FLAG);

    connect(&m_videoTimer, SIGNAL(timeout()), SLOT(onTimeout()));
    connect(m_selectRoi, SIGNAL(rectComplete(cv::Rect)), this, SLOT(onRoiSelected(cv::Rect)));

    m_threshold = ui->thrValue->text().toInt();
    int fps = ui->fpsValue->text().toInt();
    m_videoTimer.setInterval(1000/fps);
    m_videoTimer.setTimerType(Qt::PreciseTimer);
    m_videoTimer.start();
}

MainWindow::~MainWindow()
{
    if(m_record)
    {
        m_videoWriter.release();
    }
    if(m_videoTimer.isActive())
    {
        m_videoTimer.stop();
    }
    m_videoCapture.release();
    cv::destroyAllWindows();
    delete ui;
}

void MainWindow::onRoiSelected(const cv::Rect &selectedRect)
{
    if(!m_cameraWorking)
    {
        ui->statusBar->showMessage("camera not working");
        return;
    }
    if((selectedRect.size().width == 0) || (selectedRect.size().height == 0))
    {
        ui->statusBar->showMessage("invalid roi rect (zeroes)");
        return;
    }
    m_roiRect[m_cropIdx] = selectedRect;
    m_crop = true;
    ui->statusBar->showMessage("roi selected");
}

bool MainWindow::detectDiff(cv::Mat &ref, cv::Mat &comp)
{
    if((ref.cols != comp.cols) || (ref.rows != comp.rows))
    {
        return false;
    }

    cv::Mat mask(ref.size(), ref.type());
    cv::absdiff(ref, comp, mask);
    cv::GaussianBlur(mask, mask, cv::Size(5,5), 1.5, 1.5);
    cv::cvtColor(mask, mask, cv::COLOR_BGR2GRAY);

    cv::threshold(mask, mask, m_threshold, 255, cv::THRESH_BINARY);    // | cv::THRESH_OTSU

    if(cv::countNonZero(mask) == 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

void MainWindow::onTimeout()
{
    if(m_cameraWorking)
    {
        ui->frameRate->setText(QString::number(1000/m_fpsTime.elapsed()));
        m_fpsTime.restart();
    }

    cv::Mat frame;
    bool result = m_videoCapture.read(frame);
    if(!m_cameraWorking)
    {
        return;
    }
    if(!result)
    {
        ui->statusBar->showMessage("frame read failed -> timer task stop");
        m_cameraWorking = false;
        return;
    }
    if(frame.empty())
    {
        ui->statusBar->showMessage("frame empty");
        return;
    }

//    qDebug() << "end read: " << m_fpsTime.elapsed();
    m_debugTime.restart();

    cv::Mat output;
    cv::cvtColor(frame, output, cv::COLOR_YUV2RGB_NV12);

    if(m_start)
    {
        m_start = false;
        QRect rect = m_cameraOutput->geometry();
        rect.setWidth(output.cols);
        rect.setHeight(output.rows);

        ui->statusBar->showMessage(QString("frame start -> set output size (%1, %2)")
                                   .arg(output.cols).arg(output.rows));
        m_cameraOutput->setGeometry(rect);
    }

    if(ui->doRecord->isChecked() || ui->doCapture->isChecked())
    {
        bool hasDiff = false;
        for(int cnt=0; cnt<3; cnt++)
        {
            if(m_roiEnable[cnt])
            {
                cv::Mat result = output(m_roiRect[cnt]);
                bool res = detectDiff(m_roiImage[cnt], result);
                if(res)
                {
                    hasDiff = true;
                    break;
                }
            }
        }

        if(hasDiff && (!m_record || !m_capture))
        {
            if(ui->doRecord->isChecked() && !m_record)
            {
                if(setFilePath(m_recordPath))
                {
                    m_record = true;
                    m_videoWriter = cv::VideoWriter(m_recordPath.toStdString(),
                                                    cv::VideoWriter::fourcc('M','J','P','G'),
                                                    30.0, cv::Size(VIDEO_WIDTH, VIDEO_HEIGHT));
                }
                else
                {
                    ui->doRecord->setChecked(false);
                }

                if(!m_debugMsg.isEmpty())
                {
                    ui->statusBar->showMessage(m_debugMsg);
                }
            }
            if(ui->doCapture->isChecked() && !m_capture)
            {
                if(setFilePath(m_capturePath))
                {
                    m_capture = true;
                }
                else
                {
                    ui->doCapture->setChecked(false);
                }

                if(!m_debugMsg.isEmpty())
                {
                    ui->statusBar->showMessage(m_debugMsg);
                }
            }
        }
        else if(!hasDiff && m_record)
        {
            ui->recordMs->setText("stop");
            m_record = false;
            m_videoWriter.release();
        }
        else if(!hasDiff && m_capture)
        {
            ui->captureMs->setText("stop");
            m_capture = false;
        }

        ui->analyzeMs->setText(QString::number(m_debugTime.elapsed()));
    }

    QImage qimg(output.data, output.cols, output.rows, output.step, QImage::Format_RGB888);
    m_pixmap = QPixmap::fromImage(qimg);

    if(m_crop)
    {
        m_crop = false;
        if((m_roiRect[m_cropIdx].x + m_roiRect[m_cropIdx].width > output.cols) ||
            (m_roiRect[m_cropIdx].y + m_roiRect[m_cropIdx].height > output.rows))
        {
            ui->statusBar->showMessage(QString("invalid roi rect <size - (%1,%2)>")
                                       .arg(m_roiRect[m_cropIdx].x + m_roiRect[m_cropIdx].width)
                                       .arg(m_roiRect[m_cropIdx].y + m_roiRect[m_cropIdx].height));
        }
        else if((m_roiRect[m_cropIdx].x < 0) || (m_roiRect[m_cropIdx].y < 0))
        {
            ui->statusBar->showMessage(QString("invalid roi rect <pos - (%1,%2)>")
                                       .arg(m_roiRect[m_cropIdx].x)
                                       .arg(m_roiRect[m_cropIdx].y));
        }
        else
        {
            // Crop image
            if(m_roiEnable[m_cropIdx])
            {
                m_selectRoi->setCropRect(m_cropIdx, m_roiRect[m_cropIdx]);
            }
            m_roiImage[m_cropIdx] = output(m_roiRect[m_cropIdx]);

            QImage qCrop(m_roiImage[m_cropIdx].data, m_roiImage[m_cropIdx].cols,
                         m_roiImage[m_cropIdx].rows, m_roiImage[m_cropIdx].step, QImage::Format_RGB888);
            QPixmap pixmap = QPixmap::fromImage(qCrop);
            pixmap = pixmap.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation);

            // Display Cropped Image
            if(m_cropIdx == 0)
            {
                ui->refImage_1->setPixmap(pixmap);
                ui->refApply_1->setEnabled(true);
                ui->refDelete_1->setEnabled(true);
            }
            else if(m_cropIdx == 1)
            {
                ui->refImage_2->setPixmap(pixmap);
                ui->refApply_2->setEnabled(true);
                ui->refDelete_2->setEnabled(true);
            }
            else if(m_cropIdx == 2)
            {
                ui->refImage_3->setPixmap(pixmap);
                ui->refApply_3->setEnabled(true);
                ui->refDelete_3->setEnabled(true);
            }

            m_cropIdx++;
            if(m_cropIdx >= 3)
            {
                m_cropIdx = 0;
            }
            ui->nextCropPos->setText(QString("next:  ref%1").arg(m_cropIdx+1));
        }
    }
    if(m_capture)
    {
        QTime capture_time;
        capture_time.start();

        if(setFilePath(m_capturePath))
        {
            cv::Mat snapshot;  cv::cvtColor(output, snapshot, cv::COLOR_RGB2BGR);
            cv::imwrite(m_capturePath.toStdString(), snapshot);

            ui->captureMs->setText(QString::number(capture_time.elapsed()));
        }
        else
        {
            m_capture = false;
            ui->doCapture->setChecked(false);
        }

        if(!m_debugMsg.isEmpty())
        {
            ui->statusBar->showMessage(m_debugMsg);
        }
    }
    if(m_record)
    {
        QTime record_time;
        record_time.start();

        cv::cvtColor(output, output, cv::COLOR_BGR2RGB);
        m_videoWriter.write(output);

        ui->recordMs->setText(QString::number(record_time.elapsed()));
    }

    m_cameraOutput->setPixmap(m_pixmap);
    ui->totalMs->setText(QString::number(m_debugTime.elapsed()));
}

void MainWindow::on_cameraStart_clicked()
{
    m_start = true;
    m_cameraWorking = true;
    m_fpsTime.restart();

    m_capturePath = ui->capturePath->text();
    m_recordPath  = ui->recordPath->text();
    ui->capturePath->setEnabled(false);
    ui->recordPath->setEnabled(false);

    ui->cameraStop->setEnabled(true);
    ui->cameraStart->setEnabled(false);
}

void MainWindow::on_cameraStop_clicked()
{
    if(m_record)
    {
        m_record = false;
        m_videoWriter.release();
    }
    on_refDelete_1_clicked();
    on_refDelete_2_clicked();
    on_refDelete_3_clicked();
    m_capture = false;

    m_start = false;
    m_cameraWorking = false;

    ui->totalMs->setText("stop");
    ui->analyzeMs->setText("stop");
    ui->frameRate->setText("stop");
    ui->capturePath->setEnabled(true);
    ui->recordPath->setEnabled(true);

    ui->cameraStop->setEnabled(false);
    ui->cameraStart->setEnabled(true);
}

void MainWindow::on_cameraCapture_clicked()
{
    if(m_pixmap.isNull())
    {
        ui->statusBar->showMessage("no pixmap to capture");
        return;
    }
    QString path = ui->snapshotPath->text();
    setFilePath(path);
    if(!m_debugMsg.isEmpty())
    {
        ui->statusBar->showMessage(m_debugMsg);
    }
    m_pixmap.save(path);
}

void MainWindow::on_refApply_1_toggled(bool checked)
{
    m_roiEnable[0] = checked;
    if(checked)
    {
        m_selectRoi->setCropRect(0, m_roiRect[0]);
    }
    else
    {
        m_selectRoi->deleteRoi(0);
    }
}

void MainWindow::on_refApply_2_toggled(bool checked)
{
    m_roiEnable[1] = checked;
    if(checked)
    {
        m_selectRoi->setCropRect(1, m_roiRect[1]);
    }
    else
    {
        m_selectRoi->deleteRoi(1);
    }
}

void MainWindow::on_refApply_3_toggled(bool checked)
{
    m_roiEnable[2] = checked;
    if(checked)
    {
        m_selectRoi->setCropRect(2, m_roiRect[2]);
    }
    else
    {
        m_selectRoi->deleteRoi(2);
    }
}

void MainWindow::on_refDelete_1_clicked()
{
    m_roiEnable[0] = false;
    m_selectRoi->deleteRoi(0);
    ui->refImage_1->setPixmap(QPixmap());
    ui->refApply_1->setChecked(false);
    ui->refApply_1->setEnabled(false);
    ui->refDelete_1->setEnabled(false);
}

void MainWindow::on_refDelete_2_clicked()
{
    m_roiEnable[1] = false;
    m_selectRoi->deleteRoi(1);
    ui->refImage_2->setPixmap(QPixmap());
    ui->refApply_2->setChecked(false);
    ui->refApply_2->setEnabled(false);
    ui->refDelete_2->setEnabled(false);
}

void MainWindow::on_refDelete_3_clicked()
{
    m_roiEnable[2] = false;
    m_selectRoi->deleteRoi(2);
    ui->refImage_3->setPixmap(QPixmap());
    ui->refApply_3->setChecked(false);
    ui->refApply_3->setEnabled(false);
    ui->refDelete_3->setEnabled(false);
}

void MainWindow::on_setFrameRate_clicked()
{
    bool ok;
    int fps = ui->fpsValue->text().toInt(&ok);
    if(ok)
    {
        m_videoTimer.setInterval(1000/fps);
    }
    else
    {
        ui->statusBar->showMessage("framerate: can't change to int");
    }
}

void MainWindow::on_setThreshold_clicked()
{
    bool ok;
    int thr = ui->thrValue->text().toInt(&ok);
    if(ok)
    {
        m_threshold = thr;
    }
    else
    {
        ui->statusBar->showMessage("threshold: can't change to int");
    }
}

void MainWindow::on_doRecord_toggled(bool checked)
{
    if(checked)
    {
        if(ui->doCapture->isChecked())
        {
            ui->doCapture->setChecked(false);
        }
    }
    else
    {
        if(!ui->doCapture->isChecked())
        {
            ui->analyzeMs->setText("stop");
        }
    }
}

void MainWindow::on_doCapture_toggled(bool checked)
{
    if(checked)
    {
        if(ui->doRecord->isChecked())
        {
            ui->doRecord->setChecked(false);
        }
    }
    else
    {
        if(!ui->doRecord->isChecked())
        {
            ui->analyzeMs->setText("stop");
        }
    }
}

void MainWindow::on_showArea_toggled(bool checked)
{
    m_selectRoi->showRois(checked);
}
