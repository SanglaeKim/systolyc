#include "xaxicdma.h"
#include "xdebug.h"
#include "xil_exception.h"
#include "xil_cache.h"
#include "xparameters.h"
#include "platform.h"
#include "xscugic.h"

#include "xpseudo_asm_gcc.h"
#include "xreg_cortexa53.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "sleep.h"
#include "main.h"

#include "ff.h"

// number of element in an array
#define NOE(arr) (sizeof(arr)/sizeof(arr[0]))

/******************** Important!!!! **********************************/
// when you put stdio(print) related functions like printf, puts and so on...
// Be Very Cautious!!!!
// because it will ruin interrupt service routines
/************************** Function Prototypes ******************************/

static FIL fil; /* File object */
static FATFS fatfs;
static char *SD_File;
bool bSdcardMounted;

#define INTERRUPT_ID_PL2PS_EVEN  	(122U)	//	PL to PS	even intr
#define INTERRUPT_ID_PL2PS_ODD		(123U)	//	PL to PS	odd  intr

/******************** Constant Definitions **********************************/
#define DEVICE_ID_CDMA0 	XPAR_AXICDMA_0_DEVICE_ID
#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID
#define INTERRUPT_ID_CDMA0	XPAR_FABRIC_AXICDMA_0_VEC_ID

/************************** Function Prototypes ******************************/
#if (!defined(DEBUG))
extern void xil_printf(const char *format, ...);
#endif

static void isr_cdma(void *CallBackRef, u32 IrqMask, int *IgnorePtr);

/************************** Variable Definitions *****************************/
static XAxiCdma AxiCdmaInstance; /* Instance of the XAxiCdma */
static XScuGic IntcController; /* Instance of the Interrupt Controller */

static u8 ucWeightArr[NUM_BYTES_WEIGHT] __attribute__ ((aligned(32)));

/* Source and Destination buffer for DMA transfer. */
static u8 DesBuffer[16][NUM_BYTES_UL] __attribute__ ((aligned (64)));

/* Shared variables used to test the callbacks. */
volatile static u32 Done = 0U; /* Dma transfer is done */
volatile static u32 Error = 0U; /* Dma Bus Error occurs */

volatile static PL2PS_EVENT_t rd_event_type = PL2PS_EVENT_EVEN;
volatile static bool bPL2PS_READ_EVENT = false;

volatile int isr_cnt_even = 0U;
volatile int isr_cnt_odd = 0U;
volatile int isr_cnt_cdma = 0U;
volatile int cdmaErrCount = 0U;

void isr_even_pl2ps(void *CallbackRef) {
	if (!bPL2PS_READ_EVENT) {
		isr_cnt_even++;
		bPL2PS_READ_EVENT = true;
		rd_event_type = PL2PS_EVENT_EVEN;
	}
}
void isr_odd_pl2ps(void *CallbackRef) {
	if (!bPL2PS_READ_EVENT) {
		isr_cnt_odd++;
		bPL2PS_READ_EVENT = true;
		rd_event_type = PL2PS_EVENT_ODD;
	}
}
void isr_cdma(void *CallBackRef, u32 IrqMask, int *IgnorePtr) {
	bPL2PS_READ_EVENT = false;
	isr_cnt_cdma++;
	if (IrqMask & XAXICDMA_XR_IRQ_ERROR_MASK) {
		Error = TRUE;
	}
	if (IrqMask & XAXICDMA_XR_IRQ_IOC_MASK) {
		Done = TRUE;
	}
}

int main() {

	int Status;
	init_platform(); // for printf

	displayAppInfo();

	setupInterrupt();

	u32 fileNameIndex = 0;

	u32 sramBaseAddr[PL2PS_EVENT_MAX] = { SRAM_ADDR_UL_EVEN, SRAM_ADDR_UL_ODD };

	setupSram();

	while (1) {

		if (bPL2PS_READ_EVENT) {
//	  bPL2PS_READ_EVENT = false;

			u32 SrcAddr = sramBaseAddr[rd_event_type];

			Status = XAxiCdma_SimpleTransfer(&AxiCdmaInstance,
					(UINTPTR) SrcAddr
					, (UINTPTR) &DesBuffer[fileNameIndex][0]
					, NUM_BYTES_UL
					, isr_cdma,
					(void *) &AxiCdmaInstance);

			while (!Done) {
				// Wait for CDMA transfer to complete!!
			}
			Done = 0;

			if (Status == XST_FAILURE) cdmaErrCount++;

			if ((++fileNameIndex % 16) == 0) {
				fileNameIndex = 0;

				// After DMA, invalidate cache to load from DDR
				Xil_DCacheInvalidateRange((UINTPTR) &DesBuffer, sizeof(DesBuffer));

				for(int i=0; i<16; i++){

					char fileNameI[32], fileNameII[32];

					FRESULT Res;
					UINT NumBytesRead;
					uint8_t refBuff[NUM_BYTES_UL/2];

					sprintf(fileNameI,  "oData%d.bin", 2*i);
					sprintf(fileNameII, "oData%d.bin", 2*i+1);

					// check first half
					SD_File = fileNameI;
					Res = f_open(&fil, SD_File, FA_READ);
					if (Res) return XST_FAILURE;

					Res = f_read(&fil, (void*) refBuff, NUM_BYTES_UL/2, &NumBytesRead);
					if (Res) return XST_FAILURE;
					for(int j=0; j<NUM_BYTES_UL/2; j++){
						if(refBuff[j] != DesBuffer[i][j]){
							xil_printf("final fail!!\n");
							return XST_FAILURE;
						}
					}
					Res = f_close(&fil);
					if (Res) return XST_FAILURE;

					// check second half
					SD_File = fileNameII;
					Res = f_open(&fil, SD_File, FA_READ);
					if (Res) return XST_FAILURE;

					Res = f_read(&fil, (void*) refBuff, NUM_BYTES_UL/2, &NumBytesRead);
					if (Res) return XST_FAILURE;
					for(int j=0; j<NUM_BYTES_UL/2; j++){
						if(refBuff[j] != DesBuffer[i][j+NUM_BYTES_UL/2]){
							xil_printf("final fail!!\n");
							return XST_FAILURE;
						}
					}

					Res = f_close(&fil);
					if (Res) return XST_FAILURE;

				}

#ifdef WRITE_TO_FILE
				xil_printf("writing to sdcard!!\n");
				for (int i = 0; i < 16; ++i) {
					Xil_DCacheInvalidateRange((UINTPTR) &DesBuffer[i][0], NUM_BYTES_UL); // not sure if necessary
					writeToFile(&DesBuffer[i][0], 				NUM_BYTES_UL / 2, 2 * i, 	 "oData");
					writeToFile(&DesBuffer[i][NUM_BYTES_UL / 2],NUM_BYTES_UL / 2, 2 * i + 1, "oData");
				}
				xil_printf("unmounting sdcard!!\n");
#endif

				f_mount(NULL, "", 0);
				break;
				//				systolic_1_test();
			}
		}
	}

	xil_printf("Successfully ran XAxiCdma_SimpleIntr Example\r\n");
	xil_printf("--- Exiting main() --- \r\n");

	return XST_SUCCESS;
}

//=================================================================
//	SCU GIC (global interrupt) 초기화
//=================================================================
int GIC_Init(XScuGic *hGICInstPtr) {
	int Status;
	XScuGic_Config *IntcConfig;

	XScuGic_DeviceInitialize(0);

	IntcConfig = XScuGic_LookupConfig(XPAR_SCUGIC_SINGLE_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(hGICInstPtr, IntcConfig,
			IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
			(Xil_ExceptionHandler) XScuGic_InterruptHandler, hGICInstPtr);

	Xil_ExceptionInit();

	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

//=================================================================
//	Interrupt 연결
//=================================================================

int Enable_IntrruptSystem(XScuGic *hGICInstPtr, u16 IntrruptId,
		Xil_ExceptionHandler InterruptHandler) {
	int Status = 0;

	//	DP_RAM 인터럽트 트리거 설정
	if (IntrruptId == INTERRUPT_ID_PL2PS_EVEN
			|| IntrruptId == INTERRUPT_ID_PL2PS_ODD) {
		XScuGic_SetPriorityTriggerType(hGICInstPtr, IntrruptId, 0x00, 0x3);
		//		XScuGic_SetPriorityTriggerType(hGICInstPtr, IntrruptId, 0x64, 0x3);
	}

	Status = XScuGic_Connect(hGICInstPtr, IntrruptId,
			(Xil_ExceptionHandler) InterruptHandler, (void *) hGICInstPtr);

	if (Status != XST_SUCCESS) {
		return Status;
	}

	XScuGic_Enable(hGICInstPtr, IntrruptId);

	return Status;
}

//================================================================================
//	CDMA 생성 및 인터럽트 등록
//================================================================================
int Init_CDMA(XScuGic *IntcInstancePtr, XAxiCdma *InstancePtr, u16 DeviceId,
		u32 IntrId) {
	XAxiCdma_Config *CfgPtr;
	int Status;

	// Initialize the XAxiCdma device.
	CfgPtr = XAxiCdma_LookupConfig(DeviceId);
	if (!CfgPtr) {
		return XST_FAILURE;
	}

	Status = XAxiCdma_CfgInitialize(InstancePtr, CfgPtr, CfgPtr->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	//	CDMA를 GIC에 연결 (CDMA 인터럽트 등록)
	Enable_CMDA_Intrrupt(IntcInstancePtr, InstancePtr, IntrId);

	// Enable all (completion/error/delay) interrupts
	XAxiCdma_IntrEnable(InstancePtr, XAXICDMA_XR_IRQ_ALL_MASK);

	return XST_SUCCESS;
}

int Enable_CMDA_Intrrupt(XScuGic *IntcInstancePtr, XAxiCdma *InstancePtr, u32 IntrId) {
	int Status;

	XScuGic_SetPriorityTriggerType(IntcInstancePtr, IntrId, 0xA0, 0x3);

	// Connect the device driver handler that will be called when an
	// interrupt for the device occurs, the handler defined above performs
	// the specific interrupt processing for the device.

	Status = XScuGic_Connect(IntcInstancePtr, IntrId, (Xil_InterruptHandler) XAxiCdma_IntrHandler, InstancePtr);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	//Enable the interrupt for the DMA device.
	XScuGic_Enable(IntcInstancePtr, IntrId);

	return XST_SUCCESS;
}

void systolic_1_test(void) {
	unsigned int k = 0;
	unsigned int k_word = 0;
	unsigned int offset_addr = 0;
	unsigned int SRAM_0_Write_Buffer[642];
	//	usleep(1000000);	// 1ms

	xil_printf("### PS to PL Download start \r\n");
	sleep(1);

	for (int i = 0; i < 214; i++)				// 0~213 (642/3)
			{
		for (int b = 0; b < 3; b++)				// block_num : 0~2
				{

			if (b == 0)
				offset_addr = 0 + i * 642 * 4;
			else if (b == 1)
				offset_addr = 262144 * 4 + i * 642 * 4;
			else if (b == 2)
				offset_addr = 524288 * 4 + i * 642 * 4;

			for (int j = 0; j < 642; j++)				// 1~10
					{
				if (k >= 255)
					k = 0;
				else
					k = k + 1;						// 0~ 642*642

				k_word = k + k * 256 + k * 65536;	// B[23:16]. G[15:8]. R[7:0]
				SRAM_0_Write_Buffer[j] = k_word;	// DDR3 Test write data
			}
			//memcpy ((void*)(SRAM_0_Write_Buffer), PS_to_PL_DPRAM_Ctrl, sizeof(SRAM_0_Write_Buffer));

			int temp = (0xA0000000U + offset_addr);
			uintptr_t pSramBase = (uintptr_t) temp;
			memcpy((void*) pSramBase, (SRAM_0_Write_Buffer),
					sizeof(SRAM_0_Write_Buffer));

		}
	}
}

// fnPrefix - file name prefix wData, bData, iData
void initStSram(StSram *pStSram, u32 ramIndex, char *fnPrefix, u32 ramBase, u32 fileSize) {

	pStSram->pBase = (uintptr_t) ramBase;
	pStSram->numBytes = fileSize;

	char fileName[32];
	sprintf(fileName, "%s%d.bin", fnPrefix, ramIndex);
	strncpy(pStSram->FileName, fileName, sizeof(fileName));
}

bool checkFileSize(StSram *pStSram) {

	FILINFO fno;
	FRESULT res;

	bool ret = true;

	if (!bSdcardMounted){
		xil_printf("sd card is not mounted!\n");
		return false;
	}

	res = f_stat(pStSram->FileName, &fno);
	if (res){
		xil_printf("f_stat failed!!\n");
		return false;
	}

	if (fno.fsize != pStSram->numBytes) {
		ret = false;
	}
	return ret;
}

int writeToSram(StSram *pStSram) {

	FRESULT Res;
	UINT NumBytesRead;
	SD_File = pStSram->FileName;

	Res = f_open(&fil, SD_File, FA_READ);
	if (Res) return XST_FAILURE;

	//#define CACHE_AWARE
#ifdef CACHE_AWARE
//	Xil_DCacheInvalidateRange((UINTPTR) ucWeightArr, pStSram->numBytes); // not sure if necessary
	Xil_DCacheFlushRange((UINTPTR) ucWeightArr, sizeof(ucWeightArr)); // not sure if necessary
#endif

	Res = f_read(&fil, (void*) ucWeightArr, pStSram->numBytes, &NumBytesRead);
	if (Res) return XST_FAILURE;

#ifdef CACHE_AWARE
//	Xil_DCacheFlushRange((UINTPTR)&SrcBuffer, Length);
	Xil_DCacheFlushRange((UINTPTR)ucWeightArr, sizeof(ucWeightArr));
#endif

	memcpy((void*) pStSram->pBase, ucWeightArr, pStSram->numBytes);

	Res = f_close(&fil);
	if (Res) return XST_FAILURE;

	return XST_SUCCESS;

}

int setupSram() {

	StSram wStSramArr[6];
	StSram bStSramArr[6];
	StSram iStSramArr[16];
	// weight
	u32 wBases[] = { 0xA3000000, 0xA30004B0, 0xA3002A30, 0xA3003390, 0xA3004650, 0xA3005910 };
	u32 wFileSize[] = { 432, 4608, 1024, 2304, 2304, 1536 };
	// batch normalize
	u32 bBases[] = { 0xA3006590, 0xA3006690, 0xA3006790, 0xA3006890, 0xA3006990, 0xA3006A90 };
	u32 bFileSize[] = { 256, 256, 512, 256, 256, 512 };
	// input feature map?
	u32 iBases[] = { 0xA0000000, 0xA0020000, 0xA0040000, 0xA0060000, 0xA0080000, 0xA00A0000, 0xA00C0000, 0xA00E0000
					,0xA0100000, 0xA0120000, 0xA0140000, 0xA0160000, 0xA0180000, 0xA01A0000, 0xA01C0000, 0xA01E0000 };

	FRESULT Res;
	TCHAR *Path = "0:/";
	Res = f_mount(&fatfs, Path, 0);

	if (Res != FR_OK) return XST_FAILURE;
	bSdcardMounted = true;

	for (int i = 0; i < NOE(wStSramArr); i++) {
		initStSram(&wStSramArr[i], i, "wData", wBases[i], wFileSize[i]);
		if (!checkFileSize(&wStSramArr[i])) {
			xil_printf("[W]chk file size error!!\n");
			return XST_FAILURE;
		}
	}

	for (int i = 0; i < NOE(bStSramArr); i++) {
		initStSram(&bStSramArr[i], i, "bData", bBases[i], bFileSize[i]);
		if (!checkFileSize(&bStSramArr[i])) {
			xil_printf("[B]chk file size error!!\n");
			return XST_FAILURE;
		}
	}

	for (int i = 0; i < NOE(iStSramArr); i++) {
		initStSram(&iStSramArr[i], i, "iData", iBases[i], 103040);
		if (!checkFileSize(&iStSramArr[i])) {
			xil_printf("[I]chk file size error!!\n");
			return XST_FAILURE;
		}
	}

	for (int i = 0; i < NOE(wStSramArr); i++)		writeToSram(&wStSramArr[i]);
	for (int i = 0; i < NOE(bStSramArr); i++)		writeToSram(&bStSramArr[i]);
	for (int i = 0; i < NOE(iStSramArr); i++)		writeToSram(&iStSramArr[i]);

	return XST_SUCCESS;
}

/*****************************************************************************/
/*
*
* Save pBuffer data to a file with the given filename index and file name prefix
*
* @param	pBuffer is a pointer to data buffer
* @param	uiNumBytes is the number of bytes to save to a file
* @param	fnIndex is file name index
* @param	fnIndex is file name prefix
*
* @return
*		- XST_SUCCESS if the file save is successful
*		- XST_FAILURE if either the transfer fails or the data has
*		  error
*
* @note		None
*
******************************************************************************/
int writeToFile(u8 *pBuffer, u32 uiNumBytes, u8 fnIndex, char *pFnPrefix) {

	FRESULT Res;
	UINT NumBytesWritten;
	char cFileName[32];
	sprintf(cFileName, "%s%d.bin", pFnPrefix, fnIndex);

	SD_File = &cFileName[0];

	Res = f_open(&fil, SD_File, FA_CREATE_ALWAYS | FA_WRITE);
	if (Res) return XST_FAILURE;

	Res = f_lseek(&fil, 0);
	if (Res) return XST_FAILURE;

	Res = f_write(&fil, (const void*) pBuffer, uiNumBytes, &NumBytesWritten);
	if (Res) return XST_FAILURE;

	Res = f_close(&fil);
	if (Res) return XST_FAILURE;

	return XST_SUCCESS;
}

void displayAppInfo(void) {
	xil_printf("\r\n--- Entering main() --- \r\n");
	printf("-- Build Info --\r\nFile Name: %s\r\nDate: %s\r\nTime: %s\r\n",	__FILE__, __DATE__, __TIME__);
}
void setupInterrupt(void) {
	int Status;
	//	GIC 생성 및 초기화
	GIC_Init(&IntcController);

	//	인터럽트 설정
	//	PL to PS DP_RAM Even Interrupt 등록 (GIC에 인터럽트 연결) - PL(FPGA)에서 데이터 읽어오기 위한 인터럽트
	Enable_IntrruptSystem(&IntcController, INTERRUPT_ID_PL2PS_EVEN,	isr_even_pl2ps);

	//	PL to PS DP_RAM Odd Interrupt 등록  (GIC에 인터럽트 연결) - PL(FPGA)에서 데이터 읽어오기 위한 인터럽트
	Enable_IntrruptSystem(&IntcController, INTERRUPT_ID_PL2PS_ODD,	isr_odd_pl2ps);

	//	DMA 디바이스 초기화
	//	PL to PS DMA
	Status = Init_CDMA(&IntcController, &AxiCdmaInstance, DEVICE_ID_CDMA0, INTERRUPT_ID_CDMA0);
	if (Status == XST_FAILURE) xil_printf("Init_CDMA Failed!!\r\n");
}
