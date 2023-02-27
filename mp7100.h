// ***************************************************************************
// MP7100xx power supply serial control tool
// ---------------------------------------------------------------------------
// mp7100.h
// instrument communication class, header file
// ---------------------------------------------------------------------------
// Copyright (C) 2023 by t2ft - Thomas Thanner
// Waldstrasse 15, 86399 Bobingen, Germany
// thomas@t2ft.de
// ---------------------------------------------------------------------------
// 2023-2-27  tt  Initial version created
// ***************************************************************************
#ifndef MP7100_H
#define MP7100_H

#include <QObject>
#include "serdev.h"
#include <QMutex>

class MP7100 : public SerDev
{
    Q_OBJECT
public:
    explicit MP7100(QObject *parent = nullptr);

public slots:
    bool setOnOff(bool on);
    bool getOnOff();
    bool setVoltageCurrent(double u, double i);
    bool getDisplayVoltageCurrent();
    bool getSetVoltageCurrent();
    bool getMinimumVoltageCurrent();
    bool getMaximumVoltageCurrent();

signals:
    void onoffSet(bool ok);
    void onoffGet(bool on, bool ok);
    void voltageCurrentSet(bool ok);
    void displayVoltageCurrentGet(double u, double i, bool cc, bool ok);
    void setVoltageCurrentGet(double u, double i, bool ok);
    void minimumVoltageCurrentGet(double u, double i, bool ok);
    void maximumVoltageCurrentGet(double u, double i, bool ok);

protected:
    void decodeBuffer(QByteArray &buffer) override;
    void decodeCommand(const QByteArray &buffer, bool timeout);
    void timerEvent(QTimerEvent *event) override;

private:
    typedef enum {
        Idle,
        SetOnOff,
        GetOnOff,
        GetOnOffFinal,
        SetVoltageCurrent,
        GetDisplayVoltageCurrent,
        GetDisplayVoltageCurrentFinal,
        GetSetVoltageCurrent,
        GetSetVoltageCurrentFinal,
        GetMinimumVoltageCurrent,
        GetMinimumVoltageCurrentFinal,
        GetMaximumVoltageCurrent,
        GetMaximumVoltageCurrentFinal
    } STATE;

    bool sendCommand(const QByteArray &cmd, STATE currentState, STATE newState);

    QMutex      m_lock;
    STATE       m_state;
    double      m_U, m_I;
    bool        m_On, m_CC;
    int         m_idTimer;
};

#endif // MP7100_H
