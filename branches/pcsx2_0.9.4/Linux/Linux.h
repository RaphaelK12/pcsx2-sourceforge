/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2003  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_H__
#define __LINUX_H__

#include "Common.h"

typedef struct {
	char lang[256];
} _langs;

_langs *langs;
unsigned int langsMax;

extern int UseGui;
char cfgfile[256];

int LoadConfig();
void SaveConfig();

void StartGui();
void RunGui();

int Pcsx2Configure();

void InitLanguages();
char *GetLanguageNext();
void CloseLanguages();
void ChangeLanguage(char *lang);

#endif /* __LINUX_H__ */
