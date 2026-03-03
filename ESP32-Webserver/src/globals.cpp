#include "globals.h"

volatile bool rebootPending = false;
unsigned long rebootAt      = 0;

uint32_t REBOOT_DELAY_MS       = 5000;  // Defaults
uint32_t REBOOT_DELAY_WEBIF_MS = 4000;
