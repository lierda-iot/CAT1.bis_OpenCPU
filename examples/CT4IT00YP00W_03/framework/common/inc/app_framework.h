#ifndef APP_FRAMEWORK_H
#define APP_FRAMEWORK_H

#include <stdbool.h>
#include "frameworkTypes.h"

bool appFrameworkSetup(void);

const jobDesc_t *getTalkJobDesc(void);
const jobDesc_t *getPowerkeyJobDesc(void);
const jobDesc_t *getNetworkJobDesc(void);
const jobDesc_t *getLedSideJobDesc(void);
policy_t *globalPolicy(void);

#endif /* APP_FRAMEWORK_H */
