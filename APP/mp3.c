/*****************************************************
copyright (C), 2014-2015, Lighting Studio. Co.,     Ltd. 
File name：mp3.c mp3主应用程序
Author：Jerey_Jobs    Version:5.0    Date:2016/10/2  
Description：
Funcion List: 
*****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

struct song
{
	char songname[20];
	struct song * prev;
	struct song * next;
};

//孙子进程id号
pid_t gradchild;

//子进程ID号
pid_t pid;

//共享内存描述标记
int shmid;

char * p_addr = NULL;

//播放标记
int first_key = 1;
int play_flag = 0;

//播放函数
void play(struct song * currentsong)
{
	pid_t fd;
	char * c_addr;
	char * p;
	int len;
	char my_song[30]="/mp3/song";  //存放歌的路径被写死 //TODO
	while(currentsong)
	{
		//创建子进程，即孙子进程
		fd = fork();
		if(-1 == fd){
			perror("fork");
			exit(1);
		}
		else if(0 == fd){
			//歌曲名加上绝对路径
			strcat(my_song,currentsong->songname);
			p = my_song;
			len = strlen(p);
		
			//需要去掉文件名后的'\n'
			my_song[len-1] = '\0';

			printf("歌曲名为：%s\n",my_song);
			execl("/mp3/madplay","madplay",my_song,NULL);
			printf("\n\n\n");
		}
		else{
			//内存映射
			c_addr = shmat(shmid,0,0);

			//把孙子进程的id和当前播放的歌曲的节点指针传入共享内存
			memcpy(c_addr,&fd,sizeof(pid_t));
			memcpy(c_addr+sizeof(pid_t)+1,&currentsong,4);
			//使用wait阻塞孙子进程，直到孙子进程播放完才能被唤醒
			//当被唤醒时，表示播放MP3期间没有按键按下，则继续顺序播放下一首MP3
			if(fd == wait(NULL)){
				currentsong = currentsong->next;
				printf("下一首歌是：%s\n",currentsong->songname);
			}
		}
	}
}


//创建歌曲列表（双向循环链表）
struct song * create_song_list(void)
{
	FILE * fd;
	size_t size;
	size_t len;
	char * line = NULL;
	struct song * head;
	struct song * p1;
	struct song * p2;

	system("ls /mp3/song >song_list");
	fd = fopen("song_list","r");

	p1 = (struct song *)malloc(sizeof(struct song));

	printf("==========================歌曲列表=============================\n");
	system("ls /mp3/song");
	printf("\n");
	printf("===============================================================\n");
	size = getline(&line,&len,fd);

	strncpy(p1->songname,line,strlen(line));
	head = p1;
	while((size = getline(&line,&len,fd)) != -1)
	{
		p2 = p1;
		p1 = (struct song *)malloc(sizeof(struct song));
		strncpy(p1->songname,line,strlen(line));
		p2->next = p1;
		p1->prev = p2;
	}
	p1->next = head;
	head->prev = p1;
	p1 = NULL;	//防止野指针
	p2 = NULL;
	stytem("rm -rf song_list");

	return head;
}

//开始播放函数
void startplay(pid_t * childpid,struct song * my_song)
{
	pid_t pid;
	int ret;
	//创建子进程
	pid = fork();

	if(pid > 0){
		*childpid = pid;
		play_flag = 1;
		sleep(1);
		//把孙子进程的PID传给父进程
		memcpy(&gradchild,p_addr,sizeof(pid_t));
	}
	else if(0 == pid){
		//子进程播放MP3函数
		play(my_song);
	}
}


//停止播放函数
void my_stop(pid_t g_pid)
{
	printf("=====================停止！按下K1播放===================\n");
	kill(g_pid,SIGKILL);	//对孙子进程发送信号	
	kill(pid,SIGKILL);		//对子进程发送信号
	first_key = 1;
}

//继续播放函数
void conti_play(pid_t pid)
{
	printf("=======================继续播放========================\n");
	kill(pid,SIGCONT);		//对孙子进程发送信号
	play_flag = 1;
}

//下一首函数
void next(pid_t next_pid)
{
	struct song * nextsong;

	printf("======================下一首==========================\n");
	//从共享内存获得孙子进程播放歌曲的节点指针
	memcpy(&nextsong,p_addr+sizeof(pid_t)+1,4);
	//指向下一首歌曲的节点
	nextsong = nextsong->next;
	//杀死当前歌曲播放的子进程,孙子进程
	kill(pid,SIGKILL);
	wait(NULL);
	startplay(&pid,nextsong);
}


//前一首函数
void prev(pid_t prev_pid)
{
	struct song * prevsong;

	//从共享内存获得孙子进程播放歌曲的节点指针
	printf("=====================上一首===========================\n");
	memcpy(&prevsong,p_addr+sizeof(pid_t)+1,4);
	//指向上一首歌曲的节点
	prevsong = prevsong->next;
	//杀死当前歌曲播放的子进程,孙子进程
	kill(pid,SIGKILL);
	wait(NULL);
	startplay(&pid,prevsong);
}

int main(void)
{
	int buttons_fd;
	int key_value;
	struct song *head;

	//打开设备文件
	buttons_fd = open("/dev/buttons",0);
	if(buttons_fd < 0){
		perror("open device buttons");
		exit(1);
	}

	//创建播放列表
	head = create_song_list();

	printf("***************************************************\n");
	printf("**                                               **\n");
	printf("**    K1:开始/暂停 K2:停止 K3:下一首 K4:上一首   **\n");
	printf("**                                               **\n");
	printf("***************************************************\n");

	//共享内存：存放子进程ID，播放列表设置
	if((shmid = shmget(IPC_PRIVATE,5,S_IRUSR|S_IWUSR)) == -1){	//创建新的共享内存
		exit(1);
	}
	p_addr = shmat(shmid,0,0);		//把共享内存区域映射到调用进程的地址空间，返回值为成功映射的地址
	memset(p_addr,'\0',1024);

	while(1)
	{
		fd_set rds;
		int ret;

		FD_ZERO(&rds);				//清空集合rds的全部位
		FD_SET(buttons_fd,&rds);	//设置rds中buttons_fd位,既将监听值设在rds集合内

		//监听获取键值
		ret = select(buttons_fd + 1,&rds,NULL,NULL,NULL);
		if(ret < 0){
			perror("select");
			exit(1);
		}
		if(0 == ret){				//超时
			printf("TIMEOUT.\n");
		}
		else if(FD_ISSET(buttons_fd,&rds)){		//测试buttons_fd位是否改变,改变为真
			int ret = read(buttons_fd,&key_value,sizeof(key_value));
			if(ret != sizeof(key_value)){
				if(errno != EAGAIN){		//EAGAIN：无数据可读
					perror("read buttons\n");
				}
				continue;
			}
			else
			{
				//首次播放，必须K1按下
				if(first_key){
					switch(key_value)
					{
					case 0:
						startplay(&pid,head);
						first_key = 0;
						break;
					case 1:
					case 2:
					case 3:
						printf("=========PLEASE PRESS K1 TO START PLAY=============\n");
						break;
					default:
						printf("=========PLEASE PRESS K1 TO START PLAY=============\n");
						break;
					}
				}
				//不是首次播放，需要根据不同的键值进行处理
				else if(!first_key){
					switch(key_value)
					{
					case 0:
						if(play_flag){
							my_pause(gradchild);
						}
						else{
							conti_play(gradchild);
						}
						break;
					case 1:
						my_stop(gradchild);
						break;
					case 2:
						next(gradchild);
						break;
					case 3:
						prev(gradchild);
						break;
					}
				}
			}
		}
	}

	close(buttons_fd);
    return 0;
}
