#ifndef APPSOFTWARE_H
#define APPSOFTWARE_H

#include "iso14229.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
} UDSAppConfig;

typedef struct {
    Iso14229Instance iso14229;
} UDSAppInstance;

int udsAppInit(UDSAppInstance *self, const UDSAppConfig *cfg);

#endif
