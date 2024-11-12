#include "misc.h"
#include "inc/vsh_exports.h"



static void *vsh_pdata_addr = NULL;



/***********************************************************************
* search and return vsh_process toc
* Not the best way, but it work, it's generic and it is fast enough...
***********************************************************************/
static uint32_t get_vsh_toc(void)
{
	uint32_t pm_start  = 0x10000UL;
  uint32_t v0 = 0, v1 = 0, v2 = 0;
  
  while(pm_start < 0x700000UL)
  {
		v0 = *(uint32_t*)(pm_start+0x00);
		v1 = *(uint32_t*)(pm_start+0x04);
		v2 = *(uint32_t*)(pm_start+0x0C);
		
		if((v0 == 0x00010200UL/* .init_proc() */) && (v1 == v2))
			break;
		
		pm_start+=4;
	}
	
	return v1; 
}

/***********************************************************************
* get vsh io_pad_object
***********************************************************************/
static uint32_t get_vsh_pad_obj(void)
{
	uint32_t (*base)(uint32_t) = sys_io_3733EA3C;               // get pointer to cellPadGetData()
	int16_t idx = *(uint32_t*)(*(uint32_t*)base) & 0x0000FFFF;  // get got_entry idx from first instruction,  
	uint32_t got_entry = (idx + get_vsh_toc());                 // get got_entry of io_pad_object
  return (*(uint32_t*)got_entry);                             // return io_pad_object address
}

/***********************************************************************
* get pad data struct of vsh process
* 
* CellPadData *data = data struct for holding pad_data
***********************************************************************/
void VSHPadGetData(CellPadData *data)
{
	uint32_t pm_start = 0x10000UL;
	uint64_t pat[2]   = {0x380000077D3F4B78ULL, 0x7D6C5B787C0903A6ULL};
	
	if(!vsh_pdata_addr)        // first time, get address
	{
		while(pm_start < 0x700000UL)
	  {
			if((*(uint64_t*)pm_start == pat[0]) && (*(uint64_t*)(pm_start+8) == pat[1]))
		  {
		  	vsh_pdata_addr = (void*)((((*(uint32_t*)(pm_start + 0x234) & 0x0000FFFF) -1) <<16) |
		  	                           (*(uint32_t*)(pm_start + 0x244) & 0x0000FFFF));
			  break;
		  }
		  
		  pm_start+=4;
		}
	}
	
	memcpy(data, vsh_pdata_addr, 0x80);
}

/***********************************************************************
* set/unset io_pad_library init flag
* 
* uint8_t flag = 0(unset) or 1(set)
* 
* To prevent vsh pad events during vsh-menu, we set this flag to 0
* (pad_library not init). Each try of vsh to get pad_data leads intern
* to error 0x80121104(CELL_PAD_ERROR_UNINITIALIZED) and nothing will
* happen. The lib is of course not deinit, there are pad(s) mapped and we
* can get pad_data direct over syscall sys_hid_manager_read() in our
* own function MyPadGetData().
***********************************************************************/
void start_stop_vsh_pad(uint8_t flag)
{
  uint32_t lib_init_flag = get_vsh_pad_obj();
  *(uint8_t*)lib_init_flag = flag;
}

/***********************************************************************
* get pad data direct over syscall 
* (very simple, no error-handling, no synchronization...)
* 
* int32_t port_no   = pad port number (0 - 7)
* CellPadData *data = data struct for holding pad_data
***********************************************************************/
void MyPadGetData(int32_t port_no, CellPadData *data)
{
  uint32_t port = *(uint32_t*)(*(uint32_t*)(get_vsh_pad_obj() + 4) + 0x104 + port_no * 0xE8);
  
  // sys_hid_manager_read()
	system_call_4(0x1F6, (uint64_t)port, /*0x02*//*0x82*/0xFF, (uint64_t)(uint32_t)data+4, 0x80);
	
	data->len = (int32_t)p1;
}

/***********************************************************************
* pause/continue rsx fifo
* 
* uint32_t rsx_ctx = rsx context, e.g. 0x55555555 (VSH)
* uint8_t pause    = pause fifo (1), continue fifo (0)
***********************************************************************/
int32_t lv1_rsx_fifo_pause(uint32_t rsx_ctx, uint8_t pause)
{
	uint64_t op_code;
	
	if(pause)
	  op_code = 0x2ULL;         // pause rsx fifo
	else
	  op_code = 0x3ULL;         // continue rsx fifo
	
	// lv1_gpu_context_attribute()
	system_call_8(10, (uint64_t)rsx_ctx, op_code, 0, 0, 0, 0, 0, (uint64_t)225);
	uint64_t ret = p1;
	return (int32_t)ret;
}


/***********************************************************************
* ring buzzer
***********************************************************************/
void buzzer(uint8_t mode)
{
	uint16_t param = 0;
	
	switch(mode)
	{
		case 1: param = 0x0006; break;		// single beep
		case 2: param = 0x0036; break;		// double beep
		case 3: param = 0x01B6; break;		// triple beep
		case 4: param = 0x0FFF; break;		// continuous beep, gruesome!!!
	}
	
	system_call_3(392, 0x1007, 0xA, param);
}
