//
// Created by lucas on 13/03/2026.
//

#ifndef FIREBASE_AUTH_MANAGER_H
#define FIREBASE_AUTH_MANAGER_H

#include <Arduino.h>

namespace firebase_auth_manager
{
    bool signIn();
    bool refreshIdTokenIfNeeded();

    bool hasValidToken();
    const String &getIdToken();
}

#endif