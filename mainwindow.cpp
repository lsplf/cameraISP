#include "mainwindow.h"
#include "ui_mainwindow.h"

#define VIDEO_WIDTH      600
#define VIDEO_HEIGHT     480

#define CAMERA_PATH    "nvarguscamerasrc ! video/x-raw(memory:NVMM),width=640,height=480 ! nvvidconv ! appsink"
#define CAMERA_FLAG    cv::CAP_GSTREAMER


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
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

    connect(&m_videoTimer, SIGNAL(timeout()), SLOT(onTimeout()));
    connect(m_selectRoi, SIGNAL(rectComplete(cv::Rect)), this, SLOT(onRoiSelected(cv::Rect)));

    m_threshold = ui->thrValue->text().toInt();
    int fps = ui->fpsValue->text().toInt();
    m_videoTimer.setInterval(1000/fps);
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
        m_videoCapture.release();
    }
    cv::destroyAllWindows();
    delete ui;
}

void MainWindow::onRoiSelected(const cv::Rect &selectedRect)
{
    if((selectedRect.size().width == 0) || (selectedRect.size().height == 0))
    {
        return;
    }
    m_roiRect[m_cropIdx] = selectedRect;
    m_crop = true;
    qDebug() << "roi selected";
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
    cv::Mat frame;
    bool result = m_videoCapture.read(frame);
    if(!result)
    {
        qDebug() << "read failed -> stop";
        m_videoTimer.stop();
        m_videoCapture.release();
        cv::destroyAllWindows();
        return;
    }
    if(frame.empty())
    {
        qDebug() << "frame empty";
        return;
    }

    QTime debug_time;
    debug_time.start();

    cv::Mat output;
    cv::cvtColor(frame, output, cv::COLOR_YUV2RGB_NV12);

    if(m_start)
    {
        m_start = false;
        QRect rect = m_cameraOutput->geometry();
        rect.setWidth(output.cols);
        rect.setHeight(output.rows);

        qDebug() << "set output size: " << rect;
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
                    qDebug() << "diff";
                    hasDiff = true;
                    break;
                }
            }
        }

        if(hasDiff && (!m_record || !m_capture))
        {
            if(ui->doRecord->isChecked() && !m_record)
            {
                qDebug() << "record start";
                m_record = true;
                QString videoPath = ui->recordPath->text();
                setFilePath(videoPath);
                m_videoWriter = cv::VideoWriter(videoPath.toStdString(),
                                                cv::VideoWriter::fourcc('M','J','P','G'),
                                                25.0, cv::Size(VIDEO_WIDTH, VIDEO_HEIGHT));
            }
            if(ui->doCapture->isChecked() && !m_capture)
            {
                qDebug() << "capture start";
                m_capture = true;
            }
        }
        else if(!hasDiff && m_record)
        {
            qDebug() << "record stop";
            m_record = false;
            m_videoWriter.release();
        }
        else if(!hasDiff && m_capture)
        {
            qDebug() << "capture stop";
            m_capture = false;
        }


        qDebug() << "analyze: " << debug_time.elapsed();
    }

    QImage qimg(output.data, output.cols, output.rows, output.step, QImage::Format_RGB888);
    m_pixmap = QPixmap::fromImage(qimg);

    if(m_crop)
    {
        m_crop = false;
        // Crop image
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
    }
    if(m_capture)
    {
        QTime capture_time;
        capture_time.start();

        QString path = ui->capturePath->text();
        setFilePath(path);
        cv::Mat snapshot;  cv::cvtColor(output, snapshot, cv::COLOR_RGB2BGR);
        cv::imwrite(path.toStdString(), snapshot);

        qDebug() << "capture: " << capture_time.elapsed();
    }
    if(m_record)
    {
        QTime record_time;
        record_time.start();

        cv::cvtColor(output, output, cv::COLOR_BGR2RGB);
        m_videoWriter.write(output);

        qDebug() << "record: " << record_time.elapsed();
    }

    m_cameraOutput->setPixmap(m_pixmap);
    qDebug() << "end: " << debug_time.elapsed();
}

void MainWindow::on_cameraStart_clicked()
{
    m_start = true;
    m_videoCapture = cv::VideoCapture(CAMERA_PATH, CAMERA_FLAG);
    m_videoTimer.start();
}

void MainWindow::on_cameraStop_clicked()
{
    m_start = false;
    m_videoTimer.stop();
    m_videoCapture.release();
    cv::destroyAllWindows();
}

void MainWindow::on_cameraCapture_clicked()
{
    if(m_pixmap.isNull())
    {
        qDebug() << "no pixmap";
        return;
    }
    QString path = ui->snapshotPath->text();
    setFilePath(path);
    m_pixmap.save(path);
}

void MainWindow::on_refApply_1_toggled(bool checked)
{
    m_roiEnable[0] = checked;
}

void MainWindow::on_refApply_2_toggled(bool checked)
{
    m_roiEnable[1] = checked;
}

void MainWindow::on_refApply_3_toggled(bool checked)
{
    m_roiEnable[2] = checked;
}

void MainWindow::on_refDelete_1_clicked()
{
    m_roiEnable[0] = false;
    ui->refImage_1->setPixmap(QPixmap());
    ui->refApply_1->setChecked(false);
    ui->refApply_1->setEnabled(false);
    ui->refDelete_1->setEnabled(false);
}

void MainWindow::on_refDelete_2_clicked()
{
    m_roiEnable[1] = false;
    ui->refImage_2->setPixmap(QPixmap());
    ui->refApply_2->setChecked(false);
    ui->refApply_2->setEnabled(false);
    ui->refDelete_2->setEnabled(false);
}

void MainWindow::on_refDelete_3_clicked()
{
    m_roiEnable[2] = false;
    ui->refImage_3->setPixmap(QPixmap());
    ui->refApply_3->setChecked(false);
    ui->refApply_3->setEnabled(false);
    ui->refDelete_3->setEnabled(false);
}

void MainWindow::on_setFrameRate_clicked()
{
    int fps = ui->fpsValue->text().toInt();
    m_videoTimer.setInterval(1000/fps);
}

void MainWindow::on_setThreshold_clicked()
{
    m_threshold = ui->thrValue->text().toInt();
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
}
