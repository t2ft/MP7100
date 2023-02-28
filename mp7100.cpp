// ***************************************************************************
// MP7100xx power supply serial control tool
// ---------------------------------------------------------------------------
// mp7100.cpp
// instrument communication class
// ---------------------------------------------------------------------------
// Copyright (C) 2023 by t2ft - Thomas Thanner
// Waldstrasse 15, 86399 Bobingen, Germany
// thomas@t2ft.de
// ---------------------------------------------------------------------------
// 2023-2-27  tt  Initial version created
// ***************************************************************************
#include "mp7100.h"
#include <QMutexLocker>
#include <QDebug>
#include <QTimerEvent>

MP7100::MP7100(QObject *parent)
    : SerDev("COM12", 9600, parent)
    , m_state(Idle)
    , m_U(0.)
    , m_I(0.)
    , m_On(false)
    , m_CC(false)
    , m_idTimer(0)
{
}


bool MP7100::setOnOff(bool on)
{
    return sendCommand(QString("SOUT%1").arg(on ? "1" : "0").toLatin1(), Idle, SetOnOff);
}

bool MP7100::getOnOff()
{
    return sendCommand(QString("GOUT").toLatin1(), Idle, GetOnOff);
}

bool MP7100::setVoltageCurrent(double u, double i)
{
    return sendCommand(QString("SETD%1%2")
                           .arg(static_cast<unsigned>(u*100),  4, 10, QLatin1Char('0'))
                           .arg(static_cast<unsigned>(i*1000), 4, 10, QLatin1Char('0')).toLatin1(),
                       Idle, SetVoltageCurrent);
}

bool MP7100::getDisplayVoltageCurrent()
{
    return sendCommand(QString("GETD").toLatin1(), Idle, GetDisplayVoltageCurrent);
}

bool MP7100::getSetVoltageCurrent()
{
    return sendCommand(QString("GETS").toLatin1(), Idle, GetSetVoltageCurrent);
}

bool MP7100::getMinimumVoltageCurrent()
{
    return sendCommand(QString("GMIN").toLatin1(), Idle, GetMinimumVoltageCurrent);
}

bool MP7100::getMaximumVoltageCurrent()
{
    return sendCommand(QString("GMAX").toLatin1(), Idle, GetMaximumVoltageCurrent);
}

void MP7100::decodeBuffer(QByteArray &buffer)
{
    // check if there is a reply terminator in the received data
    int inx;
    while ((inx = buffer.indexOf('\r')) >= 0) {
        decodeCommand(buffer.left(inx), false);
        buffer = buffer.mid(inx+1);
    }
}

void MP7100::decodeCommand(const QByteArray &buffer, bool timeout)
{
//    qDebug() << "+++ MP7100::decodeCommand(buffer =" << buffer << ") +++";
//    qDebug() << "      m_state =" << m_state;
    if (!buffer.isEmpty()) {
        QList<QByteArray> params;

        switch(m_state) {
        case SetOnOff: {
            if (timeout) {
                emit onoffSet(false);
            } else {
                emit onoffSet(buffer.left(2)=="OK");
            }
            m_state = Idle;
            break;
        }
        case GetOnOff: {
            if (timeout) {
                emit onoffGet(false, false);
                m_state = Idle;
            } else {
                m_On = buffer[0]=='1';
                m_state = GetOnOffFinal;
            }
            break;
        }
        case GetOnOffFinal: {
            if (timeout) {
                emit onoffGet(false, false);
            } else {
                emit onoffGet(m_On, buffer.left(2)=="OK");
            }
            m_state = Idle;
            break;
        }
        case SetVoltageCurrent: {
            if (timeout) {
                emit voltageCurrentSet(false);
            } else {
                emit voltageCurrentSet(buffer.left(2)=="OK");
            }
            m_state = Idle;
            break;
        }
        case GetDisplayVoltageCurrent: {
            if (timeout) {
                emit displayVoltageCurrentGet(0., 0., false, false);
                m_state = Idle;
            } else {
                params = buffer.split(';');
                if (params.size()>0) {
                    m_U = params[0].toUInt()/100.;
                } else {
                    m_U = 0.;
                }
                if (params.size()>1) {
                    m_I = params[1].toUInt()/1000.;
                } else {
                    m_I = 0.;
                }
                if (params.size()>2) {
                    m_CC = params[2]=="1";
                } else {
                    m_CC = false;
                }
                m_state = GetDisplayVoltageCurrentFinal;
            }
            break;
        }
        case GetDisplayVoltageCurrentFinal: {
            if (timeout) {
                emit displayVoltageCurrentGet(0., 0., false, false);
            } else {
                emit displayVoltageCurrentGet(m_U, m_I, m_CC, buffer.left(2)=="OK");
            }
            m_state = Idle;
            break;
        }

        case GetSetVoltageCurrent: {
            if (timeout) {
                emit setVoltageCurrentGet(0., 0., false);
                m_state = Idle;
            } else {
                params = buffer.split(';');
                if (params.size()>0) {
                    m_U = params[0].toUInt()/100.;
                } else {
                    m_U = 0.;
                }
                if (params.size()>1) {
                    m_I = params[1].toUInt()/1000.;
                } else {
                    m_I = 0.;
                }
                m_state = GetSetVoltageCurrentFinal;
            }
            break;
        }
        case GetSetVoltageCurrentFinal: {
            if (timeout) {
                emit setVoltageCurrentGet(0., 0., false);
            } else {
                emit setVoltageCurrentGet(m_U, m_I, buffer.left(2)=="OK");
            }
            m_state = Idle;
            break;
        }

        case GetMinimumVoltageCurrent: {
            if (timeout) {
                emit minimumVoltageCurrentGet(0., 0., false);
                m_state = Idle;
            } else {
                params = buffer.split(';');
                if (params.size()>0) {
                    m_U = params[0].toUInt()/100.;
                } else {
                    m_U = 0.;
                }
                if (params.size()>1) {
                    m_I = params[1].toUInt()/1000.;
                } else {
                    m_I = 0.;
                }
                m_state = GetMinimumVoltageCurrentFinal;
            }
            break;
        }
        case GetMinimumVoltageCurrentFinal: {
            if (timeout) {
                emit minimumVoltageCurrentGet(0., 0., false);
            } else {
                emit minimumVoltageCurrentGet(m_U, m_I, buffer.left(2)=="OK");
            }
            m_state = Idle;
            break;
        }

        case GetMaximumVoltageCurrent: {
            if (timeout) {
                emit maximumVoltageCurrentGet(0., 0., false);
                m_state = Idle;
            } else {
                params = buffer.split(';');
                if (params.size()>0) {
                    m_U = params[0].toUInt()/100.;
                } else {
                    m_U = 0.;
                }
                if (params.size()>1) {
                    m_I = params[1].toUInt()/1000.;
                } else {
                    m_I = 0.;
                }
                m_state = GetMaximumVoltageCurrentFinal;
            }
            break;
        }
        case GetMaximumVoltageCurrentFinal: {
            if (timeout) {
                emit maximumVoltageCurrentGet(0., 0., false);
            } else {
                emit maximumVoltageCurrentGet(m_U, m_I, buffer.left(2)=="OK");
            }
            m_state = Idle;
            break;
        }

        default: {
            m_state = Idle;
            qWarning() << "      unexpected data received";
            break;
        }
        }
    }
    //    qDebug() << "--- MP7100::decodeCommand() ---";
}

void MP7100::timerEvent(QTimerEvent *event)
{
    if (m_idTimer == event->timerId()) {
        killTimer(m_idTimer);
        m_idTimer = 0;
        decodeCommand(QByteArray(), true);
    }
}


bool MP7100::sendCommand(const QByteArray &cmd, STATE currentState, STATE newState)
{
//    qDebug() << "+++ MP7100::sendCommand(cmd =" << cmd << "currentState =" << currentState << "newState =" << newState << ") +++";
//    qDebug() << "      m_state =" << m_state;
    QMutexLocker lock(&m_lock);
    if (m_idTimer!=0) {
        // stop current timeout
        killTimer(m_idTimer);
        m_idTimer = 0;
    }
    if (m_state!=currentState) {
        // create a timeout for any running command
        decodeCommand(QByteArray(), true);
    }
    m_state = newState;
    QByteArray txData(cmd);
    if (txData.right(1) != "\r")
        txData.append('\r');
    sendData(txData);
    // start a new timeout
    m_idTimer = startTimer(1000);

//    qDebug() << "--- MP7100::sendCommand() -> " << true << "---";
    return true;
}
