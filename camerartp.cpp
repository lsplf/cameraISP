#include <QProcess>

#if defined(Q_OS_LINUX)

#include "camerartp.h"
#include <QDir>
#include <QUrl>
#include <QGst/Init>
#include <QGlib/Connect>
#include <QGlib/Error>
#include <QGst/Pipeline>
#include <QGst/ElementFactory>
#include <QGst/Bus>
#include <QGst/Message>
#include <QGst/Query>
#include <QGst/ClockTime>
#include <QGst/Event>
#include <QGst/StreamVolume>
#include <QGst/VideoOverlay>
#include <QGst/Pad>

#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#define CAMERA_PATH    "nvarguscamerasrc ! video/x-raw(memory:NVMM),width=600,height=480 ! nvvidconv ! appsink"

CameraRtp::CameraRtp(QWidget *parent)
    : QVideoWidget(parent)  //QGst::Ui::VideoWidget(parent)
{
    QGst::init(0, 0);

    this->setStyleSheet("border-image: url(:/image/colorbar.png) 0 0 0 0 stretch stretch;");

}

CameraRtp::~CameraRtp()
{
    if (m_pipeline) {
        m_pipeline->setState(QGst::StateNull);
        //stopPipelineWatch();
    }
}

void CameraRtp::set_camera(void)
{
    if(!m_pipeline)
    {
        m_pipeline = QGst::Pipeline::create();

        if (m_pipeline) {
            // camerasrc
            camerasrc = QGst::ElementFactory::make("nvarguscamerasrc");
            if(!camerasrc) { qCritical() << "Failed to create the nvarguscamerasrc"; return; }
            m_pipeline->add(camerasrc);

            //camerasrc->setProperty("sensor-id", 0);

            // caps filter
            QGst::ElementPtr filter = QGst::ElementFactory::make("capsfilter");
            if(!filter) { qCritical() << "Failed to create the filter"; return; }
            m_pipeline->add(filter);

            //filter->setProperty("caps", "video/x-raw(memory:NVMM),width=600,height=480,format=NV12");

            camerasrc->link(filter);

            // tee
            QGst::ElementPtr tee = QGst::ElementFactory::make("tee");
            if(!tee) { qCritical() << "Failed to create the tee"; return; }
            m_pipeline->add(tee);

            //filter->link(tee);

            // video out
            QGst::ElementPtr queue = QGst::ElementFactory::make("queue");
            if(!queue) { qCritical() << "Failed to create the queue"; return; }
            m_pipeline->add(queue);

            app_vidconv = QGst::ElementFactory::make("nvvidconv");
            if(!app_vidconv) { qCritical() << "Failed to create the nvvidconv"; return; }
            m_pipeline->add(app_vidconv);

            QGst::ElementPtr sink = QGst::ElementFactory::make("nveglglessink");
            if(!sink) { qCritical() << "Failed to create the nveglglessink"; return; }
            m_pipeline->add(sink);

            //tee->getRequestPad("src_%u")->link(queue->getStaticPad("sink"));
            filter->link(queue);
            queue->link(app_vidconv);
            app_vidconv->link(sink);

            QGst::VideoOverlayPtr overlay = sink.dynamicCast<QGst::VideoOverlay>();
            overlay->setWindowHandle(this->winId());

            QGst::BusPtr bus = m_pipeline->bus();
            bus->addSignalWatch();
            QGlib::connect(bus, "message", this, &CameraRtp::onBusMessage);
            return;

            // udp out
            udp_vidconv = QGst::ElementFactory::make("nvvidconv");
            if(!udp_vidconv) { qCritical() << "Failed to create the udp_vidconv"; return; }
            m_pipeline->add(udp_vidconv);

            QGst::ElementPtr h264enc = QGst::ElementFactory::make("omxh264enc");
            if(!h264enc) { qCritical() << "Failed to create the omxh264enc"; return; }
            m_pipeline->add(h264enc);

            QGst::ElementPtr caps = QGst::ElementFactory::make("capsfilter");
            if(!caps) { qCritical() << "Failed to create the caps"; return; }
            caps->setProperty("caps",
                QGst::Caps::fromString("video/x-h264,stream-format=byte-stream,framerate=(fraction)25/1"));
            m_pipeline->add(caps);

            QGst::ElementPtr h264parse = QGst::ElementFactory::make("h264parse");
            if(!h264parse) { qCritical() << "Failed to create the h264parse"; return; }
            m_pipeline->add(h264parse);

            QGst::ElementPtr rtph264pay = QGst::ElementFactory::make("rtph264pay");
            if(!rtph264pay) { qCritical() << "Failed to create the rtph264pay"; return; }
            m_pipeline->add(rtph264pay);

            udpsink = QGst::ElementFactory::make("udpsink");
            if(!udpsink) { qCritical() << "Failed to create the udpsink"; return; }
            m_pipeline->add(udpsink);

            udp_vidconv->link(h264enc);
            h264enc->link(caps);
            caps->link(h264parse);
            //h264enc->link(h264parse);
            h264parse->link(rtph264pay);
            rtph264pay->link(udpsink);

            // final link
            tee->getRequestPad("src_%u")->link(udp_vidconv->getStaticPad("sink"));

            h264enc->setProperty("insert-sps-pps", true);
            h264enc->setProperty("insert-aud", true);
            h264enc->setProperty("insert-vui", true);
            h264enc->setProperty("profile", 1); // baseline

            rtph264pay->setProperty("send-config", true);
            rtph264pay->setProperty("config-interval", 1);

//            udpsink->setProperty("auto-multicast", true);
//            udpsink->setProperty("sync", false);
            udpsink->setProperty("multicast-iface", "eth0");
            udpsink->setProperty("host", "224.1.1.1");
            udpsink->setProperty("port", 5004);

            //QGst::BusPtr bus = m_pipeline->bus();
            //bus->addSignalWatch();
            //QGlib::connect(bus, "message", this, &CameraRtp::onBusMessage);
        } else {
            qCritical() << "Failed to create the pipeline";
        }
    }
}

void CameraRtp::set_channel(quint32 channel)
{
    if((1 <= channel) && (channel <= 4))
    {
        camerasrc->setProperty("sensor-id", 1);
        DEBUG_MSG(QString("CameraRtp: sensor-id = 1"));
    }
    else if((5 <= channel) && (channel <= 8))
    {
        camerasrc->setProperty("sensor-id", 0);
        DEBUG_MSG(QString("CameraRtp: sensor-id = 0"));
    }

    return;
}

void CameraRtp::set_property(const QString &property, int value)
{
    if (m_pipeline) {
        camerasrc->setProperty(property.toUtf8().data(), QGlib::Value(value));

        qDebug() << "setProperty: " << property << " / " << value;

        DEBUG_MSG(QString("CameraRtp: set value %1 / %2").arg(property).arg(value));
    }
}

void CameraRtp::set_property(const QString &property, float value)
{
    if (m_pipeline) {
        camerasrc->setProperty(property.toUtf8().data(), QGlib::Value(value));

        qDebug() << "setProperty: " << property << " / " << value;

        DEBUG_MSG(QString("CameraRtp: set value %1 / %2").arg(property).arg(value));
    }
}

void CameraRtp::set_property(const QString &property, const QString &value)
{
    if (m_pipeline) {
        camerasrc->setProperty(property.toUtf8().data(), QGlib::Value(value));

        qDebug() << "setProperty: " << property << " / " << value;

        DEBUG_MSG(QString("CameraRtp: set value %1 / %2").arg(property).arg(value));
    }
}

void CameraRtp::getProperty(const QString &property, int * result, bool * ok)
{
    if (m_pipeline) {
        QGlib::Value value = camerasrc->property(property.toUtf8().data());

        QByteArray array = value.toByteArray(ok);
        if(ok)
        {
            qDebug() << "array: " << array.count() << " / " << array;
        }
        else
        {
            qDebug() << "can not change to byte array";
            return;
        }

        *result = value.toInt(ok);

        if(*ok)
        {
            qDebug() << "getProperty: " << property << " / " << *result;

            DEBUG_MSG(QString("CameraRtp: get value %1 / %2").arg(property).arg(*result));
        }
    }
}

void CameraRtp::getProperty(const QString &property, float * result, bool * ok)
{
    if (m_pipeline) {
        QGlib::Value value = camerasrc->property(property.toUtf8().data());

        qDebug() << "type: " << value.type();

        QString temp = value.toString(ok);

        if(*ok)
        {
            *result = temp.toFloat();

            qDebug() << "getProperty: " << property << " / "
                     << *result;

            DEBUG_MSG(QString("CameraRtp: get value %1 / %2").arg(property).arg(*result));
        }
    }
}

void CameraRtp::getProperty(const QString &property, QString * result, bool * ok)
{
    if (m_pipeline) {
        QGlib::Value value = camerasrc->property(property.toUtf8().data());

        qDebug() << "type: " << value.type();

        *result = value.toString(ok);

        if(*ok)
        {
            qDebug() << "getProperty: " << property << " / " << *result;

            DEBUG_MSG(QString("CameraRtp: get value %1 / %2")
                               .arg(property).arg(*result));
        }
    }
}

void CameraRtp::set_orientation(int value)
{
    if (m_pipeline) {

        QGst::State current_state = this->state();

        if(current_state == QGst::StatePlaying)
        {
            m_pipeline->setState(QGst::StateReady);

            app_vidconv->setProperty("flip-method", QGlib::Value(value));
            udp_vidconv->setProperty("flip-method", QGlib::Value(value));

            m_pipeline->setState(QGst::StatePlaying);
        }
        else
        {
            app_vidconv->setProperty("flip-method", QGlib::Value(value));
            udp_vidconv->setProperty("flip-method", QGlib::Value(value));
        }

        DEBUG_MSG(QString("CameraRtp: set orientation %1").arg(value));
    }
}

QGst::State CameraRtp::state() const
{
    return m_pipeline ? m_pipeline->currentState() : QGst::StateNull;
}

void CameraRtp::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

    QWidget::paintEvent(event);
}

void CameraRtp::play()
{
    if (m_pipeline) {
        QGst::State current = this->state();

        if(current != QGst::StatePlaying)
        {
            //QGlib::Value addr = udpsink->property("host");
            //QGlib::Value port = udpsink->property("port");

            m_pipeline->setState(QGst::StatePlaying);
            //DEBUG_MSG(QString("CameraRtp: rtp server start [ %1 / %2 ]").arg(addr.toString()).arg(port.toInt()));
        }
    }
}

void CameraRtp::pause()
{
    if (m_pipeline) {
        m_pipeline->setState(QGst::StatePaused);
    }
}

void CameraRtp::stop()
{
    if (m_pipeline) {
        camerasrc->setState(QGst::StateNull);
        DEBUG_MSG("CameraRtp: rtp server stop");

        //once the pipeline stops, the bus is flushed so we will
        //not receive any StateChangedMessage about this.
        //so, to inform the ui, we have to emit this signal manually.
        Q_EMIT stateChanged();

        this->repaint();
        m_pipeline->setState(QGst::StateNull);
    }
}

void CameraRtp::onBusMessage(const QGst::MessagePtr & message)
{
    qDebug() << "CameraRtp: onBusMessage " << message->typeName();
    switch (message->type()) {
    case QGst::MessageTag:
        break;
    case QGst::MessageEos: //End of stream. We reached the end of the file.
        DEBUG_MSG(QString("CameraRtp: rtp EOS message "));
        stop();
        break;
    case QGst::MessageWarning:
        DEBUG_MSG(QString("CameraRtp: rtp warning occured"));
        //stop();
        break;
    case QGst::MessageQos:
        DEBUG_MSG(QString("CameraRtp: rtp qos occured"));
        //stop();
        break;
    case QGst::MessageError: //Some error occurred.
        DEBUG_MSG(QString("CameraRtp: rtp error occured - ").
                  append(message.staticCast<QGst::ErrorMessage>()->error().message()));
        //stop();
        break;
    case QGst::MessageStateChanged: //The element in message->source() has changed state
        if (message->source() == m_pipeline) {
            handlePipelineStateChange(message.staticCast<QGst::StateChangedMessage>());
        }
        break;
    default:
        break;
    }
}

void CameraRtp::handlePipelineStateChange(const QGst::StateChangedMessagePtr & scm)
{
    switch (scm->newState()) {
    case QGst::StatePlaying:
        //start the timer when the pipeline starts playing
        //m_positionTimer.start(100);
        break;
    case QGst::StatePaused:
        //stop the timer when the pipeline pauses
        if(scm->oldState() == QGst::StatePlaying) {
            //m_positionTimer.stop();
        }
        break;
    default:
        break;
    }

    Q_EMIT stateChanged();
}

#include "moc_camerartp.cpp"

#else

#include "camerartp.h"
#include "moc_camerartp.cpp"

#endif
