.set noreorder

#include "pspimport.s"

IMPORT_START "SystemCtrlForUser", 0

IMPORT_FUNC "SystemCtrlForUser", 0x1090A2E1, sctrlHENGetVersion
IMPORT_FUNC "SystemCtrlForUser", 0xCC9ADCF8, sctrlHENSetSpeed
IMPORT_FUNC "SystemCtrlForUser", 0xFD6E46EC, sctrlHENGetSpeed
