#pragma once
#include "utils.h"

void Flash_Init(void);

void Flash_InitPassword();
void Flash_WritePassword(char *password);
void Flash_ReadPassword(char *password, size_t *pw_len);