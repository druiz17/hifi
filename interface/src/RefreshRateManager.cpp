//
//  RefreshRateManager.cpp
//  interface/src/
//
//  Created by Dante Ruiz on 2019-04-15.
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//


#include "RefreshRateManager.h"

#include <array>
#include <map>


#include <Application.h>

#include <display-plugins/hmd/HmdDisplayPlugin.h>

static const int HMD_TARGET_RATE = 90;

static const std::array<std::string, RefreshRateManager::RefreshRateProfile::PROFILE_NUM> REFRESH_RATE_PROFILE_TO_STRING =
    { { "Eco", "Interactive", "Realtime" } };

static const std::array<std::string, RefreshRateManager::RefreshRateRegime::REGIME_NUM> REFRESH_RATE_REGIME_TO_STRING =
    { { "Running", "Unfocus", "Minimized", "StartUp", "ShutDown" } };

static const std::array<std::string, RefreshRateManager::UXMode::UX_NUM> UX_MODE_TO_STRING =
    { { "Desktop", "HMD" } };

static const std::map<std::string, RefreshRateManager::RefreshRateProfile> REFRESH_RATE_PROFILE_FROM_STRING =
    { { "Eco", RefreshRateManager::RefreshRateProfile::ECO },
      { "Interactive", RefreshRateManager::RefreshRateProfile::INTERACTIVE },
      { "Realtime", RefreshRateManager::RefreshRateProfile::REALTIME } };

static const std::array<int, RefreshRateManager::RefreshRateProfile::PROFILE_NUM> RUNNING_REGIME_PROFILES =
    { { 5, 20, 60 } };

static const std::array<int, RefreshRateManager::RefreshRateProfile::PROFILE_NUM> UNFOCUS_REGIME_PROFILES =
    { { 5, 5, 10 } };

static const std::array<int, RefreshRateManager::RefreshRateProfile::PROFILE_NUM> MINIMIZED_REGIME_PROFILE =
    { { 2, 2, 2 } };

static const std::array<int, RefreshRateManager::RefreshRateProfile::PROFILE_NUM> START_AND_SHUTDOWN_REGIME_PROFILES =
    { { 30, 30, 30 } };

static const std::array<std::array<int, RefreshRateManager::RefreshRateProfile::PROFILE_NUM>, RefreshRateManager::RefreshRateRegime::REGIME_NUM> REFRESH_RATE_REGIMES =
    { { RUNNING_REGIME_PROFILES, UNFOCUS_REGIME_PROFILES, MINIMIZED_REGIME_PROFILE,
      START_AND_SHUTDOWN_REGIME_PROFILES, START_AND_SHUTDOWN_REGIME_PROFILES } };


std::string RefreshRateManager::refreshRateProfileToString(RefreshRateManager::RefreshRateProfile refreshRateProfile) {
    return REFRESH_RATE_PROFILE_TO_STRING.at(refreshRateProfile);
}

RefreshRateManager::RefreshRateProfile RefreshRateManager::refreshRateProfileFromString(std::string refreshRateProfile) {
    return REFRESH_RATE_PROFILE_FROM_STRING.at(refreshRateProfile);
}

std::string RefreshRateManager::refreshRateRegimeToString(RefreshRateManager::RefreshRateRegime refreshRateRegime) {
    return REFRESH_RATE_REGIME_TO_STRING.at(refreshRateRegime);
}

std::string RefreshRateManager::uxModeToString(RefreshRateManager::RefreshRateManager::UXMode uxMode) {
    return UX_MODE_TO_STRING.at(uxMode);
}

RefreshRateManager::RefreshRateManager() {
    _refreshRateProfile = (RefreshRateManager::RefreshRateProfile) _refreshRateMode.get();
    _inactiveTimer->setInterval(3000);
    _inactiveTimer->setSingleShot(true);
    QObject::connect(_inactiveTimer.get(), &QTimer::timeout, [&] {
        updateRefreshRateController();
    });
}

void RefreshRateManager::setRefreshRateProfile(RefreshRateManager::RefreshRateProfile refreshRateProfile) {
    if (_refreshRateProfile != refreshRateProfile) {
        _refreshRateModeLock.withWriteLock([&] {
            _refreshRateProfile = refreshRateProfile;
            _refreshRateMode.set((int) refreshRateProfile);
        });
        updateRefreshRateController();
    }
}

RefreshRateManager::RefreshRateProfile RefreshRateManager::getRefreshRateProfile() const {
    RefreshRateManager::RefreshRateProfile profile = RefreshRateManager::RefreshRateProfile::REALTIME;

    if (getUXMode() != RefreshRateManager::UXMode::HMD) {
        profile =(RefreshRateManager::RefreshRateProfile) _refreshRateModeLock.resultWithReadLock<int>([&] {
            return _refreshRateMode.get();
        });
    }

    return profile;
}

RefreshRateManager::RefreshRateRegime RefreshRateManager::getRefreshRateRegime() const {
    return getUXMode() == RefreshRateManager::UXMode::HMD ? RefreshRateManager::RefreshRateRegime::RUNNING :
        _refreshRateRegime;
}

void RefreshRateManager::setRefreshRateRegime(RefreshRateManager::RefreshRateRegime refreshRateRegime) {
    if (_refreshRateRegime != refreshRateRegime) {
        _refreshRateRegime = refreshRateRegime;
        updateRefreshRateController();

        if (refreshRateRegime == RefreshRateManager::RefreshRateRegime::RUNNING &&
            getRefreshRateProfile() == RefreshRateManager::RefreshRateProfile::ECO) {
            _inactiveTimer->start();
        }
    }

}

void RefreshRateManager::resetInactiveTimer() {
    if (getRefreshRateProfile() == RefreshRateManager::RefreshRateProfile::ECO &&
        _refreshRateRegime == RefreshRateManager::RefreshRateRegime::RUNNING) {
        _inactiveTimer->start();
        if (_refreshRateOperator) {
            _refreshRateOperator(20);
        }
    } else if (_inactiveTimer->isActive()) {
        _inactiveTimer->stop();
    }
}

void RefreshRateManager::setUXMode(RefreshRateManager::UXMode uxMode) {
    if (_uxMode != uxMode) {
        _uxMode = uxMode;
        updateRefreshRateController();
    }
}

void RefreshRateManager::updateRefreshRateController() const {
    if (_refreshRateOperator) {
        int targetRefreshRate;
        if (_uxMode == RefreshRateManager::UXMode::DESKTOP) {
            if (_refreshRateRegime == RefreshRateManager::RefreshRateRegime::RUNNING &&
                _refreshRateProfile == RefreshRateManager::RefreshRateProfile::INTERACTIVE) {
                targetRefreshRate = getInteractiveRefreshRate();
            } else {
                targetRefreshRate = REFRESH_RATE_REGIMES[_refreshRateRegime][_refreshRateProfile];
            }
        } else {
            targetRefreshRate = HMD_TARGET_RATE;
        }

        _refreshRateOperator(targetRefreshRate);
        _activeRefreshRate = targetRefreshRate;
    }
}

void RefreshRateManager::setInteractiveRefreshRate(int refreshRate) {
     _refreshRateLock.withWriteLock([&] {
         _interactiveRefreshRate.set(refreshRate);
     });
     updateRefreshRateController();
}


int RefreshRateManager::getInteractiveRefreshRate() const {
    return _refreshRateLock.resultWithReadLock<int>([&] {
        return _interactiveRefreshRate.get();
    });
}
