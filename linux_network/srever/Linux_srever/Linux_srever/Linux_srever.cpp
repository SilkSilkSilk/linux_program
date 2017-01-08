/*
�����ź����͹����ڴ�Ļ���д��һ�������ߺ������ߵĳ��򣺷����
�ڹ����ڴ�Ŀռ��н���һ��ͷ�������ݰ���ͷ��ָ�����ݰ��ĸ����ͳ���
*/
#include "unp.h"
#include <sys/shm.h>																								//sysytem v�����ڴ���ɺ���
#include <sys/sem.h>																								//sysytem v�ź�����ɺ���

union semun {
	int              val;																							// ���pv����ֵ
	struct semid_ds *buf;																							// �ں�ά�����ź�����Ϣ
	unsigned short  *array;																							// Array for GETALL, SETALL
	struct seminfo  *__buf;																							// Buffer for IPC_INFO
};

typedef struct
{
	u_int16_t blksize;
	u_int16_t blocks;
	u_int16_t rd_index;
	u_int16_t wr_index;
}shmhead_t;

typedef struct
{
	shmhead_t *p_head;																								//ͷ����ַ
	char *p_Start;

	int shmid;
	int semid;
	int sem_mutex;
	int sem_full;
	int sem_empty;
}shmfifo_t;

typedef struct
{
	char name[26];
	int age;
}STU;

#define DELAY (rand() % 5 + 1)

const key_t Memorykey = 1;
const key_t Semaphorekey = 1;
const char *Filename = "Linux.cpp";

int semctl_getval(int semid, int n);
void semctl_setval(int semid, int n, int val);
void semop_p(int semid, u_int16_t no, int16_t sem_op = -1);
void semop_v(int semid, u_int16_t no, int16_t sem_op = 1);
shmfifo_t * shmfifo_init(int blksize, int blocks);
void shmfifo_put(shmfifo_t *fifo, const char *buf);
void shmfifo_get(shmfifo_t *fifo, char *buf);
void shmfifo_destroy(shmfifo_t *fifo);

int main(int argc, char *argv[], char *envp[])
{
	int num;
	STU buf;
	int i;
	char ch[2] = "a";
	shmfifo_t * fifo;

	if (argc != 2)
	{
		err_sys("argc less");
	}
	num = atol(argv[1]);

	fifo = shmfifo_init(sizeof(STU), 10);

	memset(&buf, 0, sizeof(STU));
	for (i = 0; i < num; i++)
	{
		buf.age = 20 + i;
		strcpy(buf.name, ch);
		shmfifo_put(fifo, reinterpret_cast<char*>(&buf));
		ch[0]++;
	}
	
	exit(0);
}

shmfifo_t * shmfifo_init(int blksize, int blocks)
{
	int shm_size;
	shmfifo_t * fifo = static_cast<shmfifo_t*>(malloc(sizeof(shmfifo_t)));
	if (fifo == nullptr)
		err_sys("fifo error");

	memset(fifo, 0, sizeof(shmfifo_t));
	shm_size = sizeof(shmhead_t) + blksize*blocks;

	fifo->sem_mutex = 0;																								//һ���źż����ź�������
	fifo->sem_full = 1;
	fifo->sem_empty = 2;

	if ((fifo->shmid = shmget(Memorykey, shm_size, 0)) < 0)
	{
		if ((fifo->shmid = shmget(Memorykey, shm_size, IPC_CREAT | 0666)) < 0)											//�򿪹����ڴ�
			err_sys("shmget error!\n");

		if ((fifo->p_head = static_cast<shmhead_t*>(shmat(fifo->shmid, nullptr, 0))) == nullptr)						//ӳ�乲���ڴ浽������
			err_sys("shmat error!\n");

		fifo->p_head->blocks = blocks;
		fifo->p_head->blksize = blksize;
		fifo->p_head->rd_index = 0;
		fifo->p_head->wr_index = 0;

		fifo->p_Start = reinterpret_cast<char*>(fifo->p_head + 1);

		if ((fifo->semid = semget(Semaphorekey, 3, IPC_CREAT | IPC_EXCL | 0666)) < 0)									//�����źż� IPC_EXCL����źż��Ѵ��ڷ��ش���
			err_sys("semget error!\n");

		semctl_setval(fifo->semid, fifo->sem_mutex, 1);																	//����
		semctl_setval(fifo->semid, fifo->sem_full, blocks);																//�ֿ����ռ�
		semctl_setval(fifo->semid, fifo->sem_empty, 0);																	//���û���

		//printf("Semaphore: 0=%d   1=%d   2=%d\n", semctl_getval(fifo->semid, fifo->sem_mutex), semctl_getval(fifo->semid, fifo->sem_full), semctl_getval(fifo->semid, fifo->sem_empty));
	}
	else
	{
		if ((fifo->p_head = static_cast<shmhead_t*>(shmat(fifo->shmid, nullptr, 0))) == nullptr)						//ӳ�乲���ڴ浽������
			err_sys("shmat error!\n");

		fifo->p_Start = reinterpret_cast<char*>(fifo->p_head + 1);

		if ((fifo->semid = semget(Semaphorekey, 3, 0)) < 0)																//���źż� IPC_EXCL����źż��Ѵ��ڷ��ش���
			err_sys("semget error!\n");

		printf("Semaphore: 0=%d   1=%d   2=%d\n", semctl_getval(fifo->semid, fifo->sem_mutex), semctl_getval(fifo->semid, fifo->sem_full), semctl_getval(fifo->semid, fifo->sem_empty));
	}
	return fifo;
}

void shmfifo_put(shmfifo_t *fifo, const char *buf)
{
	semop_p(fifo->semid, fifo->sem_full);
	semop_p(fifo->semid, fifo->sem_mutex);

	memcpy(fifo->p_Start + fifo->p_head->blksize*fifo->p_head->wr_index, buf, fifo->p_head->blksize);
	fifo->p_head->wr_index = (fifo->p_head->wr_index + 1) % fifo->p_head->blocks;

	semop_v(fifo->semid, fifo->sem_mutex);
	semop_v(fifo->semid, fifo->sem_empty);
}

void shmfifo_get(shmfifo_t *fifo, char *buf)
{
	semop_p(fifo->semid, fifo->sem_empty);
	semop_p(fifo->semid, fifo->sem_mutex);

	memcpy(buf, fifo->p_Start + fifo->p_head->blksize*fifo->p_head->rd_index, fifo->p_head->blksize);
	fifo->p_head->rd_index = (fifo->p_head->rd_index + 1) % fifo->p_head->blocks;

	semop_v(fifo->semid, fifo->sem_mutex);
	semop_v(fifo->semid, fifo->sem_full);
}

void shmfifo_destroy(shmfifo_t *fifo)
{
	if (semctl(fifo->semid, 0, IPC_RMID) < 0)																		//ɾ���źż�
		err_sys("semctl_rm error");

	shmdt(fifo->p_head);																							//���ӳ��
	if (shmctl(fifo->shmid, IPC_RMID, nullptr) < 0)																	//ɾ�������ڴ�
		err_sys("shmctl error");
}

int semctl_getval(int semid, int n)
{
	int val;
	if ((val = semctl(semid, n, GETVAL)) < 0)
		err_sys("semctl_getval error");
	return val;
}

void semctl_setval(int semid, int n, int val)
{
	semun setval;
	memset(&setval, 0, sizeof(setval));
	setval.val = val;
	if (semctl(semid, n, SETVAL, setval) < 0)
		err_sys("semctl_setval error");
}

void semop_p(int semid, u_int16_t no, int16_t sem_op)																//���sem_op<0��p���� ���sem_op>0��v����
{
	struct sembuf buf = { no, sem_op, 0 };

	if (semop(semid, &buf, 1) < 0)
		err_sys("semop error");
}

void semop_v(int semid, u_int16_t no, int16_t sem_op)																//���sem_op<0��p���� ���sem_op>0��v����
{
	struct sembuf buf = { no, sem_op, 0 };

	if (semop(semid, &buf, 1) < 0)
		err_sys("semop error");
}