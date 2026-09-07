#ifndef APP_FEATURES_H
#define APP_FEATURES_H

#define APP_ENABLED_COUNT                                                     \
  (APP_WATCHFACE_EN + APP_BAJI_EN + APP_ATTITUDE_EN + APP_ATTFUN_EN +        \
   APP_SALARY_EN + APP_MAP_EN + APP_MP3_EN)

#define APP_SINGLE_MODE (APP_ENABLED_COUNT == 1)

#endif
