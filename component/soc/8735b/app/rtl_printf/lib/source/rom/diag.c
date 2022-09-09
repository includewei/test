/**************************************************************************//**
 * @file     diag.c
 * @brief    This file just declare the variable for debug message on/off control.
 *
 * @version  V1.00
 * @date     2016-05-31
 *
 * @note
 *
 ******************************************************************************
 *
 * Copyright(c) 2007 - 2016 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 ******************************************************************************/
#include "diag.h"

#define SECTION_DIAG_BSS            SECTION(".diag.bss")

#if defined(ROM_REGION)
SECTION_DIAG_BSS uint32_t ConfigDebugErr;
SECTION_DIAG_BSS uint32_t ConfigDebugInfo;
SECTION_DIAG_BSS uint32_t ConfigDebugWarn;
#endif  // end of "#if defined(ROM_REGION)"

