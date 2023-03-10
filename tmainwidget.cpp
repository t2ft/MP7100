// ***************************************************************************
// General Support Classes
// ---------------------------------------------------------------------------
// tmainwidget.cpp
// QWidget with additional features to implement standard main widget behavior
// ---------------------------------------------------------------------------
// Copyright (C) 2021 by t2ft - Thomas Thanner
// Waldstrasse 15, 86399 Bobingen, Germany
// thomas@t2ft.de
// ---------------------------------------------------------------------------
// 2021-6-7  tt  Initial version created
// ---------------------------------------------------------------------------

#include "tmainwidget.h"
#include "tpowereventfilter.h"
#include <QSettings>
#include <QTimer>
#include <QSplitter>
#include <QDebug>
#include <QAbstractEventDispatcher>
#include <windows.h>

#define CFG_GRP_WINDOW  "TWindow"
#define CFG_GEOMETRY    "geometry"
#define CFG_MAXIMIZED   "maximized"


TMainWidget::TMainWidget(QWidget *parent)
    : QWidget(parent)
{
    qDebug() << "+++ TMainWidget::TMainWidget(QWidget *parent)";
    // allow us to dispatch windows power events
    TPowerEventFilter *pFilter = new TPowerEventFilter(this);
    QAbstractEventDispatcher::instance()->installNativeEventFilter(pFilter);
    connect(pFilter, &TPowerEventFilter::PowerStatusChange, this, &TMainWidget::PowerStatusChange);
    connect(pFilter, &TPowerEventFilter::ResumeAutomatic, this, &TMainWidget::ResumeAutomatic);
    connect(pFilter, &TPowerEventFilter::ResumeSuspend, this, &TMainWidget::ResumeSuspend);
    connect(pFilter, &TPowerEventFilter::Suspend, this, &TMainWidget::Suspend);
    QTimer::singleShot(1, this, SLOT(updateGeometry()));
    qDebug() << "--- TMainWidget::TMainWidget(QWidget *parent)";
}

void TMainWidget::closeEvent(QCloseEvent *event)
{
    qDebug() << "+++ TMainWidget::closeEvent(QCloseEvent *event)";
    QSettings cfg;
    cfg.beginGroup(CFG_GRP_WINDOW);
    cfg.setValue(CFG_GEOMETRY, saveGeometry());
    cfg.setValue(CFG_MAXIMIZED, isMaximized());
    // save state of all QSplitter children
    for (auto &child : findChildren<QSplitter*>()) {
        qDebug() << "saveState for " << child->objectName();
        cfg.setValue(child->objectName(), child->saveState());
    }
    cfg.endGroup();
    QWidget::closeEvent(event);
    qDebug() << "--- TMainWidget::closeEvent(QCloseEvent *event)";
}

void TMainWidget::preventSuspend(bool prevent)
{
#ifdef WIN32
    if (prevent) {
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);
    } else {
        SetThreadExecutionState(ES_CONTINUOUS);
    }
#else
    QWarning() << "TMainWidget::preventSuspend works on Windows only!";
#endif
}

void TMainWidget::updateGeometry()
{
    qDebug() << "+++ TMainWidget::updateGeometry()";
    QSettings cfg;
    cfg.beginGroup(CFG_GRP_WINDOW);
    restoreGeometry(cfg.value(CFG_GEOMETRY).toByteArray());
    // restore state for all QSplitter children
    for (auto &child : findChildren<QSplitter*>()) {
        qDebug() << "restoreState for " << child->objectName();
        child->restoreState(cfg.value(child->objectName()).toByteArray());
    }
    if (cfg.value(CFG_MAXIMIZED, false).toBool())
            QTimer::singleShot(1, this, SLOT(showMaximized()));
    cfg.endGroup();
    qDebug() << "--- TMainWidget::updateGeometry()";
}
