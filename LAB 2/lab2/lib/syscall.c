#include "lib.h"
#include "types.h"

typedef char *  va_list;
#define _INTSIZEOF(n)   ( (sizeof(n) + sizeof(int) - 1) & ~(sizeof(int) - 1) )
#define va_start(ap, format) ( ap = (va_list)&format+ _INTSIZEOF(format) )
#define va_arg(ap, type) ( *(type*)((ap += _INTSIZEOF(type)) -_INTSIZEOF(type)) )
#define va_end(ap)  ( ap = (va_list)0 )

/*
 * io lib here
 * 库函数写在这
 */


//static inline int32_t syscall(int num, uint32_t a1,uint32_t a2,
int32_t syscall(int num, uint32_t a1,uint32_t a2,
		uint32_t a3, uint32_t a4, uint32_t a5)
{
	int32_t ret = 0;
	//Generic system call: pass system call number in AX
	//up to five parameters in DX,CX,BX,DI,SI
	//Interrupt kernel with T_SYSCALL
	//
	//The "volatile" tells the assembler not to optimize
	//this instruction away just because we don't use the
	//return value
	//
	//The last clause tells the assembler that this can potentially
	//change the condition and arbitrary memory locations.

	/*
		 Note: ebp shouldn't be flushed
	    May not be necessary to store the value of eax, ebx, ecx, edx, esi, edi
	*/
	uint32_t eax, ecx, edx, ebx, esi, edi;
	uint16_t selector;
	
	asm volatile("movl %%eax, %0":"=m"(eax));
	asm volatile("movl %%ecx, %0":"=m"(ecx));
	asm volatile("movl %%edx, %0":"=m"(edx));
	asm volatile("movl %%ebx, %0":"=m"(ebx));
	asm volatile("movl %%esi,  %0":"=m"(esi));
	asm volatile("movl %%edi,  %0":"=m"(edi));
	asm volatile("movl %0, %%eax": :"m"(num));
	asm volatile("movl %0, %%ecx"::"m"(a1));
	asm volatile("movl %0, %%edx"::"m"(a2));
	asm volatile("movl %0, %%ebx"::"m"(a3));
	asm volatile("movl %0, %%esi" ::"m"(a4));
	asm volatile("movl %0, %%edi" ::"m"(a5));
	asm volatile("int $0x80");
	asm volatile("movl %%eax, %0":"=m"(ret));
	asm volatile("movl %0, %%eax"::"m"(eax));
	asm volatile("movl %0, %%ecx"::"m"(ecx));
	asm volatile("movl %0, %%edx"::"m"(edx));
	asm volatile("movl %0, %%ebx"::"m"(ebx));
	asm volatile("movl %0, %%esi"::"m"(esi));
	asm volatile("movl %0, %%edi"::"m"(edi));
	
	asm volatile("movw %%ss, %0":"=m"(selector)); //%ds is reset after iret
	//selector = 16;
	asm volatile("movw %%ax, %%ds"::"a"(selector));
	
	return ret;
}

char getChar(){ // 对应SYS_READ STD_IN
	// TODO: 实现getChar函数，方式不限
        char ret = 0; // 初始化返回值为0，表示暂时没有读取到字符
        // 循环直到成功读取一个字符
	while (ret == 0)
	{
     	        ret = (char)syscall(SYS_READ, STD_IN, 0, 0, 0, 0); // 调用系统调用读取标准输入的字符，返回值存储在 ret 中
        }
	return ret; // 返回读取到的字符
}

void getStr(char *str, int size){ // 对应SYS_READ STD_STR
	// TODO: 实现getStr函数，方式不限
        int ret = 0;
	while (ret == 0)
        {
		ret = syscall(SYS_READ, STD_STR, (uint32_t)str, size, 0, 0); // 调用系统调用读取字符串，将结果存储在 str 中，最多读取 size 个字符
        }
	return;
}

int dec2Str(int decimal, char *buffer, int size, int count);
int hex2Str(uint32_t hexadecimal, char *buffer, int size, int count);
int str2Str(char *string, char *buffer, int size, int count);

void printf(const char *format,...){
	int i=0; // format index
	char buffer[MAX_BUFFER_SIZE];
	int count=0; // buffer index
	//int index=0; // parameter index
	void *paraList=(void*)&format; // address of format in stack
	int state=0; // 0: legal character; 1: '%'; 2: illegal format
	int decimal=0;
	uint32_t hexadecimal=0;
	char *string=0;
	char character=0;
	while(format[i]!=0){
		// TODO: support format %d %x %s %c
                // 根据解析状态初始化为0或1
                if(state != 2)
                        state=0;
		else 
                        break;

		buffer[count] = format[i]; // 将当前字符添加到输出缓冲区
		count++; // 缓冲区索引增加

		if(format[i] == '%')
                {
			count--; // 回退一个字符
			i++; // 指向下一个字符
			state = 1; // 进入解析状态

			paraList += sizeof(format); // 将参数列表指针指向下一个参数

                        switch (format[i]) 
                        {
                        case 'c': // 字符
                                character = *(char *)(paraList);
                                buffer[count++] = character; // 将字符添加到输出缓冲区
                                break;
                        case 'd': // 十进制整数
                                decimal = *(int *)(paraList);
                                count = dec2Str(decimal, buffer, (uint32_t)MAX_BUFFER_SIZE, count);
                                break;
                        case 'x': // 十六进制整数
                                hexadecimal = *(uint32_t *)(paraList);
                                count = hex2Str(hexadecimal, buffer, (uint32_t)MAX_BUFFER_SIZE, count);
                                break;
                        case 's': // 字符串
                                string = *(char **)(paraList);
                                count = str2Str(string, buffer, (uint32_t)MAX_BUFFER_SIZE, count);
                                break;
                        case '%': // 百分号
                                state = 0; // 重置解析状态
                                count++; // 继续添加百分号到输出缓冲区
                                break;
                        default: // 非法格式
                                state = 2; // 将解析状态设为2
                                break;
                        }
	        }
		if(count == MAX_BUFFER_SIZE) // 输出缓冲区已满
                {
		        syscall(SYS_WRITE, STD_OUT, (uint32_t)buffer, (uint32_t)count, 0, 0);
			count=0; // 重置缓冲区索引
		}
		i++; // 继续解析format字符串的下一个字符

        }
	if(count!=0)
		syscall(SYS_WRITE, STD_OUT, (uint32_t)buffer, (uint32_t)count, 0, 0);
}

int dec2Str(int decimal, char *buffer, int size, int count) {
	int i=0;
	int temp;
	int number[16];

	if(decimal<0){
		buffer[count]='-';
		count++;
		if(count==size) {
			syscall(SYS_WRITE, STD_OUT, (uint32_t)buffer, (uint32_t)size, 0, 0);
			count=0;
		}
		temp=decimal/10;
		number[i]=temp*10-decimal;
		decimal=temp;
		i++;
		while(decimal!=0){
			temp=decimal/10;
			number[i]=temp*10-decimal;
			decimal=temp;
			i++;
		}
	}
	else{
		temp=decimal/10;
		number[i]=decimal-temp*10;
		decimal=temp;
		i++;
		while(decimal!=0){
			temp=decimal/10;
			number[i]=decimal-temp*10;
			decimal=temp;
			i++;
		}
	}

	while(i!=0){
		buffer[count]=number[i-1]+'0';
		count++;
		if(count==size) {
			syscall(SYS_WRITE, STD_OUT, (uint32_t)buffer, (uint32_t)size, 0, 0);
			count=0;
		}
		i--;
	}
	return count;
}

int hex2Str(uint32_t hexadecimal, char *buffer, int size, int count) {
	int i=0;
	uint32_t temp=0;
	int number[16];

	temp=hexadecimal>>4;
	number[i]=hexadecimal-(temp<<4);
	hexadecimal=temp;
	i++;
	while(hexadecimal!=0){
		temp=hexadecimal>>4;
		number[i]=hexadecimal-(temp<<4);
		hexadecimal=temp;
		i++;
	}

	while(i!=0){
		if(number[i-1]<10)
			buffer[count]=number[i-1]+'0';
		else
			buffer[count]=number[i-1]-10+'a';
		count++;
		if(count==size) {
			syscall(SYS_WRITE, STD_OUT, (uint32_t)buffer, (uint32_t)size, 0, 0);
			count=0;
		}
		i--;
	}
	return count;
}

int str2Str(char *string, char *buffer, int size, int count) {
	int i=0;
	while(string[i]!=0){
		buffer[count]=string[i];
		count++;
		if(count==size) {
			syscall(SYS_WRITE, STD_OUT, (uint32_t)buffer, (uint32_t)size, 0, 0);
			count=0;
		}
		i++;
	}
	return count;
}

