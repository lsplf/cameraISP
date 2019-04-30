#ifndef CAMERA_H
#define CAMERA_H

#include <QTimer>
#include <QProcess>
#include <QTime>
#include <QStyleOption>
#include <QPainter>

#define DEBUG_MSG(x)    qDebug() << (x);

#if defined(Q_OS_LINUX)

#include <QVideoWidget>
#include <QGst/Pipeline>
#include <QGst/Ui/VideoWidget>

class CameraRtp : public QVideoWidget  //QGst::Ui::VideoWidget
{
    Q_OBJECT
public:
    CameraRtp(QWidget *parent = 0);
    ~CameraRtp();

    void set_camera(void);
    void set_channel(quint32 channel);

    void set_property(const QString &property, int value);
    void set_property(const QString &property, float value);
    void set_property(const QString &property, const QString &value);

    void getProperty(const QString &property, int * result, bool * ok);
    void getProperty(const QString &property, float * result, bool * ok);
    void getProperty(const QString &property, QString * result, bool * ok);

    void set_orientation(int value);

    QGst::State state() const;

    void paintEvent(QPaintEvent* event);

public Q_SLOTS:
    void play();
    void pause();
    void stop();

Q_SIGNALS:
    void stateChanged();
    //void setLog(LogType type, const QString &msg);

private:
    void onBusMessage(const QGst::MessagePtr & message);
    void handlePipelineStateChange(const QGst::StateChangedMessagePtr & scm);

    QGst::PipelinePtr m_pipeline;

    QGst::ElementPtr camerasrc;
    QGst::ElementPtr app_vidconv;
    QGst::ElementPtr udp_vidconv;
    QGst::ElementPtr udpsink;
};

#else

class CameraRtp : public QWidget
{
    Q_OBJECT
public:
    explicit CameraRtp(QWidget *parent = 0) : QWidget(parent) {  }

    void set_camera(void) {}
    void set_channel(quint32 channel) { Q_UNUSED(channel); }

    void set_property(const QString &property, int value) { Q_UNUSED(property); Q_UNUSED(value); }
    void set_property(const QString &property, float value) { Q_UNUSED(property); Q_UNUSED(value); }
    void set_property(const QString &property, const QString &value) { Q_UNUSED(property); Q_UNUSED(value);}

    void getProperty(const QString &property, int * result, bool * ok)
    { Q_UNUSED(property); Q_UNUSED(result); *ok = false; }
    void getProperty(const QString &property, float * result, bool * ok)
    { Q_UNUSED(property); Q_UNUSED(result); *ok = false; }
    void getProperty(const QString &property, QString * result, bool * ok)
    { Q_UNUSED(property); Q_UNUSED(result); *ok = false; }

    void set_orientation(int value) { Q_UNUSED(value); }

    quint32 state() { return 0; }

signals:
    void stateChanged();
    void setLog(LogType type, const QString &msg);

public Q_SLOTS:
    void play() {}
    void pause() {}
    void stop() {}
};

#if defined(Q_OS_LINUX)
#include "moc_camerartp.cpp"
#endif

#endif

#endif // CAMERA_H
