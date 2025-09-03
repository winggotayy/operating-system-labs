#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

int tail=0;

void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);

#define MAX_SIZE 256

static int convertDecimalToString(int decimal, char *outputBuffer, int bufferSize, int outputIndex) 
{
	int currentIndex = 0;
	int temp;
	int digits[16]; // 用于存储每一位的数字

        // 检查是否为负数
	if(decimal < 0)
        {
		outputBuffer[outputIndex]='-'; // 将负号放入输出缓冲区
		outputIndex++;
		if(outputIndex == bufferSize) 
		{
    	                // 若缓冲区满，则执行输出操作
			outputIndex = 0;
		}
                temp = decimal / 10;
		digits[currentIndex] = temp * 10 - decimal; // 获取个位数
		decimal = temp;
		currentIndex++;
		while(decimal != 0)
                {
			temp = decimal / 10;
			digits[currentIndex] = temp * 10 - decimal; // 获取十位、百位等数字
			decimal = temp;
			currentIndex++;
		}
	}
	else
        {
		temp = decimal / 10;
		digits[currentIndex] = decimal - temp * 10; // 获取个位数
		decimal = temp;
		currentIndex++;
		while(decimal != 0)
                {
			temp = decimal / 10;
			digits[currentIndex] = decimal - temp * 10; // 获取十位、百位等数字
			decimal = temp;
			currentIndex++;
		}
	}

        // 将数字转换为字符并存储到输出缓冲区中
	while(currentIndex != 0)
        {
		outputBuffer[outputIndex] = digits[currentIndex-1] + '0';
		outputIndex++;
		if(outputIndex == bufferSize) 
                {
			// 若缓冲区满，则执行输出操作
			outputIndex = 0;
		}
		currentIndex--;
	}
	return outputIndex;
}

static void printInt(int n) 
{
   	char str[MAX_SIZE];
	int count = 0, i = 0;
	count = convertDecimalToString(n, str, MAX_SIZE, count);
	while (str[i] != '\0') putChar(str[i++]);
	putChar('\n');
}

void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));

	switch(tf->irq) {
		// TODO: 填好中断处理程序的调用
		case -1: 
                        break;
		case 0xd: 
                        GProtectFaultHandle(tf); 
                        break;
		case 0x80: 
                        syscallHandle(tf); 
                        break;
		//case 0x20: break;
		case 0x21: 
                        KeyboardHandle(tf); 
                        break; 
		default:
                        assert(0);
	}
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	uint32_t code = getKeyCode();

	if(code == 0xe){ // 退格符
		//要求只能退格用户键盘输入的字符串，且最多退到当行行首
		if(displayCol>0&&displayCol>tail){
			displayCol--;
			uint16_t data = 0 | (0x0c << 8);
			int pos = (80*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
		}
	}else if(code == 0x1c){ // 回车符
		//处理回车情况
		keyBuffer[bufferTail++]='\n';
		displayRow++;
		displayCol=0;
		tail=0;
		if(displayRow==25){
			scrollScreen();
			displayRow=24;
			displayCol=0;
		}
	}else if(code < 0x81){ 
		// TODO: 处理正常的字符
                char cd = getChar(code); // 获取键盘扫描码对应的字符
                // 检查获取到的字符是否可显示（ASCII 码大于等于 0x20）
		if (cd >= 0x20) 
                {
			putChar(cd); // 在屏幕上打印字符
			keyBuffer[bufferTail++] = cd; // 将字符存储到键盘缓冲区中，并更新缓冲区尾部位置

                        // 定义用户数据段的选择子
			int sel = USEL(SEG_UDATA); 
			// 将获取到的字符存储到变量中
                        char character = cd; 
                        // 定义将要写入显示缓冲区的数据和字符在缓冲区中的位置
			uint16_t data = 0;
			int pos = 0;
			// 使用内联汇编指令将用户数据段选择子加载到 ES 寄存器中
                        asm volatile("movw %0, %%es"::"m"(sel));
			
			data = character | (0x0c << 8); // 合并字符的 ASCII 码和颜色属性（前景色为亮红色）
			pos = (80 * displayRow + displayCol) * 2; // 计算字符在显示缓冲区中的偏移位置
			asm volatile("movw %0, (%1)"::"r"(data), "r"(pos + 0xb8000)); // 使用内联汇编指令将数据写入显示缓冲区中对应位置
				
			displayCol++;
			if (displayCol >= 80) // 显示列超过屏幕宽度
                        {
				displayCol = 0;
				displayRow++;
			}
			while (displayRow >= 25) // 显示行超过屏幕高度
                        {
				scrollScreen();
				displayRow--;
				displayCol = 0;
			}
		}
	}
	updateCursor(displayRow, displayCol);
}

void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallRead(tf);
			break; // for SYS_READ
		default:break;
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	int sel =  USEL(SEG_UDATA);
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		// TODO: 完成光标的维护和打印到显存
                if (character == '\n') 
                {
			displayCol = 0;
			displayRow++;
		}
		else 
                {
			data = character | (0x0c << 8);
			pos = (80 * displayRow + displayCol) * 2; // 一个字符占用两个字节，乘以 2
			asm volatile("movw %0, (%1)"::"r"(data), "r"(pos + 0xb8000));
			displayCol++;
		}

		if (displayCol == 80) 
                {
			displayCol = 0;
			displayRow++;
		}
		if (displayRow > 24) 
                {
			scrollScreen();
			displayRow = 24;
		} 
        }
	tail=displayCol;
	updateCursor(displayRow, displayCol);
}

void syscallRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			syscallGetChar(tf);
			break; // for STD_IN
		case 1:
			syscallGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void syscallGetChar(struct TrapFrame *tf){
	// TODO: 自由实现
        int hasNewline = 0;

        // Check if the last character is a newline
        if (bufferTail > bufferHead && keyBuffer[bufferTail - 1] == '\n') 
        {
                hasNewline = 1;
                keyBuffer[--bufferTail] = '\0'; // Remove the newline character
        }

        // If there are characters left in the buffer, return the next character
        if (bufferTail > bufferHead && hasNewline) 
        {
                tf->eax = keyBuffer[bufferHead++];
        } 
        else 
        {
                tf->eax = 0; // No characters left
        }
}

void syscallGetStr(struct TrapFrame *tf){
	// TODO: 自由实现
        int hasNewline = 0;
        int i = 0;
        int sel = USEL(SEG_UDATA);
        asm volatile("movw %0, %%es"::"m"(sel));

        // Check if the last character is a newline
        if (bufferTail > bufferHead && keyBuffer[bufferTail - 1] == '\n') 
        {
                hasNewline = 1;
                keyBuffer[--bufferTail] = '\0'; // Remove the newline character
        }
#define min(a,b) (a < b ? a : b)
        // If there are characters left in the buffer, copy them to the destination
        if (hasNewline || bufferTail - bufferHead >= tf->ebx) 
        {
                printInt(bufferTail - bufferHead);
                for (i = 0; i < min(tf->ebx, bufferTail - bufferHead); i++) 
                {
                        asm volatile("movb %1, %%es:(%0)"::"r"(tf->edx + i), "r"(keyBuffer[bufferHead + i]));
                }
                tf->eax = 1; // Successfully copied the string
        } 
        else 
        {
                tf->eax = 0; // Not enough characters in the buffer
        }
}
