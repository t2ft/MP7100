// ***************************************************************************
// generic t2ft Qt Application
// ---------------------------------------------------------------------------
// mainwidget.h
// main application widget, header file
// ---------------------------------------------------------------------------
// Copyright (C) 2021 by t2ft - Thomas Thanner
// Waldstrasse 15, 86399 Bobingen, Germany
// thomas@t2ft.de
// ---------------------------------------------------------------------------
// 2021-06-07  tt  Initial version created
// ---------------------------------------------------------------------------
#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include "tmainwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWidget; }
QT_END_NAMESPACE

class MP7100;

class MainWidget : public TMainWidget
{
    Q_OBJECT

public:
    MainWidget(QWidget *parent = nullptr);
    ~MainWidget();

protected:
    void timerEvent(QTimerEvent *event) override;


private slots:
    void startDevice();
    void on_messageAdded(const QString &msg);
    void setDisplayVoltageCurrent(double u, double i, bool cc, bool ok);
    void setMinimumVoltageCurrent(double u, double i, bool ok);
    void setMaximumVoltageCurrent(double u, double i, bool ok);
    void setVoltageCurrentSet(double u, double i, bool ok);
    void setOnOff(bool on, bool ok);
    void onSuspend();
    void onResume();

    void on_onoff_toggled(bool checked);
    void on_setVA_clicked();
    void on_setVolts_valueChanged(double x);
    void on_setAmps_valueChanged(double x);

    void updateIndicator(bool connected);

    void on_alwaysOnTop_toggled(bool checked);

private:
    Ui::MainWidget *ui;

    typedef enum {
        Uninitialized,
        MinimumVoltageCurrentWaiting,
        MinimumVoltageCurrentReceived,
        MaximumVoltageCurrentWaiting,
        MaximumVoltageCurrentReceived,
        DisplayVoltageCurrentWaiting,
        DisplayVoltageCurrentReceived,
        SetVoltageCurrentWaiting,
        SetVoltageCurrentReceived,
        GetOnOffWaiting,
        GetOnOffReceived,
    } State;

    void setOnOffText(bool on);
    void reconnectDevice();
    void disconnectDevice();
    void connectDevice();
    void triggerWatchdog();

    bool            m_lastCommandErrorRequest;
    MP7100          *m_dev;
    State           m_state;
    int             m_idUpdateTimer;
    int             m_idWatchdogTimer;
    bool            m_setOnOff;
    bool            m_newOnOff;
    bool            m_setVA;
    bool            m_setVoltageChanged;
    bool            m_setCurrentChanged;
    double          m_newVoltage;
    double          m_newCurrent;
    int             m_indicatorCount, m_indicatorInc;
};

#endif // MAINWIDGET_H
