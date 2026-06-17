#include <stdbool.h>
#include "frameworkTypes.h"
#include "frameworkCore.h"
#include "app_framework.h"

bool appFrameworkSetup(void)
{
    if (!frameworkInit()) {
        return false;
    }

    frameworkRegisterJob(getTalkJobDesc());
    frameworkRegisterJob(getPowerkeyJobDesc());
    frameworkRegisterJob(getNetworkJobDesc());
    frameworkRegisterSideJob(getLedSideJobDesc());

    frameworkRegisterPolicy(globalPolicy());

    if (!frameworkStart()) {
        return false;
    }

    return true;
}
