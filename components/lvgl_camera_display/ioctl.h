#pragma once

// Ne redéclare ioctl que s'il n'est pas déjà défini par ESP-IDF
#ifndef _SYS_IOCTL_H_  // vérifie si ESP-IDF l'a déjà inclus
#define _SYS_IOCTL_H_

// Si tu veux fournir des constantes VIDIOC_* etc. mets-les ici
#include "../mipi_dsi_cam/videodev2.h"

#ifdef __cplusplus
extern "C" {
#endif

// Ne redéclare PAS ioctl s’il est déjà défini
#ifndef _IOCTL_ALREADY_DECLARED
#define _IOCTL_ALREADY_DECLARED
int ioctl(int fd, int request, ...);  // signature conforme à ESP-IDF
#endif

#ifdef __cplusplus
}
#endif

#endif // _SYS_IOCTL_H_




