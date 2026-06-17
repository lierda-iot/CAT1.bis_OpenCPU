#include <stdint.h>
#include <stdbool.h>
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "globalPrivate.h"
#include "powerPrivate.h"
#include "talkPrivate.h"
#include "audioModule.h"
#include "keyModule.h"
#include "appFramework.h"


bool appFrameworkSetup(void)
{
    if (!frameworkInit()) {
        return false;
    }

    if (!frameworkRegisterJob(getTalkJobDesc())) {
        return false;
    }

    if (!frameworkRegisterJob(getPowerJobDesc())) {
        return false;
    }

    if (!frameworkRegisterPolicy(globalPolicy())) {
        return false;
    }

    keyModuleInit();

    frameworkStart();

    return true;
}
