#include "types.h"
#include "utils.h"
#include "lib.h"

union DirEntry {
	uint8_t byte[128];
	struct {
		int32_t inode;
		char name[64];
	};
};

typedef union DirEntry DirEntry;

int ls(char *destFilePath) {
	printf("ls %s\n", destFilePath);
	int i = 0;
	int fd = 0;
	int ret = 0;
	DirEntry *dirEntry = 0;
	uint8_t buffer[512 * 2];
	fd = open(destFilePath, O_READ | O_DIRECTORY);
	if (fd == -1)
		return -1;
	ret = read(fd, buffer, 512 * 2);
	while (ret != 0) {
		// TODO: Complete 'ls'
		// 将读取的缓冲区转换为 DirEntry 结构体数组
		dirEntry = (DirEntry *)buffer;
		// 遍历当前缓冲区中的所有 DirEntry
        	for (i = 0; i < ret / sizeof(DirEntry); i++) 
		{
			// 确定该条目是否有效
			if (dirEntry[i].inode != 0) 
			{
	                	printf("%s ", dirEntry[i].name); // 输出有效的目录条目名称
	            	}
	        }
		// 继续读取下一段数据到缓冲区，直到读取到末尾
	        ret = read(fd, buffer, 512 * 2);
	}
	printf("\n");
	close(fd);
	return 0;
}

int cat(char *destFilePath) {
	printf("cat %s\n", destFilePath);
	int fd = 0;
	int ret = 0;
	uint8_t buffer[512 * 2];
	fd = open(destFilePath, O_READ);
	if (fd == -1)
		return -1;
	ret = read(fd, buffer, 512 * 2);
	while (ret != 0) {
		// TODO: Complete 'cat'
		for(int i = 0; i < ret; i++)
		{
			printf("%c", buffer[i]); // 输出每个字符到标准输出
		}
        	ret = read(fd, buffer, 512 * 2); // 读取下一段文件内容
	}
	close(fd);
	return 0;
}

void match(const char* path, const char* name){
    int pp = 0;
    int pa = 0;
    while(path[pp] != 0){
        pa = 0;
        int np = pp;
        while(name[pa] != 0){
            if (name[pa] == path[np]){
                pa++;
                np++;
            }
            else
                break;
        }
        if(name[pa] == 0){
            printf("%s\n", path);
            return;
        }
        pp++;
    }
}

int find(char *dir, char *file)
{
	int i = 0;
	int fd = 0;
	int ret = 0;
	DirEntry *dirEntry = 0;
	uint8_t buffer[512 * 2];
	struct stat st;
	char name_buffer[512];
	int len;

	len = stringLen(dir);
	stringCpy(dir, name_buffer, len);
	name_buffer[len++] = '/';
	name_buffer[len] = '\0';
	
	ret = stat(dir, &st);
	if (ret < 0) {
		printf("cannot stat file: %s\n", dir);
		return -1;
	}

	switch (st.type) {
		case TYPE_FILE:
			match(dir, file);
			break;
		case TYPE_DIRECTORY:
			// TODO: if type is directory
			fd = open(dir, O_READ | O_DIRECTORY); // 打开目录
			if (fd == -1) 
			{
				printf("cannot open directory: %s\n", dir);
				return -1;
			}

			while ((ret = read(fd, buffer, sizeof(buffer))) > 0) 
			{
				i = 0;
				while (i < ret) 
				{
					dirEntry = (DirEntry *)(buffer + i); // 获取目录条目
					if (dirEntry->inode != 0) 
					{
						stringCpy(dir, name_buffer, len - 1); // 拷贝目录路径
						stringCpy(dirEntry->name, name_buffer + len, stringLen(dirEntry->name)); // 拷贝文件名
						name_buffer[len + stringLen(dirEntry->name)] = '\0'; // 添加结束符
						
						// Recursively search in subdirectories
						find(name_buffer, file); // 递归查找文件
					}
					i += sizeof(DirEntry);
				}
			}
			close(fd); // 关闭目录
			break;
		default:
			break;
	}

	return 0;
}


int uEntry(void) {
	int fd = 0;
	int i = 0;
	char tmp = 0;
	
	ls("/");
	ls("/dev/");

	printf("create /usr/test and write alphabets to it\n");
	fd = open("/usr/test", O_WRITE | O_READ | O_CREATE);
	for (i = 0; i < 26; i ++) {
		tmp = (char)(i % 26 + 'A');
		write(fd, (uint8_t*)&tmp, 1);
	}
	close(fd);
	
	ls("/usr/");
	cat("/usr/test");
	printf("\n");
	printf("rm /usr/test\n");
	remove("/usr/test");
	ls("/usr/");
	printf("rmdir /usr/\n");
	remove("/usr/");
	ls("/");
	printf("create /usr/\n");
	fd = open("/usr/", O_CREATE | O_DIRECTORY);
	close(fd);
	ls("/");

	printf("\n");
	printf("find test.txt in /data\n");
	find("/data", "test.txt");
	
	exit();
	return 0;
}
