#ifndef _SERVERMESSAGETASK_H
#define _SERVERMESSAGETASK_H

#include "define.h"
#include "ace/Task.h"
#include "ace/Synch.h"
#include "ace/Malloc_T.h"
#include "ace/Singleton.h"
#include "ace/Thread_Mutex.h"
#include "ace/Date_Time.h"

#include "IClientManager.h"
#include "MessageBlockManager.h"

#include <map>
using namespace std;

//�����������������ݰ����̴���
//������������̴߳�������ˣ��᳢����������
//add by freeeyes

#define MAX_SERVER_MESSAGE_QUEUE 1000    //���������г���
#define MAX_DISPOSE_TIMEOUT      30      //�������ȴ�����ʱ��  

//��������ͨѶ�����ݽṹ�����հ���
struct _Server_Message_Info
{
	IClientMessage*    m_pClientMessage;
	uint16             m_u2CommandID;
	ACE_Message_Block* m_pRecvFinish;
	_ClientIPInfo      m_objServerIPInfo;

	ACE_Message_Block* m_pmbQueuePtr;        //��Ϣ����ָ���

	_Server_Message_Info()
	{
		m_u2CommandID    = 0;
		m_pClientMessage = NULL;
		m_pRecvFinish    = NULL;

		//����������Ϣ����ģ��ָ�����ݣ������Ͳ��ط�����new��delete����������
		//ָ���ϵҲ����������ֱ��ָ��������ʹ�õ�ʹ����ָ��
		m_pmbQueuePtr  = new ACE_Message_Block(sizeof(_Server_Message_Info*));

		_Server_Message_Info** ppMessage = (_Server_Message_Info**)m_pmbQueuePtr->base();
		*ppMessage = this;
	}

	~_Server_Message_Info()
	{
		if(NULL != m_pmbQueuePtr)
		{
			m_pmbQueuePtr->release();
			m_pmbQueuePtr = NULL;
		}
	}
	
	ACE_Message_Block* GetQueueMessage()
	{
		return m_pmbQueuePtr;
	}

};

#define MAX_SERVER_MESSAGE_INFO_COUNT 100

//_Server_Message_Info�����
class CServerMessageInfoPool
{
public:
	CServerMessageInfoPool();
	~CServerMessageInfoPool();

	void Init(uint32 u4PacketCount = MAX_SERVER_MESSAGE_INFO_COUNT);
	void Close();

	_Server_Message_Info* Create();
	bool Delete(_Server_Message_Info* pMakePacket);

	int GetUsedCount();
	int GetFreeCount();

private:
	typedef map<_Server_Message_Info*, _Server_Message_Info*> mapMessage;
	mapMessage                  m_mapMessageUsed;                      //��ʹ�õ�
	mapMessage                  m_mapMessageFree;                      //û��ʹ�õ�
	ACE_Recursive_Thread_Mutex  m_ThreadWriteLock;                     //���ƶ��߳���
}; 

//�����������ݰ���Ϣ���д������
class CServerMessageTask : public ACE_Task<ACE_MT_SYNCH>
{
public:
	CServerMessageTask();
	~CServerMessageTask();

	virtual int open(void* args = 0);
	virtual int svc (void);

	virtual int handle_signal (int signum,
		siginfo_t *  = 0,
		ucontext_t * = 0);

	bool Start();
	int  Close();
	bool IsRun();

	uint32 GetThreadID();

	bool PutMessage(_Server_Message_Info* pMessage);

	bool CheckServerMessageThread(ACE_Time_Value tvNow);

	void AddClientMessage(IClientMessage* pClientMessage);

	void DelClientMessage(IClientMessage* pClientMessage);

private:
	bool CheckValidClientMessage(IClientMessage* pClientMessage);
	bool ProcessMessage(_Server_Message_Info* pMessage, uint32 u4ThreadID);

private:
	uint32               m_u4ThreadID;  //��ǰ�߳�ID
	bool                 m_blRun;       //��ǰ�߳��Ƿ�����
	uint32               m_u4MaxQueue;  //�ڶ����е�����������
	EM_Server_Recv_State m_emState;     //����״̬
	ACE_Time_Value       m_tvDispose;   //�������ݰ�����ʱ��

	//��¼��ǰ��Ч��IClientMessage����Ϊ���첽�Ĺ�ϵ��
	//������뱣֤�ص���ʱ��IClientMessage�ǺϷ��ġ�
	typedef vector<IClientMessage*> vecValidIClientMessage;
	vecValidIClientMessage m_vecValidIClientMessage;
};

class CServerMessageManager
{
public:
	CServerMessageManager();
	~CServerMessageManager();

	void Init();

	bool Start();
	int  Close();
	bool PutMessage(_Server_Message_Info* pMessage);
	bool CheckServerMessageThread(ACE_Time_Value tvNow);

	void AddClientMessage(IClientMessage* pClientMessage);
	void DelClientMessage(IClientMessage* pClientMessage);

private:
	CServerMessageTask*         m_pServerMessageTask;
	ACE_Recursive_Thread_Mutex  m_ThreadWritrLock; 
};


typedef ACE_Singleton<CServerMessageManager, ACE_Recursive_Thread_Mutex> App_ServerMessageTask;
typedef ACE_Singleton<CServerMessageInfoPool, ACE_Recursive_Thread_Mutex> App_ServerMessageInfoPool;
#endif
