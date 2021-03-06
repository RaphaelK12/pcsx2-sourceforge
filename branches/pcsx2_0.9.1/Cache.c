/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2005  Pcsx2 Team
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "Common.h"
#include "Cache.h"

/*_cacheS pCache[64];
int getFreeCache(u32 mem, int mode, int * way) {
	u8 * out;
	u32 paddr;
	u32 taddr[2];
	u8 * t;
	int number;
	int i = (mem >> 6) & 0x3F;
	
	paddr = memLUTR[mem >> 12];
	taddr[0] = memLUTW[pCache[i].tag[0]>>12];
	taddr[1] = memLUTW[pCache[i].tag[1]>>12];

	if (taddr[0] == paddr && (pCache[i].tag[0] & 0x20)) 
	{
		*way = 0;
		return i;
	}else if(taddr[1] == paddr && (pCache[i].tag[1] & 0x20)) 
	{
		*way = 1;
		return i;
	}

	number = ((pCache[i].tag[0]>>4) & 1) ^ ((pCache[i].tag[1]>>4) & 1);

	if(pCache[i].tag[number] & 0x60)	// Valid Dirty
	{
		t = (char *)(taddr[number]);
		out = (u8*)(t + (mem & 0xFC0));
		((u64*)out)[0] = ((u64*)pCache[i].data[number][0].b8._8)[0];
		((u64*)out)[1] = ((u64*)pCache[i].data[number][0].b8._8)[1];
		((u64*)out)[2] = ((u64*)pCache[i].data[number][1].b8._8)[0];
		((u64*)out)[3] = ((u64*)pCache[i].data[number][1].b8._8)[1];
		((u64*)out)[4] = ((u64*)pCache[i].data[number][2].b8._8)[0];
		((u64*)out)[5] = ((u64*)pCache[i].data[number][2].b8._8)[1];
		((u64*)out)[6] = ((u64*)pCache[i].data[number][3].b8._8)[0];
		((u64*)out)[7] = ((u64*)pCache[i].data[number][3].b8._8)[1];
	}


	
	if(mode == 1)
	{
		pCache[i].tag[number] |= 0x40;	// Set Dirty Bit if mode == write
	}

	pCache[i].tag[number] &= ~(0xFFFFF000);
	pCache[i].tag[number] |= ((mem>>12) & 0xFFFFF) << 12;


	t = (u8 *)paddr; 
	out=  (u8*)(t + (mem & 0xFC0));
	((u64*)pCache[i].data[number][0].b8._8)[0] = ((u64*)out)[0];
	((u64*)pCache[i].data[number][0].b8._8)[1] = ((u64*)out)[1];
	((u64*)pCache[i].data[number][1].b8._8)[0] = ((u64*)out)[2];
	((u64*)pCache[i].data[number][1].b8._8)[1] = ((u64*)out)[3];
	((u64*)pCache[i].data[number][2].b8._8)[0] = ((u64*)out)[4];
	((u64*)pCache[i].data[number][2].b8._8)[1] = ((u64*)out)[5];
	((u64*)pCache[i].data[number][3].b8._8)[0] = ((u64*)out)[6];
	((u64*)pCache[i].data[number][3].b8._8)[1] = ((u64*)out)[7];

	if(pCache[i].tag[number] & 0x10) pCache[i].tag[number] &= ~(0x10);
	else pCache[i].tag[number] |= 0x10;
	
	pCache[i].tag[number] |= 0x20;
	*way = number;
	return i;
}

void writeCache8(u32 mem, u8 value) {
	int i, number;

	i = getFreeCache(mem,1,&number);

	pCache[i].data[number][(mem>>4) & 0x3].b8._8[(mem&0xf)] = value;
}

void writeCache16(u32 mem, u16 value) {
	int i, number;
 
	i = getFreeCache(mem,1,&number);

	*(u16*)(&pCache[i].data[number][(mem>>4) & 0x3].b8._8[(mem&0xf)]) = value;
}

void writeCache32(u32 mem, u32 value) {
	int i, number;

	i = getFreeCache(mem,1,&number);
	*(u32*)(&pCache[i].data[number][(mem>>4) & 0x3].b8._8[(mem&0xf)]) = value;
}

void writeCache64(u32 mem, u64 value) {
	int i, number;

	i = getFreeCache(mem,1,&number);

	*(u64*)(&pCache[i].data[number][(mem>>4) & 0x3].b8._8[(mem&0xf)]) = value;
}

void writeCache128(u32 mem, u64 *value) {
	int i, number;

	i = getFreeCache(mem,1,&number);
	((u64*)pCache[i].data[number][(mem>>4) & 0x3].b8._8)[0] = value[0];
	((u64*)pCache[i].data[number][(mem>>4) & 0x3].b8._8)[1] = value[1];
}

u8 *readCache(u32 mem) {
	int i, number;

	i = getFreeCache(mem,0,&number);

	return pCache[i].data[number][(mem>>4) & 0x3].b8._8;
}

void CACHE() {
    u32 addr;

	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	switch (_Rt_) {
		case 0x1a:
		{
			int index = (addr >> 6) & 0x3F;
			u32 paddr[2];
			int way;
			u32 taddr = memLUTR[addr >> 12];
			paddr[0] = memLUTW[pCache[index].tag[0] >> 12];
			paddr[1] = memLUTW[pCache[index].tag[1] >> 12];

			if(paddr[0] == taddr && (pCache[index].tag[0] & 0x20))
			{
				way = 0;
			}
			else if(paddr[1] == taddr && (pCache[index].tag[1] & 0x20))
			{
				way = 1;
			} 
			else
			{
				return;
			}

#ifdef GTE_LOG
	GTE_LOG("CACHE DHIN addr %x, index %d, way %d, Flags %x\n",addr,index,way,pCache[index].tag[way] & 0x78);
#endif
			pCache[index].tag[way] &= ~(0x6F);
			((u64*)pCache[index].data[way][0].b8._8)[0] = 0;
			((u64*)pCache[index].data[way][0].b8._8)[1] = 0;
			((u64*)pCache[index].data[way][1].b8._8)[0] = 0;
			((u64*)pCache[index].data[way][1].b8._8)[1] = 0;
			((u64*)pCache[index].data[way][2].b8._8)[0] = 0;
			((u64*)pCache[index].data[way][2].b8._8)[1] = 0;
			((u64*)pCache[index].data[way][3].b8._8)[0] = 0;
			((u64*)pCache[index].data[way][3].b8._8)[1] = 0;
			break;
		}
		case 0x18:
		{
			u8 * out;
			int index = (addr >> 6) & 0x3F;
			u32 paddr[2];
			int way;
			u32 taddr = memLUTW[addr >> 12];
			paddr[0] = memLUTW[pCache[index].tag[0] >> 12];
			paddr[1] = memLUTW[pCache[index].tag[1] >> 12];

			if(paddr[0] == taddr && (pCache[index].tag[0] & 0x20))
			{
				way = 0;
			}
			else if(paddr[1] == taddr && (pCache[index].tag[1] & 0x20))
			{
				way = 1;
			} 
			else
			{
				return;
			}

#ifdef GTE_LOG
	GTE_LOG("CACHE DHWBIN addr %x, index %d, way %d, Flags %x\n",addr,index,way,pCache[index].tag[way] & 0x78);
#endif

			if(pCache[index].tag[way] & 0x60)	// Valid Dirty
			{
				char * t = (char *)(taddr);
				out = (u8*)(t + (addr & 0xFC0));
				((u64*)out)[0] = ((u64*)pCache[index].data[way][0].b8._8)[0];
				((u64*)out)[1] = ((u64*)pCache[index].data[way][0].b8._8)[1];
				((u64*)out)[2] = ((u64*)pCache[index].data[way][1].b8._8)[0];
				((u64*)out)[3] = ((u64*)pCache[index].data[way][1].b8._8)[1];
				((u64*)out)[4] = ((u64*)pCache[index].data[way][2].b8._8)[0];
				((u64*)out)[5] = ((u64*)pCache[index].data[way][2].b8._8)[1];
				((u64*)out)[6] = ((u64*)pCache[index].data[way][3].b8._8)[0];
				((u64*)out)[7] = ((u64*)pCache[index].data[way][3].b8._8)[1];
			}

			pCache[index].tag[way] &= ~(0x6F);
			break;
		}
		case 0x1c:
		{
			u8 * out;
			int index = (addr >> 6) & 0x3F;
			u32 paddr[2];
			int way;
			u32 taddr = memLUTW[addr >> 12];
			paddr[0] = memLUTW[pCache[index].tag[0] >> 12];
			paddr[1] = memLUTW[pCache[index].tag[1] >> 12];

			if(paddr[0] == taddr && (pCache[index].tag[0] & 0x20))
			{
				way = 0;
			}
			else if(paddr[1] == taddr && (pCache[index].tag[1] & 0x20))
			{
				way = 1;
			} 
			else
			{
				return;
			}
#ifdef GTE_LOG
	GTE_LOG("CACHE DHWOIN addr %x, index %d, way %d, Flags %x\n",addr,index,way,pCache[index].tag[way] & 0x78);
#endif
			if(pCache[index].tag[way] & 0x60)	// Valid Dirty
			{
				char * t = (char *)(taddr);
				out = (u8*)(t + (addr & 0xFC0));
				((u64*)out)[0] = ((u64*)pCache[index].data[way][0].b8._8)[0];
				((u64*)out)[1] = ((u64*)pCache[index].data[way][0].b8._8)[1];
				((u64*)out)[2] = ((u64*)pCache[index].data[way][1].b8._8)[0];
				((u64*)out)[3] = ((u64*)pCache[index].data[way][1].b8._8)[1];
				((u64*)out)[4] = ((u64*)pCache[index].data[way][2].b8._8)[0];
				((u64*)out)[5] = ((u64*)pCache[index].data[way][2].b8._8)[1];
				((u64*)out)[6] = ((u64*)pCache[index].data[way][3].b8._8)[0];
				((u64*)out)[7] = ((u64*)pCache[index].data[way][3].b8._8)[1];
			}

			pCache[index].tag[way] &= ~(0x40);
			break;
		}
		case 0x16:
		{
			int index = (addr >> 6) & 0x3F;
			int way = addr & 0x1;
#ifdef GTE_LOG
	GTE_LOG("CACHE DXIN addr %x, index %d, way %d, flag %x\n",addr,index,way,pCache[index].tag[way] & 0x78);
#endif
			pCache[index].tag[way] &= ~(0x6F);
			break;
		}
		case 0x11:
		{
			int index = (addr >> 6) & 0x3F;
			int way = addr & 0x1;
			u8 * out = pCache[index].data[way][(addr>>4) & 0x3].b8._8;
			cpuRegs.CP0.r[28] = *(u32 *)(out+(addr&0xf));
#ifdef GTE_LOG
	GTE_LOG("CACHE DXLDT addr %x, index %d, way %d, DATA %x\n",addr,index,way,cpuRegs.CP0.r[28]);
#endif
			break;
		}
		case 0x10:
		{
			int index = (addr >> 6) & 0x3F;
			int way = addr & 0x1;
			
			cpuRegs.CP0.r[28] = 0;
			cpuRegs.CP0.r[28] = pCache[index].tag[way];
#ifdef GTE_LOG
	GTE_LOG("CACHE DXLTG addr %x, index %d, way %d, DATA %x\n",addr,index,way,cpuRegs.CP0.r[28]);
#endif		
			break;
		}
		case 0x13:
		{
			int index = (addr >> 6) & 0x3F;
			int way = addr & 0x1;
			u8 * out = pCache[index].data[way][(addr>>4) & 0x3].b8._8;
			*(u32*)(&pCache[index].data[way][(addr>>4) & 0x3].b8._8[(addr&0xf)]) = cpuRegs.CP0.r[28];
#ifdef GTE_LOG
	GTE_LOG("CACHE DXSDT addr %x, index %d, way %d, DATA %x\n",addr,index,way,cpuRegs.CP0.r[28]);
#endif
			break;
		}
		case 0x12:
		{
			int index = (addr >> 6) & 0x3F;
			int way = addr & 0x1;
			pCache[index].tag[way] = cpuRegs.CP0.r[28];
#ifdef GTE_LOG
	GTE_LOG("CACHE DXSTG addr %x, index %d, way %d, DATA %x\n",addr,index,way,cpuRegs.CP0.r[28] & 0x6F);
#endif
			break;
		}
		case 0x14:
		{

			u8 * out;
			int index = (addr >> 6) & 0x3F;
			int way = addr & 0x1;

#ifdef GTE_LOG
	GTE_LOG("CACHE DXWBIN addr %x, index %d, way %d, Flags %x\n",addr,index,way,pCache[index].tag[way] & 0x78);
#endif
			if(pCache[index].tag[way] & 0x60)	// Dirty
			{
				u32 paddr = memLUTW[pCache[index].tag[way] >> 12];
				char * t = (char *)(paddr);
				out = (u8*)(t + (addr & 0xFC0)); 
				((u64*)out)[0] = ((u64*)pCache[index].data[way][0].b8._8)[0];
				((u64*)out)[1] = ((u64*)pCache[index].data[way][0].b8._8)[1];
				((u64*)out)[2] = ((u64*)pCache[index].data[way][1].b8._8)[0];
				((u64*)out)[3] = ((u64*)pCache[index].data[way][1].b8._8)[1];
				((u64*)out)[4] = ((u64*)pCache[index].data[way][2].b8._8)[0];
				((u64*)out)[5] = ((u64*)pCache[index].data[way][2].b8._8)[1];
				((u64*)out)[6] = ((u64*)pCache[index].data[way][3].b8._8)[0];
				((u64*)out)[7] = ((u64*)pCache[index].data[way][3].b8._8)[1];
			}

			pCache[index].tag[way] &= ~(0x6F);
			break;
		}
	}
}*/

void CACHE() {
}