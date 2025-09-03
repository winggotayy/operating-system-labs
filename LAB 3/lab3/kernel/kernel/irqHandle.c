#include "x86.h"
#include "device.h"

extern TSS tss;
extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern int displayRow;
extern int displayCol;

void GProtectFaultHandle(struct StackFrame *sf);

void syscallHandle(struct StackFrame *sf);

void syscallWrite(struct StackFrame *sf);
void syscallPrint(struct StackFrame *sf);

void timerHandle(struct StackFrame *sf);
void syscallFork(struct StackFrame *sf);
void syscallWrite(struct StackFrame *sf);
void syscallSleep(struct StackFrame *sf);
void syscallExit(struct StackFrame *sf);

void irqHandle(struct StackFrame *sf)
{ // pointer sf = esp
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds" ::"a"(KSEL(SEG_KDATA)));
	/*XXX Save esp to stackTop */
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].prevStackTop = pcb[current].stackTop;
	pcb[current].stackTop = (uint32_t)sf;

	switch (sf->irq)
	{
	case -1:
		break;
	case 0xd:
		GProtectFaultHandle(sf);
		break;
	case 0x20:
		timerHandle(sf);
		break;
	case 0x80:
		syscallHandle(sf);
		break;
	default:
		assert(0);
	}
	/*XXX Recover stackTop */
	pcb[current].stackTop = tmpStackTop;
}

void GProtectFaultHandle(struct StackFrame *sf)
{
	assert(0);
	return;
}

void timerHandle(struct StackFrame* sf)
{
	// TODO
	int i;                  // 循环计数器
	uint32_t tmpStackTop;   // 临时存储堆栈顶部指针

	// 遍历所有 pcb 进程，检查阻塞的进程是否需要减少睡眠时间并唤醒
	for (i = (current + 1) % MAX_PCB_NUM; i != current; i = (i + 1) % MAX_PCB_NUM)
	{
		if (pcb[i].state == STATE_BLOCKED && pcb[i].sleepTime != -1)
		{
			pcb[i].sleepTime -= 1;   // 减少睡眠时间
			// 如果睡眠时间为0
			if (pcb[i].sleepTime == 0)
			{
				pcb[i].state = STATE_RUNNABLE;   // 进程状态 设置为 RUNNABLE
			}
		}
	}

	// 当前进程正在运行
	if (pcb[current].state == STATE_RUNNING && pcb[current].timeCount != MAX_TIME_COUNT)
	{
		// 未达最大运行时间
		pcb[current].timeCount += 1;   // 增加其运行时间并返回
		return;
	}
	else
	{
		// 已达到最大运行时间
		if (pcb[current].state == STATE_RUNNING)
		{
			pcb[current].state = STATE_RUNNABLE;   // 进程状态 设置为 RUNNABLE
			pcb[current].timeCount = 0;   // 重置运行时间
		}
		// 进行进程和堆栈切换：
		// 在 pcb 包含的进程中选择下一个 RUNNABLE 的进程
		for (i = (current + 1) % MAX_PCB_NUM; i != current; i = (i + 1) % MAX_PCB_NUM)
		{
			if (i != 0 && pcb[i].state == STATE_RUNNABLE)
				break;
		}

		// 如果没有其他 RUNNABLE 的进程，则选择空闲进程
		if (pcb[i].state != STATE_RUNNABLE)
			i = 0;
		current = i;

		/* 输出选定进程的PID */
		//putChar('0'+current);

		// 将选定的进程状态设置为 RUNNING，并将其运行时间重置为 1
		pcb[current].state = STATE_RUNNING;
		pcb[current].timeCount = 1;

		/* 恢复选定进程的堆栈顶部 */
		tmpStackTop = pcb[current].stackTop;   // 将选定进程的堆栈顶部指针存储到临时变量中
		pcb[current].stackTop = pcb[current].prevStackTop;   // 恢复选定进程的堆栈顶部
		tss.esp0 = (uint32_t) & (pcb[current].stackTop); // 设置TSS以供用户进程使用

		// 切换内核堆栈和恢复寄存器状态，使处理器转到选定进程的执行上下文
		asm volatile("movl %0, %%esp"::"m"(tmpStackTop)); // 切换内核栈
		asm volatile("popl %gs");
		asm volatile("popl %fs");
		asm volatile("popl %es");
		asm volatile("popl %ds");
		asm volatile("popal");
		asm volatile("addl $8, %esp");
		asm volatile("iret");
	}
}

void syscallHandle(struct StackFrame *sf)
{
	switch (sf->eax)
	{ // syscall number
	case 0:
		syscallWrite(sf);
		break; // for SYS_WRITE
	/*TODO Add Fork,Sleep... */
	case 1:
		syscallFork(sf);
		break; // for SYS_FORK
/*
	case 2:
		break;
*/
	case 3:
		syscallSleep(sf);
		break; // for SYS_SLEEP
	case 4:
		syscallExit(sf);
		break; // for SYS_EXIT
	default:
		break;
	}
}

void syscallWrite(struct StackFrame *sf)
{
	switch (sf->ecx)
	{ // file descriptor
	case 0:
		syscallPrint(sf);
		break; // for STD_OUT
	default:
		break;
	}
}

void syscallPrint(struct StackFrame *sf)
{
	int sel = sf->ds; // segment selector for user data, need further modification
	char *str = (char *)sf->edx;
	int size = sf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	for (i = 0; i < size; i++)
	{
		asm volatile("movb %%es:(%1), %0" : "=r"(character) : "r"(str + i));
		if (character == '\n')
		{
			displayRow++;
			displayCol = 0;
			if (displayRow == 25)
			{
				displayRow = 24;
				displayCol = 0;
				scrollScreen();
			}
		}
		else
		{
			data = character | (0x0c << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)" ::"r"(data), "r"(pos + 0xb8000));
			displayCol++;
			if (displayCol == 80)
			{
				displayRow++;
				displayCol = 0;
				if (displayRow == 25)
				{
					displayRow = 24;
					displayCol = 0;
					scrollScreen();
				}
			}
		}
		// asm volatile("int $0x20"); //XXX Testing irqTimer during syscall
		// asm volatile("int $0x20":::"memory"); //XXX Testing irqTimer during syscall
	}

	updateCursor(displayRow, displayCol);
	// take care of return value
	return;
}

// TODO syscallFork ...
void syscallFork(struct StackFrame* sf)
{
    int i;
    for (i = 0; i < MAX_PCB_NUM; i++)
    {
        if (pcb[i].state == STATE_DEAD)
            break;
    }
    if (i != MAX_PCB_NUM)
    {
        enableInterrupt();

        // 复制用户空间数据
        for (int j = 0; j < 0x100000; j++)
        {
            *(uint8_t*)(j + (i + 1) * 0x100000) = *(uint8_t*)(j + (current + 1) * 0x100000);
        }

        disableInterrupt();

        // 设置新 PCB
        uint32_t offset = (uint32_t)&pcb[i] - (uint32_t)&pcb[current];
        pcb[i].prevStackTop = pcb[current].prevStackTop + offset;
        pcb[i].stackTop = pcb[current].stackTop + offset;
        pcb[i].sleepTime = 0;
        pcb[i].state = STATE_RUNNABLE;
        pcb[i].timeCount = 0;

        // 设置寄存器
        pcb[i].regs.ss = pcb[i].regs.cs = pcb[i].regs.ds = pcb[i].regs.es = pcb[i].regs.fs = pcb[i].regs.gs = USEL(2 + i * 2);
        pcb[i].regs.eax = pcb[current].regs.eax;
        pcb[i].regs.ecx = pcb[current].regs.ecx;
        pcb[i].regs.edx = pcb[current].regs.edx;
        pcb[i].regs.ebx = pcb[current].regs.ebx;
        pcb[i].regs.xxx = pcb[current].regs.xxx;
        pcb[i].regs.ebp = pcb[current].regs.ebp;
        pcb[i].regs.esi = pcb[current].regs.esi;
        pcb[i].regs.edi = pcb[current].regs.edi;
        pcb[i].regs.eflags = pcb[current].regs.eflags;
        pcb[i].regs.eip = pcb[current].regs.eip;
        pcb[i].regs.esp = pcb[current].regs.esp;
        pcb[i].regs.cs = USEL(1 + i * 2);

        // 设置返回值
        pcb[i].regs.eax = 0;
        pcb[current].regs.eax = i;
    }
    else
    {
        pcb[current].regs.eax = -1;
    }
    return;
}

void syscallSleep(struct StackFrame* sf)
{
    if (sf->ecx <= 0)
    {
        // 如果传入的睡眠时间小于等于0，直接返回，不进行睡眠操作
        return;
    }
    else
    {
        // 否则，将当前进程的状态设置为阻塞状态
        pcb[current].state = STATE_BLOCKED;
        // 设置当前进程的睡眠时间为传入的睡眠时间
        pcb[current].sleepTime = sf->ecx;
        // 发送中断请求，进行睡眠
        asm volatile("int $0x20");
        return;
    }
}


void syscallExit(struct StackFrame* sf) 
{
    // 将当前进程的状态设置为死亡状态
    pcb[current].state = STATE_DEAD;
    // 发送中断请求，通知系统当前进程已退出
    asm volatile("int $0x20");
    return;
}

