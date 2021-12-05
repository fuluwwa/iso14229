#include "appsoftware.h"
#include "iso14229.h"

int udsAppInit(UDSAppInstance *self, const UDSAppConfig *cfg) {
    iso14229UserEnableService(&self->iso14229, kSID_DIAGNOSTIC_SESSION_CONTROL);
    iso14229UserEnableService(&self->iso14229, kSID_ECU_RESET);
    iso14229UserEnableService(&self->iso14229, kSID_READ_DATA_BY_IDENTIFIER);
    iso14229UserEnableService(&self->iso14229, kSID_WRITE_DATA_BY_IDENTIFIER);
    iso14229UserEnableService(&self->iso14229, kSID_TESTER_PRESENT);
    return 0;
}