/* $Id:
 * 
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS kernel
 * FILE:            drivers/dd/serial/devctrl.c
 * PURPOSE:         Serial IRP_MJ_DEVICE_CONTROL operations
 * 
 * PROGRAMMERS:     Herv� Poussineau (poussine@freesurf.fr)
 */
/* FIXME: call IoAcquireRemoveLock/IoReleaseRemoveLock around each I/O operation */

#define NDEBUG
#include "serial.h"

NTSTATUS STDCALL
SerialSetBaudRate(
	IN PSERIAL_DEVICE_EXTENSION DeviceExtension,
	IN ULONG NewBaudRate)
{
	USHORT divisor;
	PUCHAR ComPortBase = (PUCHAR)DeviceExtension->BaseAddress;
	ULONG BaudRate;
	NTSTATUS Status = STATUS_SUCCESS;
	
	if (NewBaudRate & SERIAL_BAUD_USER)
	{
		BaudRate = NewBaudRate & ~SERIAL_BAUD_USER;
		divisor = (USHORT)(BAUD_CLOCK / (CLOCKS_PER_BIT * BaudRate));
	}
	else
	{
		switch (NewBaudRate)
		{
			case SERIAL_BAUD_075:    divisor = 0x600; BaudRate = 75; break;
			case SERIAL_BAUD_110:    divisor = 0x400; BaudRate = 110; break;
			case SERIAL_BAUD_134_5:  divisor = 0x360; BaudRate = 134; break;
			case SERIAL_BAUD_150:    divisor = 0x300; BaudRate = 150; break;
			case SERIAL_BAUD_300:    divisor = 0x180; BaudRate = 300; break;
			case SERIAL_BAUD_600:    divisor = 0xc0;  BaudRate = 600; break;
			case SERIAL_BAUD_1200:   divisor = 0x60;  BaudRate = 1200; break;
			case SERIAL_BAUD_1800:   divisor = 0x40;  BaudRate = 1800; break;
			case SERIAL_BAUD_2400:   divisor = 0x30;  BaudRate = 2400; break;
			case SERIAL_BAUD_4800:   divisor = 0x18;  BaudRate = 4800; break;
			case SERIAL_BAUD_7200:   divisor = 0x10;  BaudRate = 7200; break;
			case SERIAL_BAUD_9600:   divisor = 0xc;   BaudRate = 9600; break;
			case SERIAL_BAUD_14400:  divisor = 0x8;   BaudRate = 14400; break;
			case SERIAL_BAUD_38400:  divisor = 0x3;   BaudRate = 38400; break;
			case SERIAL_BAUD_57600:  divisor = 0x2;   BaudRate = 57600; break;
			case SERIAL_BAUD_115200: divisor = 0x1;   BaudRate = 115200; break;
			case SERIAL_BAUD_56K:    divisor = 0x2;   BaudRate = 57600; break;
			case SERIAL_BAUD_128K:   divisor = 0x1;   BaudRate = 115200; break;
			default: Status = STATUS_INVALID_PARAMETER;
		}
	}
	
	if (NT_SUCCESS(Status))
	{
		Status = IoAcquireRemoveLock(&DeviceExtension->RemoveLock, (PVOID)DeviceExtension->ComPort);
		if (NT_SUCCESS(Status))
		{
			UCHAR Lcr;
			DPRINT("Serial: SerialSetBaudRate(COM%lu, %lu Bauds)\n", DeviceExtension->ComPort, BaudRate);
			/* FIXME: update DeviceExtension->LowerDevice when modifying LCR? */
			/* Set Bit 7 of LCR to expose baud registers */
			Lcr = READ_PORT_UCHAR(SER_LCR(ComPortBase));
			Lcr |= SR_LCR_DLAB;
			WRITE_PORT_UCHAR(SER_LCR(ComPortBase), Lcr);
			/* Write the baud rate */
			WRITE_PORT_UCHAR(SER_DLL(ComPortBase), divisor & 0xff);
			WRITE_PORT_UCHAR(SER_DLM(ComPortBase), divisor >> 8);
			/* Switch back to normal registers */
			Lcr ^= SR_LCR_DLAB;
			WRITE_PORT_UCHAR(SER_LCR(ComPortBase), Lcr);
			
			IoReleaseRemoveLock(&DeviceExtension->RemoveLock, (PVOID)DeviceExtension->ComPort);
		}
	}
	
	if (NT_SUCCESS(Status))
		DeviceExtension->BaudRate = NewBaudRate;
	return Status;
}

NTSTATUS STDCALL
SerialSetLineControl(
	IN PSERIAL_DEVICE_EXTENSION DeviceExtension,
	IN PSERIAL_LINE_CONTROL NewSettings)
{
	PUCHAR ComPortBase;
	UCHAR Lcr = 0;
	NTSTATUS Status;
	
	DPRINT("Serial: SerialSetLineControl(COM%lu, Settings { %lu %lu %lu })\n",
		DeviceExtension->ComPort, NewSettings->StopBits, NewSettings->Parity, NewSettings->WordLength);
	
	/* Verify parameters */
	switch (NewSettings->WordLength)
	{
		case 5: Lcr |= SR_LCR_CS5; break;
		case 6: Lcr |= SR_LCR_CS6; break;
		case 7: Lcr |= SR_LCR_CS7; break;
		case 8: Lcr |= SR_LCR_CS8; break;
		default: return STATUS_INVALID_PARAMETER;
	}
	
	if (NewSettings->WordLength < 5 || NewSettings->WordLength > 8)
		return STATUS_INVALID_PARAMETER;
	
	switch (NewSettings->Parity)
	{
		case NO_PARITY:    Lcr |= SR_LCR_PNO; break;
		case ODD_PARITY:   Lcr |= SR_LCR_POD; break;
		case EVEN_PARITY:  Lcr |= SR_LCR_PEV; break;
		case MARK_PARITY:  Lcr |= SR_LCR_PMK; break;
		case SPACE_PARITY: Lcr |= SR_LCR_PSP; break;
		default: return STATUS_INVALID_PARAMETER;
	}
	
	switch (NewSettings->StopBits)
	{
		case STOP_BIT_1:
			Lcr |= SR_LCR_ST1;
			break;
		case STOP_BITS_1_5:
			if (NewSettings->WordLength != 5)
				return STATUS_INVALID_PARAMETER;
			Lcr |= SR_LCR_ST2;
			break;
		case STOP_BITS_2:
			if (NewSettings->WordLength < 6 || NewSettings->WordLength > 8)
				return STATUS_INVALID_PARAMETER;
			Lcr |= SR_LCR_ST2;
			break;
		default:
			return STATUS_INVALID_PARAMETER;
	}
	
	/* Update current parameters */
	ComPortBase = (PUCHAR)DeviceExtension->BaseAddress;
	Status = IoAcquireRemoveLock(&DeviceExtension->RemoveLock, (PVOID)DeviceExtension->ComPort);
	if (!NT_SUCCESS(Status))
		return Status;
	WRITE_PORT_UCHAR(SER_LCR(ComPortBase), Lcr);
	
	/* Read junk out of RBR */
	READ_PORT_UCHAR(SER_RBR(ComPortBase));
	IoReleaseRemoveLock(&DeviceExtension->RemoveLock, (PVOID)DeviceExtension->ComPort);
	
	if (NT_SUCCESS(Status))
		DeviceExtension->SerialLineControl = *NewSettings;
	
	return Status;
}

BOOLEAN
SerialClearPerfStats(
	IN PSERIALPERF_STATS pSerialPerfStats)
{
	RtlZeroMemory(pSerialPerfStats, sizeof(SERIALPERF_STATS));
	return TRUE;
}

BOOLEAN
SerialGetPerfStats(IN PIRP pIrp)
{
	PSERIAL_DEVICE_EXTENSION pDeviceExtension;
	pDeviceExtension = (PSERIAL_DEVICE_EXTENSION)
		IoGetCurrentIrpStackLocation(pIrp)->DeviceObject->DeviceExtension;
	/*
	 * we assume buffer is big enough to hold SerialPerfStats structure
	 * caller must verify this
	 */
	RtlCopyMemory(
		pIrp->AssociatedIrp.SystemBuffer,
		&pDeviceExtension->SerialPerfStats,
		sizeof(SERIALPERF_STATS)
	);
	return TRUE;
}

NTSTATUS
SerialGetCommProp(
	OUT PSERIAL_COMMPROP pCommProp,
	IN PSERIAL_DEVICE_EXTENSION DeviceExtension)
{
	RtlZeroMemory(pCommProp, sizeof(SERIAL_COMMPROP));
	
	pCommProp->PacketLength = sizeof(SERIAL_COMMPROP);
	pCommProp->PacketVersion = 2;
	pCommProp->ServiceMask = SERIAL_SP_SERIALCOMM;
	pCommProp->MaxTxQueue = pCommProp->CurrentTxQueue = DeviceExtension->OutputBuffer.Length - 1;
	pCommProp->MaxRxQueue = pCommProp->CurrentRxQueue = DeviceExtension->InputBuffer.Length - 1;
	pCommProp->MaxBaud = SERIAL_BAUD_115200;
	pCommProp->ProvSubType = 1; // PST_RS232;
	/* FIXME: ProvCapabilities may be related to Uart type */
	pCommProp->ProvCapabilities = SERIAL_PCF_DTRDSR | SERIAL_PCF_INTTIMEOUTS | SERIAL_PCF_PARITY_CHECK
		| SERIAL_PCF_RTSCTS | SERIAL_PCF_SETXCHAR | SERIAL_PCF_SPECIALCHARS | SERIAL_PCF_TOTALTIMEOUTS
		| SERIAL_PCF_XONXOFF;
	/* FIXME: SettableParams may be related to Uart type */
	pCommProp->SettableParams = SERIAL_SP_BAUD | SERIAL_SP_DATABITS | SERIAL_SP_HANDSHAKING
		| SERIAL_SP_PARITY | SERIAL_SP_PARITY_CHECK | SERIAL_SP_STOPBITS;
	/* FIXME: SettableBaud may be related to Uart type */
	pCommProp->SettableBaud = SERIAL_BAUD_075 | SERIAL_BAUD_110 | SERIAL_BAUD_134_5
		| SERIAL_BAUD_150 | SERIAL_BAUD_300 | SERIAL_BAUD_600 | SERIAL_BAUD_1200
		| SERIAL_BAUD_1800 | SERIAL_BAUD_2400 | SERIAL_BAUD_4800 | SERIAL_BAUD_7200
		| SERIAL_BAUD_9600 | SERIAL_BAUD_14400 | SERIAL_BAUD_19200 | SERIAL_BAUD_38400
		| SERIAL_BAUD_56K | SERIAL_BAUD_57600 | SERIAL_BAUD_115200 | SERIAL_BAUD_128K
		| SERIAL_BAUD_USER;
	pCommProp->SettableData = SERIAL_DATABITS_5 | SERIAL_DATABITS_6 | SERIAL_DATABITS_7 | SERIAL_DATABITS_8;
	pCommProp->SettableStopParity = SERIAL_STOPBITS_10 | SERIAL_STOPBITS_15 | SERIAL_STOPBITS_20
		| SERIAL_PARITY_NONE | SERIAL_PARITY_ODD | SERIAL_PARITY_EVEN | SERIAL_PARITY_MARK | SERIAL_PARITY_SPACE;
	
	return STATUS_SUCCESS;
}

NTSTATUS STDCALL
SerialDeviceControl(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp)
{
	PIO_STACK_LOCATION Stack;
	PSERIAL_DEVICE_EXTENSION DeviceExtension;
	ULONG LengthIn, LengthOut;
	ULONG Information = 0;
	PUCHAR Buffer;
	PUCHAR ComPortBase;
	NTSTATUS Status;
	
	DPRINT("Serial: IRP_MJ_DEVICE_CONTROL dispatch\n");
	
	/* FIXME: pend operation if possible */
	
	Stack = IoGetCurrentIrpStackLocation(Irp);
	LengthIn = Stack->Parameters.DeviceIoControl.InputBufferLength;
	LengthOut = Stack->Parameters.DeviceIoControl.OutputBufferLength;
	Buffer = Irp->AssociatedIrp.SystemBuffer;
	DeviceExtension = (PSERIAL_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	ComPortBase = (PUCHAR)DeviceExtension->BaseAddress;
	
	/* FIXME: need to probe buffers */
	/* FIXME: see http://www.osronline.com/ddkx/serial/serref_61bm.htm */
	switch (Stack->Parameters.DeviceIoControl.IoControlCode)
	{
		case IOCTL_SERIAL_CLEAR_STATS:
		{
			DPRINT("Serial: IOCTL_SERIAL_CLEAR_STATS\n");
			KeSynchronizeExecution(
				DeviceExtension->Interrupt,
				(PKSYNCHRONIZE_ROUTINE)SerialClearPerfStats,
				&DeviceExtension->SerialPerfStats);
			Status = STATUS_SUCCESS;
			break;
		}
		case IOCTL_SERIAL_CLR_DTR:
		{
			DPRINT("Serial: IOCTL_SERIAL_CLR_DTR\n");
			/* FIXME: If the handshake flow control of the device is configured to 
			 * automatically use DTR, return STATUS_INVALID_PARAMETER */
			DeviceExtension->MCR &= ~SR_MCR_DTR;
			WRITE_PORT_UCHAR(SER_MCR(ComPortBase), DeviceExtension->MCR);
			Status = STATUS_SUCCESS;
			break;
		}
		case IOCTL_SERIAL_CLR_RTS:
		{
			DPRINT("Serial: IOCTL_SERIAL_CLR_RTS\n");
			/* FIXME: If the handshake flow control of the device is configured to 
			 * automatically use RTS, return STATUS_INVALID_PARAMETER */
			DeviceExtension->MCR &= ~SR_MCR_RTS;
			WRITE_PORT_UCHAR(SER_MCR(ComPortBase), DeviceExtension->MCR);
			Status = STATUS_SUCCESS;
			break;
		}
		case IOCTL_SERIAL_CONFIG_SIZE:
		{
			/* Obsolete on Microsoft Windows 2000+ */
			PULONG pConfigSize;
			DPRINT("Serial: IOCTL_SERIAL_CONFIG_SIZE\n");
			if (LengthOut != sizeof(ULONG) || Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
			{
				pConfigSize = (PULONG)Buffer;
				*pConfigSize = 0;
				Status = STATUS_SUCCESS;
			}
			break;
		}
		case IOCTL_SERIAL_GET_BAUD_RATE:
		{
			DPRINT("Serial: IOCTL_SERIAL_GET_BAUD_RATE\n");
			if (LengthOut < sizeof(SERIAL_BAUD_RATE))
				Status = STATUS_BUFFER_TOO_SMALL;
			else if (Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
			{
				((PSERIAL_BAUD_RATE)Buffer)->BaudRate = DeviceExtension->BaudRate;
				Information = sizeof(SERIAL_BAUD_RATE);
				Status = STATUS_SUCCESS;
			}
		}
		case IOCTL_SERIAL_GET_CHARS:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_GET_CHARS not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_GET_COMMSTATUS:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_GET_COMMSTATUS not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_GET_DTRRTS:
		{
			PULONG pDtrRts;
			DPRINT("Serial: IOCTL_SERIAL_GET_DTRRTS\n");
			if (LengthOut != sizeof(ULONG) || Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
			{
				pDtrRts = (PULONG)Buffer;
				*pDtrRts = 0;
				if (DeviceExtension->MCR & SR_MCR_DTR)
					*pDtrRts |= SERIAL_DTR_STATE;
				if (DeviceExtension->MCR & SR_MCR_RTS)
					*pDtrRts |= SERIAL_RTS_STATE;
				Status = STATUS_SUCCESS;
			}
			break;
		}
		case IOCTL_SERIAL_GET_HANDFLOW:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_GET_HANDFLOW not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_GET_LINE_CONTROL:
		{
			DPRINT("Serial: IOCTL_SERIAL_GET_LINE_CONTROL\n");
			if (LengthOut < sizeof(SERIAL_LINE_CONTROL))
				Status = STATUS_BUFFER_TOO_SMALL;
			else if (Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
			{
				*((PSERIAL_LINE_CONTROL)Buffer) = DeviceExtension->SerialLineControl;
				Information = sizeof(SERIAL_LINE_CONTROL);
				Status = STATUS_SUCCESS;
			}
			break;
		}
		case IOCTL_SERIAL_GET_MODEM_CONTROL:
		{
			PULONG pMCR;
			DPRINT("Serial: IOCTL_SERIAL_GET_MODEM_CONTROL\n");
			if (LengthOut != sizeof(ULONG) || Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
			{
				pMCR = (PULONG)Buffer;
				*pMCR = DeviceExtension->MCR;
				Status = STATUS_SUCCESS;
			}
			break;
		}
		case IOCTL_SERIAL_GET_MODEMSTATUS:
		{
			PULONG pMSR;
			DPRINT("Serial: IOCTL_SERIAL_GET_MODEMSTATUS\n");
			if (LengthOut != sizeof(ULONG) || Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
			{
				pMSR = (PULONG)Buffer;
				*pMSR = DeviceExtension->MSR;
				Status = STATUS_SUCCESS;
			}
			break;
		}
		case IOCTL_SERIAL_GET_PROPERTIES:
		{
			DPRINT("Serial: IOCTL_SERIAL_GET_PROPERTIES\n");
			if (LengthOut < sizeof(SERIAL_COMMPROP))
			{
				DPRINT("Serial: return STATUS_BUFFER_TOO_SMALL\n");
				Status = STATUS_BUFFER_TOO_SMALL;
			}
			else if (Buffer == NULL)
			{
				DPRINT("Serial: return STATUS_INVALID_PARAMETER\n");
				Status = STATUS_INVALID_PARAMETER;
			}
			else
			{
				Status = SerialGetCommProp((PSERIAL_COMMPROP)Buffer, DeviceExtension);
				Information = sizeof(SERIAL_COMMPROP);
			}
			break;
		}
		case IOCTL_SERIAL_GET_STATS:
		{
			DPRINT("Serial: IOCTL_SERIAL_GET_STATS\n");
			if (LengthOut < sizeof(SERIALPERF_STATS))
			{
				DPRINT("Serial: return STATUS_BUFFER_TOO_SMALL\n");
				Status = STATUS_BUFFER_TOO_SMALL;
			}
			else if (Buffer == NULL)
			{
				DPRINT("Serial: return STATUS_INVALID_PARAMETER\n");
				Status = STATUS_INVALID_PARAMETER;
			}
			else
			{
				KeSynchronizeExecution(DeviceExtension->Interrupt,
					(PKSYNCHRONIZE_ROUTINE)SerialGetPerfStats, Irp);
				Status = STATUS_SUCCESS;
				Information = sizeof(SERIALPERF_STATS);
			}
			break;
		}
		case IOCTL_SERIAL_GET_TIMEOUTS:
		{
			DPRINT("Serial: IOCTL_SERIAL_GET_TIMEOUTS\n");
			if (LengthOut != sizeof(SERIAL_TIMEOUTS) || Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
			{
				*(PSERIAL_TIMEOUTS)Buffer = DeviceExtension->SerialTimeOuts;
				Status = STATUS_SUCCESS;
			}
			break;
		}
		case IOCTL_SERIAL_GET_WAIT_MASK:
		{
			PULONG pWaitMask;
			DPRINT("Serial: IOCTL_SERIAL_GET_WAIT_MASK\n");
			if (LengthOut != sizeof(ULONG) || Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
			{
				pWaitMask = (PULONG)Buffer;
				*pWaitMask = DeviceExtension->WaitMask;
				Status = STATUS_SUCCESS;
			}
			break;
		}
		case IOCTL_SERIAL_IMMEDIATE_CHAR:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_IMMEDIATE_CHAR not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_LSRMST_INSERT:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_LSRMST_INSERT not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_PURGE:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_PURGE not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_RESET_DEVICE:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_RESET_DEVICE not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_SET_BAUD_RATE:
		{
			PULONG pNewBaudRate;
			DPRINT("Serial: IOCTL_SERIAL_SET_BAUD_RATE\n");
			if (LengthIn != sizeof(ULONG) || Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
			{
				pNewBaudRate = (PULONG)Buffer;
				Status = SerialSetBaudRate(DeviceExtension, *pNewBaudRate);
			}
			break;
		}
		case IOCTL_SERIAL_SET_BREAK_OFF:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_SET_BREAK_OFF not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_SET_BREAK_ON:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_SET_BREAK_ON not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_SET_CHARS:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_SET_CHARS not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_SET_DTR:
		{
			/* FIXME: If the handshake flow control of the device is configured to 
			 * automatically use DTR, return STATUS_INVALID_PARAMETER */
			DPRINT("Serial: IOCTL_SERIAL_SET_DTR\n");
			if (!(DeviceExtension->MCR & SR_MCR_DTR))
			{
				DeviceExtension->MCR |= SR_MCR_DTR;
				WRITE_PORT_UCHAR(SER_MCR(ComPortBase), DeviceExtension->MCR);
			}
			Status = STATUS_SUCCESS;
			break;
		}
		case IOCTL_SERIAL_SET_FIFO_CONTROL:
		{
			DPRINT("Serial: IOCTL_SERIAL_SET_FIFO_CONTROL\n");
			if (LengthIn != sizeof(ULONG) || Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
			{
				WRITE_PORT_UCHAR(SER_FCR(ComPortBase), (UCHAR)((*(PULONG)Buffer) & 0xff));
				Status = STATUS_SUCCESS;
			}
			break;
		}
		case IOCTL_SERIAL_SET_HANDFLOW:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_SET_HANDFLOW not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_SET_LINE_CONTROL:
		{
			DPRINT("Serial: IOCTL_SERIAL_SET_LINE_CONTROL\n");
			if (LengthIn < sizeof(SERIAL_LINE_CONTROL))
				Status = STATUS_BUFFER_TOO_SMALL;
			else if (Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
				Status = SerialSetLineControl(DeviceExtension, (PSERIAL_LINE_CONTROL)Buffer);
			break;
		}
		case IOCTL_SERIAL_SET_MODEM_CONTROL:
		{
			PULONG pMCR;
			DPRINT("Serial: IOCTL_SERIAL_SET_MODEM_CONTROL\n");
			if (LengthIn != sizeof(ULONG) || Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
			{
				pMCR = (PULONG)Buffer;
				DeviceExtension->MCR = (UCHAR)(*pMCR & 0xff);
				WRITE_PORT_UCHAR(SER_MCR(ComPortBase), DeviceExtension->MCR);
				Status = STATUS_SUCCESS;
			}
			break;
		}
		case IOCTL_SERIAL_SET_QUEUE_SIZE:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_SET_QUEUE_SIZE not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_SET_RTS:
		{
			/* FIXME: If the handshake flow control of the device is configured to 
			 * automatically use DTR, return STATUS_INVALID_PARAMETER */
			DPRINT("Serial: IOCTL_SERIAL_SET_RTS\n");
			if (!(DeviceExtension->MCR & SR_MCR_RTS))
			{
				DeviceExtension->MCR |= SR_MCR_RTS;
				WRITE_PORT_UCHAR(SER_MCR(ComPortBase), DeviceExtension->MCR);
			}
			Status = STATUS_SUCCESS;
			break;
		}
		case IOCTL_SERIAL_SET_TIMEOUTS:
		{
			DPRINT("Serial: IOCTL_SERIAL_SET_TIMEOUTS\n");
			if (LengthIn != sizeof(SERIAL_TIMEOUTS) || Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
			{
				DeviceExtension->SerialTimeOuts = *(PSERIAL_TIMEOUTS)Buffer;
				Status = STATUS_SUCCESS;
			}
			break;
		}
		case IOCTL_SERIAL_SET_WAIT_MASK:
		{
			PULONG pWaitMask;
			DPRINT("Serial: IOCTL_SERIAL_SET_WAIT_MASK\n");
			if (LengthIn != sizeof(ULONG) || Buffer == NULL)
				Status = STATUS_INVALID_PARAMETER;
			else
			{
				pWaitMask = (PULONG)Buffer;
				DeviceExtension->WaitMask = *pWaitMask;
				Status = STATUS_SUCCESS;
			}
			break;
		}
		case IOCTL_SERIAL_SET_XOFF:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_SET_XOFF not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_SET_XON:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_SET_XON not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_WAIT_ON_MASK:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_WAIT_ON_MASK not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_SERIAL_XOFF_COUNTER:
		{
			/* FIXME */
			DPRINT1("Serial: IOCTL_SERIAL_XOFF_COUNTER not implemented.\n");
			Status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		default:
		{
			/* Pass Irp to lower driver */
			DPRINT("Serial: Unknown IOCTL code 0x%x\n", Stack->Parameters.DeviceIoControl.IoControlCode);
			IoSkipCurrentIrpStackLocation(Irp);
			return IoCallDriver(DeviceExtension->LowerDevice, Irp);
		}
	}
	
	Irp->IoStatus.Information = Information;
	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Status;
}
