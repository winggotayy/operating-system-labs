#include "x86.h"
#include "device.h"

#include <stddef.h>

#define SYS_WRITE 0
#define SYS_READ 1
#define SYS_FORK 2
#define SYS_EXEC 3
#define SYS_SLEEP 4
#define SYS_EXIT 5
#define SYS_SEM 6

#define STD_OUT 0
#define STD_IN 1

#define SEM_INIT 0
#define SEM_WAIT 1
#define SEM_POST 2
#define SEM_DESTROY 3

extern TSS tss;

extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern Semaphore sem[MAX_SEM_NUM];
extern Device dev[MAX_DEV_NUM];

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

void GProtectFaultHandle(struct StackFrame *sf);
void timerHandle(struct StackFrame *sf);
void keyboardHandle(struct StackFrame *sf);
void syscallHandle(struct StackFrame *sf);

void syscallWrite(struct StackFrame *sf);
void syscallRead(struct StackFrame *sf);
void syscallFork(struct StackFrame *sf);
void syscallExec(struct StackFrame *sf);
void syscallSleep(struct StackFrame *sf);
void syscallExit(struct StackFrame *sf);
void syscallSem(struct StackFrame *sf);

void syscallWriteStdOut(struct StackFrame *sf);

void syscallReadStdIn(struct StackFrame *sf);

void syscallSemInit(struct StackFrame *sf);
void syscallSemWait(struct StackFrame *sf);
void syscallSemPost(struct StackFrame *sf);
void syscallSemDestroy(struct StackFrame *sf);

void irqHandle(struct StackFrame *sf) { // pointer sf = esp
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	/* Save esp to stackTop */
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].prevStackTop = pcb[current].stackTop;
	pcb[current].stackTop = (uint32_t)sf;

	switch(sf->irq) {
		case -1:
			break;
		case 0xd:
			GProtectFaultHandle(sf);
			break;
		case 0x20:
			timerHandle(sf);
			break;
		case 0x21:
			keyboardHandle(sf);
			break;
		case 0x80:
			syscallHandle(sf);
			break;
		default:assert(0);
	}
	/* Recover stackTop */
	pcb[current].stackTop = tmpStackTop;
}

void GProtectFaultHandle(struct StackFrame *sf) {
	assert(0);
	return;
}

void timerHandle(struct StackFrame *sf) {
	int i;
	uint32_t tmpStackTop;
	i = (current+1) % MAX_PCB_NUM;
	while (i != current) {
		if (pcb[i].state == STATE_BLOCKED && pcb[i].sleepTime != -1) {
			pcb[i].sleepTime --;
			if (pcb[i].sleepTime == 0)
				pcb[i].state = STATE_RUNNABLE;
		}
		i = (i+1) % MAX_PCB_NUM;
	}

	if (pcb[current].state == STATE_RUNNING &&
		pcb[current].timeCount != MAX_TIME_COUNT) {
		pcb[current].timeCount++;
		return;
	}
	else {
		if (pcb[current].state == STATE_RUNNING) {
			pcb[current].state = STATE_RUNNABLE;
			pcb[current].timeCount = 0;
		}
		
		i = (current+1) % MAX_PCB_NUM;
		while (i != current) {
			if (i !=0 && pcb[i].state == STATE_RUNNABLE)
				break;
			i = (i+1) % MAX_PCB_NUM;
		}
		if (pcb[i].state != STATE_RUNNABLE)
			i = 0;
		current = i;
		/* echo pid of selected process */
		//putChar('0'+current);
		pcb[current].state = STATE_RUNNING;
		pcb[current].timeCount = 1;
		/* recover stackTop of selected process */
		tmpStackTop = pcb[current].stackTop;
		pcb[current].stackTop = pcb[current].prevStackTop;
		tss.esp0 = (uint32_t)&(pcb[current].stackTop); // setting tss for user process
		asm volatile("movl %0, %%esp"::"m"(tmpStackTop)); // switch kernel stack
		asm volatile("popl %gs");
		asm volatile("popl %fs");
		asm volatile("popl %es");
		asm volatile("popl %ds");
		asm volatile("popal");
		asm volatile("addl $8, %esp");
		asm volatile("iret");
	}
}

// 键盘中断处理函数
void keyboardHandle(struct StackFrame *sf) {
	ProcessTable *pt = NULL; // 进程表指针
	uint32_t keyCode = getKeyCode(); // 获取键盘键码
	if (keyCode == 0) // illegal keyCode
		return;

        /* Implementation */
        // 缓冲区溢出检查
        if ((bufferTail + 1) % MAX_KEYBUFFER_SIZE == bufferHead) 
        {
                // 如果缓冲区已满，处理溢出（在这里，只是简单返回，不做处理）
                return;
        }

	//putChar(getChar(keyCode));
	keyBuffer[bufferTail] = keyCode; // 将键码插入到键盘缓冲区
	bufferTail=(bufferTail+1)%MAX_KEYBUFFER_SIZE; // 更新缓冲区尾指针

	if (dev[STD_IN].value < 0) { // with process blocked // 检查是否有进程因等待标准输入而阻塞
		// TODO: deal with blocked situation // 处理阻塞的进程
                dev[STD_IN].value ++; // 增加设备值以解除阻塞

                // 获取被阻塞进程表项
		pt = (ProcessTable*)((uint32_t)(dev[STD_IN].pcb.prev) - offsetof(ProcessTable, blocked));
                // 更新进程状态为可运行，并重置睡眠时间
		pt->state = STATE_RUNNABLE;
		pt->sleepTime = 0;

                // 从阻塞队列中移除解除阻塞的进程
		dev[STD_IN].pcb.prev = (dev[STD_IN].pcb.prev)->prev;
		(dev[STD_IN].pcb.prev)->next = &(dev[STD_IN].pcb);
	}

	return;
}

void syscallHandle(struct StackFrame *sf) {
	switch(sf->eax) { // syscall number
		case SYS_WRITE:
			syscallWrite(sf);
			break; // for SYS_WRITE
		case SYS_READ:
			syscallRead(sf);
			break; // for SYS_READ
		case SYS_FORK:
			syscallFork(sf);
			break; // for SYS_FORK
		case SYS_EXEC:
			syscallExec(sf);
			break; // for SYS_EXEC
		case SYS_SLEEP:
			syscallSleep(sf);
			break; // for SYS_SLEEP
		case SYS_EXIT:
			syscallExit(sf);
			break; // for SYS_EXIT
		case SYS_SEM:
			syscallSem(sf);
			break; // for SYS_SEM
		default:break;
	}
}

void syscallWrite(struct StackFrame *sf) {
	switch(sf->ecx) { // file descriptor
		case STD_OUT:
			if (dev[STD_OUT].state == 1)
				syscallWriteStdOut(sf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallWriteStdOut(struct StackFrame *sf) {
	int sel = sf->ds; // segment selector for user data, need further modification
	char *str = (char*)sf->edx;
	int size = sf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		if(character == '\n') {
			displayRow++;
			displayCol=0;
			if(displayRow==MAX_ROW){
				displayRow=MAX_ROW-1;
				displayCol=0;
				scrollScreen();
			}
		}
		else {
			data = character | (0x0c << 8);
			pos = (MAX_COL*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol++;
			if(displayCol==MAX_COL){
				displayRow++;
				displayCol=0;
				if(displayRow==MAX_ROW){
					displayRow=MAX_ROW-1;
					displayCol=0;
					scrollScreen();
				}
			}
		}
	}
	
	updateCursor(displayRow, displayCol);
	return;
}

void syscallRead(struct StackFrame *sf) {
	switch(sf->ecx) {
		case STD_IN:
			if (dev[STD_IN].state == 1)
				syscallReadStdIn(sf);
			break; // for STD_IN
		default:
			break;
	}
}

void syscallReadStdIn(struct StackFrame *sf) {
	// TODO: complete `stdin`
        if (dev[STD_IN].value == 0) // 没有进程阻塞 
        { 
                // 为I/O阻塞当前进程
                dev[STD_IN].value--;

                // 将当前进程插入到阻塞队列中
                ProcessTable *currentProcess = &pcb[current];
                currentProcess->blocked.next = dev[STD_IN].pcb.next;
                currentProcess->blocked.prev = &(dev[STD_IN].pcb);
                dev[STD_IN].pcb.next = &(currentProcess->blocked);
                currentProcess->blocked.next->prev = &(currentProcess->blocked);

                // 设置当前进程状态为阻塞
                currentProcess->state = STATE_BLOCKED;
                currentProcess->sleepTime = -1; // 在标准输入上阻塞

                // 重置缓冲区头和尾
                bufferHead = bufferTail;
                asm volatile("int $0x20"); // 触发上下文切换

                // 从阻塞状态恢复
                int sel = sf->ds;
                char *buffer = (char *)sf->edx;
                int maxSize = sf->ebx; // MAX_BUFFER_SIZE
                int bytesRead = 0;
                char character = 0;

                // 设置段寄存器
                asm volatile("movw %0, %%es"::"m"(sel));

                // 从键盘缓冲区读取字符
                while (bytesRead < maxSize - 1) 
                {
                        if (bufferHead != bufferTail) // 检查键盘缓冲区是否为空
                        { 
                                character = getChar(keyBuffer[bufferHead]);
                                bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;
                                putChar(character); // 将字符输出到屏幕
                                if (character != 0) 
                                {
                                        // 将字符存储到用户提供的缓冲区
                                        asm volatile("movb %0, %%es:(%1)"::"r"(character), "r"(buffer + bytesRead));
                                        bytesRead++;
                                }
                        } 
                        else 
                        {
                                break; // Exit loop if key buffer is empty
                        }
                }

                // 将缓冲区以空字符结尾
                asm volatile("movb $0x00, %%es:(%0)"::"r"(buffer + bytesRead));
                currentProcess->regs.eax = bytesRead; // 返回读取的字符数
        } 
        else if (dev[STD_IN].value < 0) // 已经有进程阻塞 
        { 
                pcb[current].regs.eax = -1; // Return error
        }
}

void syscallFork(struct StackFrame *sf) {
	int i, j;
	for (i = 0; i < MAX_PCB_NUM; i++) {
		if (pcb[i].state == STATE_DEAD)
			break;
	}
	if (i != MAX_PCB_NUM) {
		/* copy userspace
		   enable interrupt
		 */
		enableInterrupt();
		for (j = 0; j < 0x100000; j++) {
			*(uint8_t *)(j + (i+1)*0x100000) = *(uint8_t *)(j + (current+1)*0x100000);
			//asm volatile("int $0x20"); // Testing irqTimer during syscall
		}
		/* disable interrupt
		 */
		disableInterrupt();
		/* set pcb
		   pcb[i]=pcb[current] doesn't work
		*/
		pcb[i].stackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].stackTop);
		pcb[i].prevStackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].prevStackTop);
		pcb[i].state = STATE_RUNNABLE;
		pcb[i].timeCount = pcb[current].timeCount;
		pcb[i].sleepTime = pcb[current].sleepTime;
		pcb[i].pid = i;
		/* set regs */
		pcb[i].regs.ss = USEL(2+i*2);
		pcb[i].regs.esp = pcb[current].regs.esp;
		pcb[i].regs.eflags = pcb[current].regs.eflags;
		pcb[i].regs.cs = USEL(1+i*2);
		pcb[i].regs.eip = pcb[current].regs.eip;
		pcb[i].regs.eax = pcb[current].regs.eax;
		pcb[i].regs.ecx = pcb[current].regs.ecx;
		pcb[i].regs.edx = pcb[current].regs.edx;
		pcb[i].regs.ebx = pcb[current].regs.ebx;
		pcb[i].regs.xxx = pcb[current].regs.xxx;
		pcb[i].regs.ebp = pcb[current].regs.ebp;
		pcb[i].regs.esi = pcb[current].regs.esi;
		pcb[i].regs.edi = pcb[current].regs.edi;
		pcb[i].regs.ds = USEL(2+i*2);
		pcb[i].regs.es = pcb[current].regs.es;
		pcb[i].regs.fs = pcb[current].regs.fs;
		pcb[i].regs.gs = pcb[current].regs.gs;
		/* set return value */
		pcb[i].regs.eax = 0;
		pcb[current].regs.eax = i;
	}
	else {
		pcb[current].regs.eax = -1;
	}
	return;
}

void syscallExec(struct StackFrame *sf) {
	return;
}

void syscallSleep(struct StackFrame *sf) {
	if (sf->ecx == 0)
		return;
	else {
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = sf->ecx;
		asm volatile("int $0x20");
		return;
	}
}

void syscallExit(struct StackFrame *sf) {
	pcb[current].state = STATE_DEAD;
	asm volatile("int $0x20");
	return;
}

void syscallSem(struct StackFrame *sf) {
	switch(sf->ecx) {
		case SEM_INIT:
			syscallSemInit(sf);
			break;
		case SEM_WAIT:
			syscallSemWait(sf);
			break;
		case SEM_POST:
			syscallSemPost(sf);
			break;
		case SEM_DESTROY:
			syscallSemDestroy(sf);
			break;
		default:break;
	}
}

void syscallSemInit(struct StackFrame *sf) {
	// TODO: complete `SemInit`
        int idx;

        // 遍历查找未使用的信号量槽
        for (idx = 0; idx < MAX_SEM_NUM && sem[idx].state != 0; idx++);

        // 将找到的索引返回给当前进程
        pcb[current].regs.eax = (idx < MAX_SEM_NUM) ? idx : -1;

        // 如果找到未使用的信号量槽，则进行初始化
        if (idx < MAX_SEM_NUM) 
        {
                sem[idx].state = 1;  // 标记信号量为已使用
                sem[idx].value = (int32_t)sf->edx;  // 初始化信号量的值
                sem[idx].pcb.next = &(sem[idx].pcb);  // 初始化阻塞队列
                sem[idx].pcb.prev = &(sem[idx].pcb);
        }
}

void syscallSemWait(struct StackFrame *sf) {
	// TODO: complete `SemWait` and note that you need to consider some special situations
        int idx = (int)sf->edx;

        // 检查信号量索引是否有效
        if (idx < 0 || idx >= MAX_SEM_NUM || sem[idx].state == 0) 
        {
                pcb[current].regs.eax = -1; // 返回错误代码
                return;
        }

        // 检查信号量的值
        if (sem[idx].value > 0) 
        {
                // 如果信号量值大于 0，直接减 1 并返回
                sem[idx].value--;
                pcb[current].regs.eax = 0; // 操作成功
        } 
        else 
        {
                // 如果信号量值小于等于 0，将当前进程阻塞在该信号量上
                sem[idx].value--;

                // 获取当前进程表
                ProcessTable *currentProcess = &pcb[current];

                // 将当前进程插入信号量的阻塞队列
                currentProcess->blocked.next = sem[idx].pcb.next;
                currentProcess->blocked.prev = &(sem[idx].pcb);
                sem[idx].pcb.next = &(currentProcess->blocked);
                currentProcess->blocked.next->prev = &(currentProcess->blocked);

                // 设置当前进程状态为阻塞并进行上下文切换
                currentProcess->state = STATE_BLOCKED;
                currentProcess->sleepTime = -1;
                asm volatile("int $0x20");

                // 上下文切换后恢复执行
                pcb[current].regs.eax = 0; // 操作成功
        }
}

void syscallSemPost(struct StackFrame *sf) {
	int i = (int)sf->edx;
	ProcessTable *pt = NULL;
	if (i < 0 || i >= MAX_SEM_NUM) {
		pcb[current].regs.eax = -1;
		return;
	}
	// TODO: complete other situations
        // 检查信号量是否被使用
        // not in use
        if (sem[i].state == 0) 
        { 
		pcb[current].regs.eax = -1; // 返回错误代码
		return;
	}
        // 检查是否有进程被阻塞在该信号量上
        // no process blocked
	else if (sem[i].value >= 0) 
        { 
		sem[i].value ++; // 增加信号量的值
		pcb[current].regs.eax = 0; // 操作成功
		return;
	}
        // 有进程被阻塞在该信号量上，释放一个被阻塞的进程
        // release process blocked on this sem
	else if (sem[i].value < 0) 
        {  
		sem[i].value ++; // 增加信号量的值

                // 获取被阻塞的进程表项
		pt = (ProcessTable*)((uint32_t)(sem[i].pcb.prev) - offsetof(ProcessTable, blocked));
		pt->state = STATE_RUNNABLE; // 设置进程状态为可运行
		pt->sleepTime = 0; // 重置睡眠时间

                // 从信号量的阻塞队列中移除被阻塞的进程
		sem[i].pcb.prev = (sem[i].pcb.prev)->prev;
		(sem[i].pcb.prev)->next = &(sem[i].pcb);
		
		pcb[current].regs.eax = 0; // 操作成功
		return;
	}
}

void syscallSemDestroy(struct StackFrame *sf) {
	// TODO: complete `SemDestroy`
        int i = (int)sf->edx;
        // 检查信号量索引是否有效
	if (i < 0 || i >= MAX_SEM_NUM) 
        {
		pcb[current].regs.eax = -1; // 返回错误代码
		return;
	}
        // 检查信号量是否被使用
        // not in use
	if (sem[i].state == 0) 
        { 
		pcb[current].regs.eax = -1; // 返回错误代码
		return;
	}
	// 将信号量设置为未使用状态
        sem[i].state = 0;
	sem[i].value = 0;
	sem[i].pcb.next = &(sem[i].pcb);
	sem[i].pcb.prev = &(sem[i].pcb);

	pcb[current].regs.eax = 0; // 操作成功

	return;
}
