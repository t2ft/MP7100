// ***************************************************************************
// generic t2ft Qt Application
// ---------------------------------------------------------------------------
// mainwidget.cpp
// main application widget
// ---------------------------------------------------------------------------
// Copyright (C) 2021 by t2ft - Thomas Thanner
// Waldstrasse 15, 86399 Bobingen, Germany
// thomas@t2ft.de
// ---------------------------------------------------------------------------
// 2021-06-07  tt  Initial version created
// ---------------------------------------------------------------------------

#include "mainwidget.h"
#include "ui_mainwidget.h"
#include "tapp.h"
#include "tmessagehandler.h"
#include "silentcall.h"
#include <QDebug>
#include <QTimer>
#include <QSettings>
#include <QTimerEvent>
#include <QMessageBox>
#include <QSettings>
#include "mp7100.h"

#define GRP_MP7100          "MP7100_Config"
#define CFG_ALWAYS_ON_TOP   "alwaysOnTop"
#define CFG_LOG_FONT_SIZE   "logFont"


// expect a successful new measurement at least every second
#define UPDATE_MS   250
#define WATCHDOG_MS 5000

MainWidget::MainWidget(QWidget *parent)
    : TMainWidget(parent)
    , ui(new Ui::MainWidget)
    , m_lastCommandErrorRequest(false)
    , m_dev(nullptr)
    , m_state(Uninitialized)
    , m_idUpdateTimer(0)
    , m_idWatchdogTimer(0)
    , m_setOnOff(false)
    , m_setVA(false)
    , m_setVoltageChanged(false)
    , m_setCurrentChanged(false)
    , m_indicatorCount(0)
    , m_indicatorInc(8)
{
    ui->setupUi(this);
    QSettings cfg;
    cfg.beginGroup(GRP_MP7100);
    ui->alwaysOnTop->setChecked(cfg.value(CFG_ALWAYS_ON_TOP, false).toBool());
    setWindowFlag(Qt::WindowStaysOnTopHint, ui->alwaysOnTop->isChecked());
    QFont f = ui->textMessage->document()->defaultFont();
    f.setPointSizeF(cfg.value(CFG_LOG_FONT_SIZE, f.pointSizeF()).toReal());
    ui->textMessage->document()->setDefaultFont(f);
    cfg.endGroup();

    // allow debug message display
    connect(reinterpret_cast<TApp*>(qApp)->msgHandler(), SIGNAL(messageAdded(QString)), this, SLOT(on_messageAdded(QString)));

    // handle power events
    connect(this, &MainWidget::ResumeSuspend, this, &MainWidget::onResume);
    connect(this, &MainWidget::Suspend, this, &MainWidget::onSuspend);

    // setup resources
    QFontDatabase::addApplicationFont(":/res/LCDM2B__.TTF");
    QFontDatabase::addApplicationFont(":/res/LCDMB___.TTF");

    // set custom widget fonts
    QFont fontLCD = QFont("LCDMono", 32);
    QFont fontLCDsmall = QFont("LCDMono2", 16);
    ui->measuredVolts->setFont(fontLCD);
    ui->measuredAmps->setFont(fontLCD);
    ui->setVolts->setFont(fontLCDsmall);
    ui->setAmps->setFont(fontLCDsmall);
    ui->setVolts->setStyleSheet("color:white;");
    ui->setAmps->setStyleSheet("color:white;");
    ui->CC_CV->setFont(fontLCDsmall);
    reconnectDevice();
}

void MainWidget::startDevice()
{
    // check if device is available
    if ((m_dev!=nullptr) && (!m_dev->isValid())) {
        QMessageBox::critical(this, qApp->applicationDisplayName(), tr("No device or cannot open serial port"));
        qCritical() << "No device or cannot open serial port";
        close();
    }
    // start regular operations
    m_idUpdateTimer = startTimer(UPDATE_MS, Qt::PreciseTimer);
    triggerWatchdog();
    // prevent uncontrolled power down
}

MainWidget::~MainWidget()
{
    qDebug() << "MainWidget::~MainWidget()";
    // save log window font size to restore zoom level on next start
    QSettings cfg;
    cfg.beginGroup(GRP_MP7100);
    qreal s = ui->textMessage->document()->defaultFont().pointSizeF();
    cfg.setValue(CFG_LOG_FONT_SIZE, s);
    cfg.endGroup();
    delete m_dev;
    delete ui;
}

void MainWidget::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_idUpdateTimer) {
//        qDebug() << "+++ MainWidget::timerEvent() +++";
//        qDebug() << "      flags =" << Qt::hex << m_flags;
        if (m_setOnOff) {
            qInfo() << "      -> set on/off to" << (m_newOnOff ? "ON" : "OFF");
            m_setOnOff = !m_dev->setOnOff(m_newOnOff);
        } else if (m_setVA) {
            qInfo() << "      -> set voltage to" << m_newVoltage << "V, current to" << m_newCurrent << "A";
            m_setVA = !m_dev->setVoltageCurrent(m_newVoltage, m_newCurrent);
            if (!m_setVA) {
                m_setVoltageChanged = false;
                m_setCurrentChanged = false;
                ui->setVolts->setStyleSheet("color:white;");
                ui->setAmps->setStyleSheet("color:white;");
            }
        } else {
            switch (m_state) {
            case Uninitialized:
                m_state = MinimumVoltageCurrentWaiting;
                m_dev->getMinimumVoltageCurrent();
                break;
            case MinimumVoltageCurrentReceived:
                m_state = MaximumVoltageCurrentWaiting;
                m_dev->getMaximumVoltageCurrent();
                break;
            case MaximumVoltageCurrentReceived:
            case SetVoltageCurrentReceived:
                m_state = GetOnOffWaiting;
                m_dev->getOnOff();
                break;
            case GetOnOffReceived:
                m_state = DisplayVoltageCurrentWaiting;
                m_dev->getDisplayVoltageCurrent();
                break;
            case DisplayVoltageCurrentReceived:
                m_state = SetVoltageCurrentWaiting;
                m_dev->getSetVoltageCurrent();
                break;
            default:
                break;
            }
        }
//        qDebug() << "--- MainWidget::timerEvent() ---";
    } else if (event->timerId() == m_idWatchdogTimer) {
        if (m_state!=Uninitialized) {
            qWarning() << "Watchdog Timeout!";
        }
        updateIndicator(false);
        reconnectDevice();
    }
}

void MainWidget::on_messageAdded(const QString &msg)
{
    QTextCursor cursor = ui->textMessage->cursorForPosition(QPoint(0,1));
    QStringList sl = msg.split(']');
    QString lineTag = sl.takeFirst() + "]";
    QString lineText;
    while (!sl.isEmpty())
        lineText += sl.takeFirst();
    bool bold =  lineText.contains("send:");
    bool italics = lineText.contains("GUI:", Qt::CaseInsensitive);
    QString lineColor, tagColor;

    // format qDebug() generated lines
    if (lineText.contains("DBUG")) {
        // do not add debug lines
        return;
    } else if (lineText.contains("INFO")) {
    // format qInfo() generated lines
        tagColor = bold ? "black" : "grey";
        lineColor = "black";
        if (lineText.contains("\'BE\'")) {
            if (bold) {
                m_lastCommandErrorRequest = true;
            } else {
                if (!m_lastCommandErrorRequest) {
                    tagColor = "chocolate";
                    lineColor = "chocolate";
                }
                m_lastCommandErrorRequest = false;
            }
        } else if (lineText.contains("MCP connected", Qt::CaseInsensitive)) {
            tagColor = "darkgreen";
            lineColor = "green";
        } else if (lineText.contains("MCP disconnected", Qt::CaseInsensitive)) {
            tagColor = "maroon";
            lineColor = "brown";
        }
    } else if (lineText.contains("WARN")) {
    // format qWarning() generated lines
        tagColor = bold ? "mediumblue" : "royalblue";
        lineColor = "mediumblue";
        m_lastCommandErrorRequest = false;
    } else if (lineText.contains("CRIT")) {
    // format qCritical() generated lines
        tagColor = bold ? "firebrick" : "indianred";
        lineColor = "firebrick";
        m_lastCommandErrorRequest = false;
    } else if (lineText.contains("FATL")) {
    // format qFatal() generated lines
        tagColor = bold ? "darkviolet" : "blueviolet";
        lineColor = "darkviolet";
        m_lastCommandErrorRequest = false;
    } else {
    // format all other lines
        tagColor = bold ? "gray" : "darkgray";
        lineColor = "gray";
        m_lastCommandErrorRequest = false;
    }
    // generate HTML code for this line
    QString line = "<div><span style=\"color:" + tagColor + ";\">" + lineTag + "</span>" \
            "<span style=\"color:" + lineColor + ";font-weight:" + QString(bold ? "bold" : "regular") + ";\">" +
            QString(italics ? "<i>" : "") + lineText.toHtmlEscaped() + QString(italics ? "</i>" : "") + "</span></div>";
    ui->textMessage->appendHtml(line);
    // ensure last line is visible
    cursor.movePosition(QTextCursor::End);
    cursor.movePosition(QTextCursor::StartOfLine);
    ui->textMessage->setTextCursor(cursor);
    ui->textMessage->ensureCursorVisible();
}

void MainWidget::setDisplayVoltageCurrent(double u, double i, bool cc, bool ok)
{
    m_state = DisplayVoltageCurrentReceived;
    if (ok) {
        triggerWatchdog();
        ui->measuredVolts->setText(QString("%1 V").arg(u, 5, 'f', 2, QLatin1Char('0')));
        ui->measuredAmps->setText(QString("%1 A").arg(i, 5, 'f', 3, QLatin1Char('0')));
        ui->CC_CV->setText(cc ? "CC" : "CV");
    }
}

void MainWidget::setMinimumVoltageCurrent(double u, double i, bool ok)
{
    m_state = MinimumVoltageCurrentReceived;
    if (ok) {
        triggerWatchdog();
        qInfo() << "minimum voltage:" << u << "V";
        qInfo() << "minimum current:" << i << "A";
        SilentCall(ui->setVolts)->setMinimum(u);
        SilentCall(ui->setAmps)->setMinimum(u);
    } else {
        qWarning() << "minimum voltage: FAILED";
        qWarning() << "minimum current: FAILED";
    }
}

void MainWidget::setMaximumVoltageCurrent(double u, double i, bool ok)
{
    m_state = MaximumVoltageCurrentReceived;
    if (ok) {
        triggerWatchdog();
        qInfo() << "maximum voltage:" << u << "V";
        qInfo() << "maximum current:" << i << "A";
        SilentCall(ui->setVolts)->setMaximum(u);
        SilentCall(ui->setAmps)->setMaximum(i);
    } else {
        qWarning() << "maximum voltage: FAILED";
        qWarning() << "maximum current: FAILED";
    }
}

void MainWidget::setVoltageCurrentSet(double u, double i, bool ok)
{
    m_state = SetVoltageCurrentReceived;
    if (ok) {
        triggerWatchdog();
        if (!m_setVoltageChanged) {
            SilentCall(ui->setVolts)->setValue(u);
        }
        if (!m_setCurrentChanged) {
            SilentCall(ui->setAmps)->setValue(i);
        }
    }
}

void MainWidget::setOnOff(bool on, bool ok)
{
    m_state = GetOnOffReceived;
    if (ok) {
        triggerWatchdog();
        if (!m_setOnOff) {
            SilentCall(ui->onoff)->setChecked(on);
            setOnOffText(on);
        }
    }
}

void MainWidget::on_onoff_toggled(bool checked)
{
    m_setOnOff = true;
    m_newOnOff = checked;
    qInfo() << "switch " << (checked ? "ON" : "OFF");
    setOnOffText(checked);
}


void MainWidget::on_setVA_clicked()
{
    m_newVoltage = ui->setVolts->value();
    m_newCurrent = ui->setAmps->value();
    m_setVA = true;
    qInfo() << "set voltage to" << m_newVoltage << "V";
    qInfo() << "set current to" << m_newCurrent << "A";
}

void MainWidget::on_setVolts_valueChanged(double x)
{
    Q_UNUSED(x)
    m_setVoltageChanged = true;
    ui->setVolts->setStyleSheet("color:red;");
}


void MainWidget::on_setAmps_valueChanged(double x)
{
    Q_UNUSED(x)
    m_setCurrentChanged = true;
    ui->setAmps->setStyleSheet("color:red;");
}

void MainWidget::updateIndicator(bool connected)
{
    const int maxIndicatorCount = 64;
    ui->indicator->setText(connected ? tr("connected") : tr("Error"));
    ui->indicator->setStyleSheet( QString("color:%1;font-weight:bold ;background:%2;border-radius:15px;border-style:solid;border-width:4px;border-color:%3;")
                                     .arg(QColor::fromHsv(connected ? 120 : 0, 200, 128).name())
                                     .arg(QColor::fromHsv(connected ? 120 : 0, 128, (255-maxIndicatorCount/4)+m_indicatorCount/4).name())
                                     .arg(QColor::fromHsv(connected ? 120 : 0, 255, (255-2*maxIndicatorCount)+m_indicatorCount).name())
    );
    m_indicatorCount +=m_indicatorInc;
    if ((m_indicatorCount > maxIndicatorCount) || (m_indicatorCount < 0)) {
        m_indicatorInc = -m_indicatorInc;
        m_indicatorCount +=m_indicatorInc;
    }
}

void MainWidget::setOnOffText(bool on)
{
    ui->measuredAmps->setStyleSheet(on ? "color:yellow" : "color:darkgrey");
    ui->measuredVolts->setStyleSheet(on ? "color:yellow" : "color:darkgrey");
    ui->CC_CV->setStyleSheet(on ? "color:yellow" : "color:darkgrey");
    ui->onoff->setText(on ? tr("ON / off") : tr("on / OFF"));
    QString name = on ? ":/res/power_on.svg" : ":/res/power_off.svg";
    ui->output->load(name);
}

void MainWidget::reconnectDevice()
{
    disconnectDevice();
    connectDevice();
}

void MainWidget::disconnectDevice()
{
    delete m_dev;
    m_state = Uninitialized;
    killTimer(m_idUpdateTimer);
    m_idUpdateTimer = 0;
    killTimer(m_idWatchdogTimer);
    m_idWatchdogTimer = 0;
}

void MainWidget::connectDevice()
{
    m_dev = new MP7100(this);
    connect(m_dev, &MP7100::displayVoltageCurrentGet, this, &MainWidget::setDisplayVoltageCurrent);
    connect(m_dev, &MP7100::minimumVoltageCurrentGet, this, &MainWidget::setMinimumVoltageCurrent);
    connect(m_dev, &MP7100::maximumVoltageCurrentGet, this, &MainWidget::setMaximumVoltageCurrent);
    connect(m_dev, &MP7100::setVoltageCurrentGet, this, &MainWidget::setVoltageCurrentSet);
    connect(m_dev, &MP7100::onoffGet, this, &MainWidget::setOnOff);
    QTimer::singleShot(250, this, &MainWidget::startDevice);
}


void MainWidget::triggerWatchdog()
{
    killTimer(m_idWatchdogTimer);
    qDebug() << "   -> trigger watchdog";
    m_idWatchdogTimer = startTimer(WATCHDOG_MS);
}


void MainWidget::on_alwaysOnTop_toggled(bool checked)
{
    qDebug() << "always on top =" << checked;
    QSettings cfg;
    cfg.beginGroup(GRP_MP7100);
    cfg.setValue(CFG_ALWAYS_ON_TOP, checked);
    cfg.endGroup();
    setWindowFlag(Qt::WindowStaysOnTopHint, checked);
    show();
}

void MainWidget::onSuspend()
{
    qInfo() << "suspending DP700 communications";
    disconnectDevice();
}

void MainWidget::onResume()
{
    qInfo() << "resuming DP700 communications";
    connectDevice();
}
