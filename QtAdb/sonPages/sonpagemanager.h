#ifndef SONPAGEMANAGER_H
#define SONPAGEMANAGER_H

#include <QCoreApplication>   // Q_DECLARE_TR_FUNCTIONS / tr()
#include <QApplication>
#include <QHash>
#include <functional>
#include "textexplainer.h"
#include "devInfo/sp_dpichanger.h"
#include "devInfo/sp_wmsize.h"
#include "activator/sp_activator.h"
#include "apps/sp_installer.h"
#include "apps/sp_permissiongroups.h"
#include "apps/sp_permissions.h"
#include "apps/sp_packages.h"
#include "apps/sp_features.h"
#include "apps/sp_libraries.h"
#include "apps/sp_users.h"
#include "devControl/sp_devcontrol_power.h"
#include "devControl/btnemulator.h"
#include "recovery/sp_recovery.h"
#include "advanced/sp_customize_cmd.h"
#include "advanced/sp_accounts.h"
#include "advanced/sp_captive_portal.h"
#include "advanced/sp_scales.h"
#include "advanced/sp_status_bar.h"
#include "advanced/sp_vibrate.h"


class sonPageManager
{
    // 本类不是 QObject，但需要 tr()（Qt 官方推荐做法）
    Q_DECLARE_TR_FUNCTIONS(sonPageManager)

public:
    sonPageManager();
    ~sonPageManager();

    textExplainer *explainer = nullptr;


    animationWidget *selector(QWidget *parent, QString parentName, int key , device dev);

private:

    animationWidget* createSonPageFor_devInfo(QWidget *parent, int key, device dev);
    animationWidget* createSonPageFor_activator(QWidget *parent, int key, device dev);
    animationWidget* createSonPageFor_apps(QWidget *parent, int key, device dev);
    animationWidget* createSonPageFor_devControl(QWidget *parent, int key, device dev);
    animationWidget* createSonPageFor_recovery(QWidget *parent, int key, device dev);
    animationWidget* createSonPageFor_advanced(QWidget *parent, int key, device dev);
};

#endif // SONPAGEMANAGER_H
