/*************************************************************************
    > File Name: try.c
    > Author: NiGo
    > Mail: nigo@xiyoulinux.org
    > Created Time: 2020年07月22日 星期三 22时14分14秒
 ************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "md5.c"
 
int main(int argc, char *argv[])
{
	int i;
	unsigned char encrypt[] ="admin";//21232f297a57a5a743894a0e4a801fc3
	unsigned char decrypt[16];    
	MD5_CTX md5;
	MD5Init(&md5);         		
	MD5Update(&md5,encrypt,strlen((char *)encrypt));
	MD5Final(&md5,decrypt);        
	printf("加密前:%s\n加密后:",encrypt);
	for(i=0;i<16;i++)
	{
		printf("%02x",decrypt[i]);
	}
	return 0;
}
