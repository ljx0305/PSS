#include "ConnectHandler.h"

CConnectHandler::CConnectHandler(void)
{
	m_szError[0]          = '\0';
	m_u4ConnectID         = 0;
	m_u2SendCount         = 0;
	m_u4AllRecvCount      = 0;
	m_u4AllSendCount      = 0;
	m_u4AllRecvSize       = 0;
	m_u4AllSendSize       = 0;
	m_nIOCount            = 1;
	m_u4SendThresHold     = MAX_MSG_SNEDTHRESHOLD;
	m_u2SendQueueMax      = MAX_MSG_SENDPACKET;
	m_u1ConnectState      = CONNECT_INIT;
	m_u1SendBuffState     = CONNECT_SENDNON;
	m_pCurrMessage        = NULL;
	m_pBlockMessage       = NULL;
	m_pPacketParse        = NULL;
	m_u4CurrSize          = 0;
	m_u4HandlerID         = 0;
	m_u2MaxConnectTime    = 0;
	m_u1IsActive          = 0;
	m_u4MaxPacketSize     = MAX_MSG_PACKETLENGTH;
	m_blBlockState        = false;
	m_nBlockCount         = 0;
	m_nBlockSize          = MAX_BLOCK_SIZE;
	m_nBlockMaxCount      = MAX_BLOCK_COUNT;
	m_u8RecvQueueTimeCost = 0;
	m_u4RecvQueueCount    = 0;
	m_u8SendQueueTimeCost = 0;
	m_u4ReadSendSize      = 0;
	m_u4SuccessSendSize   = 0;
	m_u8SendQueueTimeout  = MAX_QUEUE_TIMEOUT * 1000 * 1000;  //Ŀǰ��Ϊ��¼��������
	m_u8RecvQueueTimeout  = MAX_QUEUE_TIMEOUT * 1000 * 1000;  //Ŀǰ��Ϊ��¼��������
	m_u2TcpNodelay        = TCP_NODELAY_ON;
	m_emStatus            = CLIENT_CLOSE_NOTHING;
	m_u4SendMaxBuffSize   = 5*MAX_BUFF_1024;
}

CConnectHandler::~CConnectHandler(void)
{
	//OUR_DEBUG((LM_INFO, "[CConnectHandler::~CConnectHandler].\n"));
	//OUR_DEBUG((LM_INFO, "[CConnectHandler::~CConnectHandler]End.\n"));
}

const char* CConnectHandler::GetError()
{
	return m_szError;
}

bool CConnectHandler::Close(int nIOCount)
{
	m_ThreadLock.acquire();
	if(nIOCount > m_nIOCount)
	{
		m_nIOCount = 0;
	}

	if(m_nIOCount > 0)
	{
		m_nIOCount -= nIOCount;
	}

	if(m_nIOCount == 0)
	{
		m_u1IsActive = 0;
	}
	m_ThreadLock.release();

	//OUR_DEBUG((LM_ERROR, "[CConnectHandler::Close]ConnectID=%d,m_nIOCount=%d.\n", GetConnectID(), m_nIOCount));

	//�ӷ�Ӧ��ע���¼�
	if(m_nIOCount == 0)
	{
		//�鿴�Ƿ���IP׷����Ϣ�������¼
		//App_IPAccount::instance()->CloseIP((string)m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllSendSize);

		//OUR_DEBUG((LM_ERROR, "[CConnectHandler::Close]ConnectID=%d,m_nIOCount=%d.\n", GetConnectID(), m_nIOCount));

		//ɾ�����󻺳��PacketParse
		if(m_pCurrMessage != NULL)
		{
			App_MessageBlockManager::instance()->Close(m_pCurrMessage);
		}

		//�������ӶϿ���Ϣ
		App_PacketParseLoader::instance()->GetPacketParseInfo()->DisConnect(GetConnectID());

		//��֯����
		_MakePacket objMakePacket;

		objMakePacket.m_u4ConnectID       = GetConnectID();
		objMakePacket.m_pPacketParse      = NULL;

		//���Ϳͻ������ӶϿ���Ϣ��
		ACE_Time_Value tvNow = ACE_OS::gettimeofday();
		if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_CDISCONNECT, &objMakePacket, tvNow))
		{
			OUR_DEBUG((LM_ERROR, "[CConnectHandler::Close] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
		}

		//msg_queue()->deactivate();
		shutdown();
		AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, m_u8RecvQueueTimeCost = %dws, m_u4RecvQueueCount = %d, m_u8SendQueueTimeCost = %dws.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, (uint32)m_u8RecvQueueTimeCost, m_u4RecvQueueCount, (uint32)m_u8SendQueueTimeCost);

		//ɾ�����Ӷ���
		App_ConnectManager::instance()->CloseConnectByClient(GetConnectID());

		m_u4ConnectID = 0;

		//�ع��ù���ָ��
		App_ConnectHandlerPool::instance()->Delete(this);
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::Close](0x%08x)Close(ConnectID=%d) OK.\n", this, GetConnectID()));
		return true;
	}

	return false;
}

void CConnectHandler::Init(uint16 u2HandlerID)
{
	m_u4HandlerID      = u2HandlerID;
	m_u2MaxConnectTime = App_MainConfig::instance()->GetMaxConnectTime();
	m_u4SendThresHold  = App_MainConfig::instance()->GetSendTimeout();
	m_u2SendQueueMax   = App_MainConfig::instance()->GetSendQueueMax();
	m_u4MaxPacketSize  = App_MainConfig::instance()->GetRecvBuffSize();
	m_u2TcpNodelay     = App_MainConfig::instance()->GetTcpNodelay();

	m_u8SendQueueTimeout = App_MainConfig::instance()->GetSendQueueTimeout() * 1000 * 1000;
	if(m_u8SendQueueTimeout == 0)
	{
		m_u8SendQueueTimeout = MAX_QUEUE_TIMEOUT * 1000 * 1000;
	}

	m_u8RecvQueueTimeout = App_MainConfig::instance()->GetRecvQueueTimeout() * 1000 * 1000;
	if(m_u8RecvQueueTimeout <= 0)
	{
		m_u8RecvQueueTimeout = MAX_QUEUE_TIMEOUT * 1000 * 1000;
	}

	m_u4SendMaxBuffSize  = App_MainConfig::instance()->GetBlockSize();
	//m_pBlockMessage      = new ACE_Message_Block(m_u4SendMaxBuffSize);
	m_pBlockMessage      = NULL;
	m_emStatus           = CLIENT_CLOSE_NOTHING;
}

bool CConnectHandler::ServerClose(EM_Client_Close_status emStatus, uint8 u1OptionEvent)
{
	OUR_DEBUG((LM_ERROR, "[CConnectHandler::ServerClose]Close(%d) OK.\n", GetConnectID()));
	//AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, m_u8RecvQueueTimeCost = %d, m_u4RecvQueueCount = %d, m_u8SendQueueTimeCost = %d.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, m_u8RecvQueueTimeCost, m_u4RecvQueueCount, m_u8SendQueueTimeCost);

	if(CLIENT_CLOSE_IMMEDIATLY == emStatus)
	{
		//��֯����
		_MakePacket objMakePacket;

		objMakePacket.m_u4ConnectID       = GetConnectID();
		objMakePacket.m_pPacketParse      = NULL;

		//���Ϳͻ������ӶϿ���Ϣ��
		ACE_Time_Value tvNow = ACE_OS::gettimeofday();
		if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), u1OptionEvent, &objMakePacket, tvNow))
		{
			OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
		}

		//msg_queue()->deactivate();
		shutdown();

		ClearPacketParse();

		m_u4ConnectID = 0;

		//�ع��ù���ָ��
		App_ConnectHandlerPool::instance()->Delete(this);
	}
	else
	{
		m_emStatus = emStatus;
	}

	return true;
}

void CConnectHandler::SetConnectID(uint32 u4ConnectID)
{
	m_u4ConnectID = u4ConnectID;
}

uint32 CConnectHandler::GetConnectID()
{
	return m_u4ConnectID;
}

int CConnectHandler::open(void*)
{
	//OUR_DEBUG((LM_ERROR, "[CConnectHandler::open](0x%08x),m_nIOCount=%d.\n", this, m_nIOCount));
	ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadLock);
	
	m_nIOCount            = 1;
	m_blBlockState        = false;
	m_nBlockCount         = 0;
	m_u8SendQueueTimeCost = 0;
	m_blIsLog             = false;
	m_szConnectName[0]    = '\0';
	m_u1IsActive          = 1;

	//���û�����
	//m_pBlockMessage->reset();

	//���Զ�����ӵ�ַ�Ͷ˿�
	if(this->peer().get_remote_addr(m_addrRemote) == -1)
	{
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::open]this->peer().get_remote_addr error.\n"));
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectHandler::open]this->peer().get_remote_addr error.");
		return -1;
	}

	if(App_ForbiddenIP::instance()->CheckIP(m_addrRemote.get_host_addr()) == false)
	{
		//�ڽ�ֹ�б��У����������
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::open]IP Forbidden(%s).\n", m_addrRemote.get_host_addr()));
		return -1;
	}

	//��鵥λʱ�����Ӵ����Ƿ�ﵽ����
	if(false == App_IPAccount::instance()->AddIP((string)m_addrRemote.get_host_addr()))
	{
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::open]IP connect frequently.\n", m_addrRemote.get_host_addr()));
		App_ForbiddenIP::instance()->AddTempIP(m_addrRemote.get_host_addr(), App_MainConfig::instance()->GetIPAlert()->m_u4IPTimeout);

		//���͸澯�ʼ�
		AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECT, 
			App_MainConfig::instance()->GetIPAlert()->m_u4MailID,
			(char* )"Alert IP",
			"[CConnectHandler::open] IP is more than IP Max,");

		return -1;
	}

	//��ʼ�������
	m_TimeConnectInfo.Init(App_MainConfig::instance()->GetClientDataAlert()->m_u4RecvPacketCount, 
		App_MainConfig::instance()->GetClientDataAlert()->m_u4RecvDataMax, 
		App_MainConfig::instance()->GetClientDataAlert()->m_u4SendPacketCount,
		App_MainConfig::instance()->GetClientDataAlert()->m_u4SendDataMax);

	int nRet = 0;
	/*
	int nRet = ACE_Svc_Handler<ACE_SOCK_STREAM, ACE_MT_SYNCH>::open();
	if(nRet != 0)
	{
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::open]ACE_Svc_Handler<ACE_SOCK_STREAM, ACE_MT_SYNCH>::open() error [%d].\n", nRet));
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectHandler::open]ACE_Svc_Handler<ACE_SOCK_STREAM, ACE_MT_SYNCH>::open() error [%d].", nRet);
		return -1;
	}
	*/

	//��������Ϊ������ģʽ
	if (this->peer().enable(ACE_NONBLOCK) == -1)
	{
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::open]this->peer().enable  = ACE_NONBLOCK error.\n"));
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectHandler::open]this->peer().enable  = ACE_NONBLOCK error.");
		return -1;
	}

	//����Ĭ�ϱ���
	SetConnectName(m_addrRemote.get_host_addr());
	OUR_DEBUG((LM_INFO, "[CConnectHandler::open] Connection from [%s:%d]\n",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number()));

	//��ʼ����ǰ���ӵ�ĳЩ����
	m_atvConnect          = ACE_OS::gettimeofday();
	m_atvInput            = ACE_OS::gettimeofday();
	m_atvOutput           = ACE_OS::gettimeofday();
	m_atvSendAlive        = ACE_OS::gettimeofday();

	m_u4AllRecvCount      = 0;
	m_u4AllSendCount      = 0;
	m_u4AllRecvSize       = 0;
	m_u4AllSendSize       = 0;
	m_u8RecvQueueTimeCost = 0;
	m_u4RecvQueueCount    = 0;
	m_u8SendQueueTimeCost = 0;
	m_u4CurrSize          = 0;

	m_u4ReadSendSize      = 0;
	m_u4SuccessSendSize   = 0;

	//���ý��ջ���صĴ�С
	int nTecvBuffSize = MAX_MSG_SOCKETBUFF;
	//ACE_OS::setsockopt(this->get_handle(), SOL_SOCKET, SO_RCVBUF, (char* )&nTecvBuffSize, sizeof(nTecvBuffSize));
	ACE_OS::setsockopt(this->get_handle(), SOL_SOCKET, SO_SNDBUF, (char* )&nTecvBuffSize, sizeof(nTecvBuffSize));

	if(m_u2TcpNodelay == TCP_NODELAY_OFF)
	{
		//��������˽���Nagle�㷨��������Ҫ���á�
		int nOpt=1; 
		ACE_OS::setsockopt(this->get_handle(), IPPROTO_TCP, TCP_NODELAY, (char* )&nOpt, sizeof(int)); 
	}

	//int nOverTime = MAX_MSG_SENDTIMEOUT;
	//ACE_OS::setsockopt(this->get_handle(), SOL_SOCKET, SO_SNDTIMEO, (char* )&nOverTime, sizeof(nOverTime));

	m_pPacketParse = App_PacketParsePool::instance()->Create();
	if(NULL == m_pPacketParse)
	{
		OUR_DEBUG((LM_DEBUG,"[CConnectHandler::open] Open(%d) m_pPacketParse new error.\n", GetConnectID()));
		return -1;
	}

	//����ͷ�Ĵ�С��Ӧ��mb
	if(m_pPacketParse->GetPacketMode() == PACKET_WITHHEAD)
	{
		m_pCurrMessage = App_MessageBlockManager::instance()->Create(m_pPacketParse->GetPacketHeadSrcLen());
	}
	else
	{
		m_pCurrMessage = App_MessageBlockManager::instance()->Create(App_MainConfig::instance()->GetServerRecvBuff());
	}

	if(m_pCurrMessage == NULL)
	{
		//AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, m_u8RecvQueueTimeCost = %d, m_u4RecvQueueCount = %d, m_u8SendQueueTimeCost = %d.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, m_u8RecvQueueTimeCost, m_u4RecvQueueCount, m_u8SendQueueTimeCost);
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::open] pmb new is NULL.\n"));

		App_ConnectManager::instance()->Close(GetConnectID());
		return -1;
	}

	//��������ӷ������ӿ�
	if(false == App_ConnectManager::instance()->AddConnect(this))
	{
		OUR_DEBUG((LM_ERROR, "%s.\n", App_ConnectManager::instance()->GetError()));
		sprintf_safe(m_szError, MAX_BUFF_500, "%s", App_ConnectManager::instance()->GetError());
		return -1;
	}

	AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Connection from [%s:%d].",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number());

	//����PacketParse����Ӧ����
	App_PacketParseLoader::instance()->GetPacketParseInfo()->Connect(GetConnectID(), GetClientIPInfo(), GetLocalIPInfo());

	//��֯����
	_MakePacket objMakePacket;

	objMakePacket.m_u4ConnectID       = GetConnectID();
	objMakePacket.m_pPacketParse      = NULL;

	//�������ӽ�����Ϣ��
	ACE_Time_Value tvNow = ACE_OS::gettimeofday();
	if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_CONNECT, &objMakePacket, tvNow))
	{
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::open] ConnectID=%d, PACKET_CONNECT is error.\n", GetConnectID()));
	}

	m_u1ConnectState = CONNECT_OPEN;

	nRet = this->reactor()->register_handler(this, ACE_Event_Handler::READ_MASK);
	//OUR_DEBUG((LM_ERROR, "[CConnectHandler::open]ConnectID=%d, nRet=%d.\n", GetConnectID(), nRet));	

	return nRet;
}

//��������
int CConnectHandler::handle_input(ACE_HANDLE fd)
{
	//OUR_DEBUG((LM_ERROR, "[CConnectHandler::handle_input](0x%08x)ConnectID=%d,m_nIOCount=%d.\n", this, GetConnectID(), m_nIOCount));
	//ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadLock);
	//OUR_DEBUG((LM_ERROR, "[CConnectHandler::handle_input]ConnectID=%d,m_nIOCount=%d.\n", GetConnectID(), m_nIOCount));

	m_atvInput = ACE_OS::gettimeofday();

	if(fd == ACE_INVALID_HANDLE)
	{
		m_u4CurrSize = 0;
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::handle_input]fd == ACE_INVALID_HANDLE.\n"));
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectHandler::handle_input]fd == ACE_INVALID_HANDLE.");

		//��֯����
		_MakePacket objMakePacket;

		objMakePacket.m_u4ConnectID       = GetConnectID();
		objMakePacket.m_pPacketParse      = NULL;

		//���Ϳͻ������ӶϿ���Ϣ��
		ACE_Time_Value tvNow = ACE_OS::gettimeofday();
		if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_CDISCONNECT, &objMakePacket, tvNow))
		{
			OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
		}

		return -1;
	}

	//�ж����ݰ��ṹ�Ƿ�ΪNULL
	if(m_pPacketParse == NULL)
	{
		m_u4CurrSize = 0;
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::handle_input]ConnectID=%d, m_pPacketParse == NULL.\n", GetConnectID()));
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectHandler::handle_input]m_pPacketParse == NULL.");

		//��֯����
		_MakePacket objMakePacket;

		objMakePacket.m_u4ConnectID       = GetConnectID();
		objMakePacket.m_pPacketParse      = NULL;

		//���Ϳͻ������ӶϿ���Ϣ��
		ACE_Time_Value tvNow = ACE_OS::gettimeofday();
		if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_CDISCONNECT, &objMakePacket, tvNow))
		{
			OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
		}

		return -1;
	}

	//��л���ʯ�Ľ���
	//���￼�Ǵ����etģʽ��֧��
	if(App_MainConfig::instance()->GetNetworkMode() != (uint8)NETWORKMODE_RE_EPOLL_ET)
	{
		return RecvData();
	}
	else
	{
		return RecvData_et();
	}
}

//����������ݴ���
int CConnectHandler::RecvData()
{
	m_ThreadLock.acquire();
	m_nIOCount++;
	m_ThreadLock.release();	

	ACE_Time_Value nowait(0, MAX_QUEUE_TIMEOUT);

	//�жϻ����Ƿ�ΪNULL
	if(m_pCurrMessage == NULL)
	{
		m_u4CurrSize = 0;
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::RecvData]m_pCurrMessage == NULL.\n"));
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectHandler::RecvData]m_pCurrMessage == NULL.");

		//�رյ�ǰ��PacketParse
		ClearPacketParse();

		Close();
		return -1;
	}

	//����Ӧ�ý��յ����ݳ���
	int nCurrCount = 0;
	if(m_pPacketParse->GetIsHandleHead())
	{
		nCurrCount = (uint32)m_pPacketParse->GetPacketHeadSrcLen() - m_u4CurrSize;
	}
	else
	{
		nCurrCount = (uint32)m_pPacketParse->GetPacketBodySrcLen() - m_u4CurrSize;
	}

	//������Ҫ��m_u4CurrSize���м�顣
	if(nCurrCount < 0)
	{
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::RecvData][%d] nCurrCount < 0 m_u4CurrSize = %d.\n", GetConnectID(), m_u4CurrSize));
		m_u4CurrSize = 0;

		//�رյ�ǰ��PacketParse
		ClearPacketParse();

		Close();
		return -1;
	}

	int nDataLen = this->peer().recv(m_pCurrMessage->wr_ptr(), nCurrCount, MSG_NOSIGNAL, &nowait);
	if(nDataLen <= 0)
	{
		m_u4CurrSize = 0;
		uint32 u4Error = (uint32)errno;
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::RecvData] ConnectID=%d, recv data is error nDataLen = [%d] errno = [%d].\n", GetConnectID(), nDataLen, u4Error));
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectHandler::RecvData] ConnectID = %d, recv data is error[%d].\n", GetConnectID(), nDataLen);

		//�رյ�ǰ��PacketParse
		ClearPacketParse();

		Close();
		return -1;
	}

	//�����DEBUG״̬����¼��ǰ���ܰ��Ķ���������
	if(App_MainConfig::instance()->GetDebug() == DEBUG_ON || m_blIsLog == true)
	{
		char szDebugData[MAX_BUFF_1024] = {'\0'};
		char szLog[10]  = {'\0'};
		int  nDebugSize = 0; 
		bool blblMore   = false;

		if(nDataLen >= MAX_BUFF_200)
		{
			nDebugSize = MAX_BUFF_200;
			blblMore   = true;
		}
		else
		{
			nDebugSize = nDataLen;
		}

		char* pData = m_pCurrMessage->wr_ptr();
		for(int i = 0; i < nDebugSize; i++)
		{
			sprintf_safe(szLog, 10, "0x%02X ", (unsigned char)pData[i]);
			sprintf_safe(szDebugData + 5*i, MAX_BUFF_1024 - 5*i, "%s", szLog);
		}

		if(blblMore == true)
		{
			AppLogManager::instance()->WriteLog(LOG_SYSTEM_DEBUG_CLIENTRECV, "[(%s)%s:%d]%s.(���ݰ�����ֻ��¼ǰ200�ֽ�)", m_szConnectName, m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), szDebugData);
		}
		else
		{
			AppLogManager::instance()->WriteLog(LOG_SYSTEM_DEBUG_CLIENTRECV, "[(%s)%s:%d]%s.", m_szConnectName, m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), szDebugData);
		}
	}

	m_u4CurrSize += nDataLen;

	m_pCurrMessage->wr_ptr(nDataLen);

	if(m_pPacketParse->GetPacketMode() == PACKET_WITHHEAD)
	{
		//���û�ж��꣬�̶�
		if(nCurrCount - nDataLen > 0)
		{
			Close();
			return 0;
		}
        else if(m_pCurrMessage->length() == m_pPacketParse->GetPacketHeadSrcLen() && m_pPacketParse->GetIsHandleHead())
		{
			_Head_Info objHeadInfo;
			bool blStateHead = App_PacketParseLoader::instance()->GetPacketParseInfo()->Parse_Packet_Head_Info(GetConnectID(), m_pCurrMessage, App_MessageBlockManager::instance(), &objHeadInfo);
			if(false == blStateHead)
			{
				m_u4CurrSize = 0;
				OUR_DEBUG((LM_ERROR, "[CConnectHandler::RecvData]SetPacketHead is false.\n"));

				//�رյ�ǰ��PacketParse
				ClearPacketParse();

				Close();
				return -1;
			}
			else
			{
				m_pPacketParse->SetPacket_IsHandleHead(false);
				m_pPacketParse->SetPacket_Head_Message(objHeadInfo.m_pmbHead);
				m_pPacketParse->SetPacket_Head_Curr_Length(objHeadInfo.m_u4HeadCurrLen);
				m_pPacketParse->SetPacket_Body_Src_Length(objHeadInfo.m_u4BodySrcLen);
				m_pPacketParse->SetPacket_CommandID(objHeadInfo.m_u2PacketCommandID);
			}

			uint32 u4PacketBodyLen = m_pPacketParse->GetPacketBodySrcLen();
			m_u4CurrSize = 0;


			//�������ֻ�����ͷ������
			//�������ֻ�а�ͷ������Ҫ���壬�����������һЩ����������ֻ�����ͷ���ӵ�DoMessage()
			if(u4PacketBodyLen == 0)
			{
				//ֻ�����ݰ�ͷ
				if(false == CheckMessage())
				{
					Close();
					return -1;
				}

				m_u4CurrSize = 0;

				//�����µİ�
				m_pPacketParse = App_PacketParsePool::instance()->Create();
				if(NULL == m_pPacketParse)
				{
					OUR_DEBUG((LM_DEBUG,"[%t|CConnectHandle::RecvData] Open(%d) m_pPacketParse new error.\n", GetConnectID()));
					Close();
					return -1;
				}

				//����ͷ�Ĵ�С��Ӧ��mb
				m_pCurrMessage = App_MessageBlockManager::instance()->Create(m_pPacketParse->GetPacketHeadSrcLen());
				if(m_pCurrMessage == NULL)
				{
					AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, m_u8RecvQueueTimeCost = %dws, m_u4RecvQueueCount = %d, m_u8SendQueueTimeCost = %dws.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, (uint32)m_u8RecvQueueTimeCost, m_u4RecvQueueCount, (uint32)m_u8SendQueueTimeCost);
					OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvData] pmb new is NULL.\n"));

					//��֯����
					_MakePacket objMakePacket;

					objMakePacket.m_u4ConnectID       = GetConnectID();
					objMakePacket.m_pPacketParse      = NULL;

					//���Ϳͻ������ӶϿ���Ϣ��
					ACE_Time_Value tvNow = ACE_OS::gettimeofday();
					if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_CDISCONNECT, &objMakePacket, tvNow))
					{
						OUR_DEBUG((LM_ERROR, "[CProConnectHandle::RecvData] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
					}

					Close();
					return -1;
				}
			}
			else
			{
				//����������������ȣ�Ϊ�Ƿ�����
				if(u4PacketBodyLen >= m_u4MaxPacketSize)
				{
					m_u4CurrSize = 0;
					OUR_DEBUG((LM_ERROR, "[CConnectHandler::RecvData]u4PacketHeadLen(%d) more than %d.\n", u4PacketBodyLen, m_u4MaxPacketSize));

					//�رյ�ǰ��PacketParse
					ClearPacketParse();

					Close();
					return -1;
				}
				else
				{
					//OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvClinetPacket] m_pPacketParse->GetPacketBodyLen())=%d.\n", m_pPacketParse->GetPacketBodyLen()));
					//����ͷ�Ĵ�С��Ӧ��mb
					m_pCurrMessage = App_MessageBlockManager::instance()->Create(m_pPacketParse->GetPacketBodySrcLen());
					if(m_pCurrMessage == NULL)
					{
						m_u4CurrSize = 0;
						//AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, m_u8RecvQueueTimeCost = %d, m_u4RecvQueueCount = %d, m_u8SendQueueTimeCost = %d.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, m_u8RecvQueueTimeCost, m_u4RecvQueueCount, m_u8SendQueueTimeCost);
						OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvData] pmb new is NULL.\n"));

						//�رյ�ǰ��PacketParse
						ClearPacketParse();

						Close();
						return -1;
					}
				}
			}

		}
		else
		{
			//��������������ɣ���ʼ�����������ݰ�
			_Body_Info obj_Body_Info;
			bool blStateBody = App_PacketParseLoader::instance()->GetPacketParseInfo()->Parse_Packet_Body_Info(GetConnectID(), m_pCurrMessage, App_MessageBlockManager::instance(), &obj_Body_Info);
			if(false == blStateBody)
			{
				//������ݰ����Ǵ���ģ���Ͽ�����
				m_u4CurrSize = 0;
				OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvData]SetPacketBody is false.\n"));

				//�رյ�ǰ��PacketParse
				ClearPacketParse();

				Close();
				return -1;
			}
			else
			{
				m_pPacketParse->SetPacket_Body_Message(obj_Body_Info.m_pmbBody);
				m_pPacketParse->SetPacket_Body_Curr_Length(obj_Body_Info.m_u4BodyCurrLen);
				if(obj_Body_Info.m_u2PacketCommandID > 0)
				{
					m_pPacketParse->SetPacket_CommandID(obj_Body_Info.m_u2PacketCommandID);
				}
			}

			if(false == CheckMessage())
			{
				Close();
				return -1;
			}

			m_u4CurrSize = 0;

			//�����µİ�
			m_pPacketParse = App_PacketParsePool::instance()->Create();
			if(NULL == m_pPacketParse)
			{
				OUR_DEBUG((LM_DEBUG,"[%t|CConnectHandle::RecvData] Open(%d) m_pPacketParse new error.\n", GetConnectID()));
				Close();
				return -1;
			}

			//����ͷ�Ĵ�С��Ӧ��mb
			m_pCurrMessage = App_MessageBlockManager::instance()->Create(m_pPacketParse->GetPacketHeadSrcLen());
			if(m_pCurrMessage == NULL)
			{
				AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, m_u8RecvQueueTimeCost = %dws, m_u4RecvQueueCount = %d, m_u8SendQueueTimeCost = %dws.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, (uint32)m_u8RecvQueueTimeCost, m_u4RecvQueueCount, (uint32)m_u8SendQueueTimeCost);
				OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvData] pmb new is NULL.\n"));

				//��֯����
				_MakePacket objMakePacket;

				objMakePacket.m_u4ConnectID       = GetConnectID();
				objMakePacket.m_pPacketParse      = NULL;

				//���Ϳͻ������ӶϿ���Ϣ��
				ACE_Time_Value tvNow = ACE_OS::gettimeofday();
				if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_CDISCONNECT, &objMakePacket, tvNow))
				{
					OUR_DEBUG((LM_ERROR, "[CProConnectHandle::RecvData] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
				}

				Close();
				return -1;
			}
		}
	}
	else
	{
		//����ģʽ����
		while(true)
		{
			_Packet_Info obj_Packet_Info;
			uint8 n1Ret = App_PacketParseLoader::instance()->GetPacketParseInfo()->Parse_Packet_Stream(GetConnectID(), m_pCurrMessage, (IMessageBlockManager* )App_MessageBlockManager::instance(), &obj_Packet_Info);
			if(PACKET_GET_ENOUGTH == n1Ret)
			{
				m_pPacketParse->SetPacket_Head_Message(obj_Packet_Info.m_pmbHead);
				m_pPacketParse->SetPacket_Body_Message(obj_Packet_Info.m_pmbBody);
				m_pPacketParse->SetPacket_CommandID(obj_Packet_Info.m_u2PacketCommandID);
				m_pPacketParse->SetPacket_Head_Src_Length(obj_Packet_Info.m_u4HeadSrcLen);
				m_pPacketParse->SetPacket_Head_Curr_Length(obj_Packet_Info.m_u4HeadCurrLen);
				m_pPacketParse->SetPacket_Head_Src_Length(obj_Packet_Info.m_u4BodySrcLen);
				m_pPacketParse->SetPacket_Body_Curr_Length(obj_Packet_Info.m_u4BodyCurrLen);

				if(false == CheckMessage())
				{
					Close();
					return -1;
				}

				m_u4CurrSize = 0;

				//�����µİ�
				m_pPacketParse = App_PacketParsePool::instance()->Create();
				if(NULL == m_pPacketParse)
				{
					OUR_DEBUG((LM_DEBUG,"[%t|CConnectHandle::RecvData] Open(%d) m_pPacketParse new error.\n", GetConnectID()));
					Close();
					return -1;
				}

				//�����Ƿ���������
				if(m_pCurrMessage->length() == 0)
				{
					break;
				}
				else
				{
					//�������ݣ���������
					continue;
				}

			}
			else if(PACKET_GET_NO_ENOUGTH == n1Ret)
			{
				break;
			}
			else
			{
				m_pPacketParse->Clear();

				AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, m_u8RecvQueueTimeCost = %dws, m_u4RecvQueueCount = %d, m_u8SendQueueTimeCost = %dws.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, (uint32)m_u8RecvQueueTimeCost, m_u4RecvQueueCount, (uint32)m_u8SendQueueTimeCost);
				OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvData] pmb new is NULL.\n"));

				//��֯����
				_MakePacket objMakePacket;

				objMakePacket.m_u4ConnectID       = GetConnectID();
				objMakePacket.m_pPacketParse      = NULL;

				//���Ϳͻ������ӶϿ���Ϣ��
				ACE_Time_Value tvNow = ACE_OS::gettimeofday();
				if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_CDISCONNECT, &objMakePacket, tvNow))
				{
					OUR_DEBUG((LM_ERROR, "[CProConnectHandle::RecvData] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
				}

				Close();
				return -1;
			}
		}

		App_MessageBlockManager::instance()->Close(m_pCurrMessage);
		m_u4CurrSize = 0;

		//����ͷ�Ĵ�С��Ӧ��mb
		m_pCurrMessage = App_MessageBlockManager::instance()->Create(App_MainConfig::instance()->GetServerRecvBuff());
		if(m_pCurrMessage == NULL)
		{
			AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, m_u8RecvQueueTimeCost = %dws, m_u4RecvQueueCount = %d, m_u8SendQueueTimeCost = %dws.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, (uint32)m_u8RecvQueueTimeCost, m_u4RecvQueueCount, (uint32)m_u8SendQueueTimeCost);
			OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvData] pmb new is NULL.\n"));

			//��֯����
			_MakePacket objMakePacket;

			objMakePacket.m_u4ConnectID       = GetConnectID();
			objMakePacket.m_pPacketParse      = NULL;

			//���Ϳͻ������ӶϿ���Ϣ��
			ACE_Time_Value tvNow = ACE_OS::gettimeofday();
			if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_CDISCONNECT, &objMakePacket, tvNow))
			{
				OUR_DEBUG((LM_ERROR, "[CProConnectHandle::RecvData] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
			}

			Close();
			return -1;
		}
	}

	Close();
	return 0;	
}

//etģʽ��������
int CConnectHandler::RecvData_et()
{
	m_ThreadLock.acquire();
	m_nIOCount++;
	m_ThreadLock.release();

	while(true)
	{
		//OUR_DEBUG((LM_ERROR, "[CConnectHandler::RecvData_et]m_nIOCount=%d.\n", m_nIOCount));

		//�жϻ����Ƿ�ΪNULL
		if(m_pCurrMessage == NULL)
		{
			m_u4CurrSize = 0;
			OUR_DEBUG((LM_ERROR, "[CConnectHandler::RecvData_et]m_pCurrMessage == NULL.\n"));
			sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectHandler::RecvData_et]m_pCurrMessage == NULL.");

			//�رյ�ǰ��PacketParse
			ClearPacketParse();

			Close();
			return -1;
		}

		//����Ӧ�ý��յ����ݳ���
		int nCurrCount = 0;
		if(m_pPacketParse->GetIsHandleHead())
		{
			nCurrCount = (uint32)m_pPacketParse->GetPacketHeadSrcLen() - m_u4CurrSize;
		}
		else
		{
			nCurrCount = (uint32)m_pPacketParse->GetPacketBodySrcLen() - m_u4CurrSize;
		}

		//������Ҫ��m_u4CurrSize���м�顣
		if(nCurrCount < 0)
		{
			OUR_DEBUG((LM_ERROR, "[CConnectHandler::RecvData_et][%d] nCurrCount < 0 m_u4CurrSize = %d.\n", GetConnectID(), m_u4CurrSize));
			m_u4CurrSize = 0;

			//�رյ�ǰ��PacketParse
			ClearPacketParse();

			Close();

			return -1;
		}

		int nDataLen = this->peer().recv(m_pCurrMessage->wr_ptr(), nCurrCount, MSG_NOSIGNAL);
		//OUR_DEBUG((LM_ERROR, "[CConnectHandler::handle_input] ConnectID=%d, GetData=[%d],errno=[%d].\n", GetConnectID(), nDataLen, errno));
		if(nDataLen <= 0)
		{
			m_u4CurrSize = 0;
			uint32 u4Error = (uint32)errno;

			//�����-1 ��Ϊ11�Ĵ��󣬺���֮
			if(nDataLen == -1 && u4Error == EAGAIN)
			{
				break;
			}

			OUR_DEBUG((LM_ERROR, "[CConnectHandler::RecvData_et] ConnectID = %d, recv data is error nDataLen = [%d] errno = [%d] EAGAIN=[%d].\n", GetConnectID(), nDataLen, u4Error, EAGAIN));
			sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectHandler::RecvData_et] ConnectID = %d,nDataLen = [%d],recv data is error[%d].\n", GetConnectID(), nDataLen, u4Error);

			//�رյ�ǰ��PacketParse
			ClearPacketParse();

			Close();
			return -1;
		}

		//�����DEBUG״̬����¼��ǰ���ܰ��Ķ���������
		if(App_MainConfig::instance()->GetDebug() == DEBUG_ON || m_blIsLog == true)
		{
			char szDebugData[MAX_BUFF_1024] = {'\0'};
			char szLog[10]  = {'\0'};
			int  nDebugSize = 0; 
			bool blblMore   = false;

			if(nDataLen >= MAX_BUFF_200)
			{
				nDebugSize = MAX_BUFF_200;
				blblMore   = true;
			}
			else
			{
				nDebugSize = nDataLen;
			}

			char* pData = m_pCurrMessage->wr_ptr();
			for(int i = 0; i < nDebugSize; i++)
			{
				sprintf_safe(szLog, 10, "0x%02X ", (unsigned char)pData[i]);
				sprintf_safe(szDebugData + 5*i, MAX_BUFF_1024 - 5*i, "%s", szLog);
			}

			if(blblMore == true)
			{
				AppLogManager::instance()->WriteLog(LOG_SYSTEM_DEBUG_CLIENTRECV, "[(%s)%s:%d]%s.(���ݰ�����ֻ��¼ǰ200�ֽ�)", m_szConnectName, m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), szDebugData);
			}
			else
			{
				AppLogManager::instance()->WriteLog(LOG_SYSTEM_DEBUG_CLIENTRECV, "[(%s)%s:%d]%s.", m_szConnectName, m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), szDebugData);
			}
		}

		m_u4CurrSize += nDataLen;

		m_pCurrMessage->wr_ptr(nDataLen);

		if(m_pPacketParse->GetPacketMode() == PACKET_WITHHEAD)
		{
			//���û�ж��꣬�̶�
			if(nCurrCount - nDataLen > 0)
			{
				Close();
				return 0;
			}
            else if(m_pCurrMessage->length() == m_pPacketParse->GetPacketHeadSrcLen() && m_pPacketParse->GetIsHandleHead())
			{
				_Head_Info objHeadInfo;
				bool blStateHead = App_PacketParseLoader::instance()->GetPacketParseInfo()->Parse_Packet_Head_Info(GetConnectID(), m_pCurrMessage, App_MessageBlockManager::instance(), &objHeadInfo);				
				if(false == blStateHead)
				{
					m_u4CurrSize = 0;
					OUR_DEBUG((LM_ERROR, "[CConnectHandler::RecvData_et]SetPacketHead is false.\n"));

					//�رյ�ǰ��PacketParse
					ClearPacketParse();

					Close();
					return -1;
				}
				else
				{
					m_pPacketParse->SetPacket_IsHandleHead(false);
					m_pPacketParse->SetPacket_Head_Message(objHeadInfo.m_pmbHead);
					m_pPacketParse->SetPacket_Head_Curr_Length(objHeadInfo.m_u4HeadCurrLen);
					m_pPacketParse->SetPacket_Body_Src_Length(objHeadInfo.m_u4BodySrcLen);
					m_pPacketParse->SetPacket_CommandID(objHeadInfo.m_u2PacketCommandID);
				}

				uint32 u4PacketBodyLen = m_pPacketParse->GetPacketBodySrcLen();
				m_u4CurrSize = 0;


				//�������ֻ�����ͷ������
				//�������ֻ�а�ͷ������Ҫ���壬�����������һЩ����������ֻ�����ͷ���ӵ�DoMessage()
				if(u4PacketBodyLen == 0)
				{
					//ֻ�����ݰ�ͷ
					if(false == CheckMessage())
					{
						Close();
						return -1;
					}

					m_u4CurrSize = 0;

					//�����µİ�
					m_pPacketParse = App_PacketParsePool::instance()->Create();
					if(NULL == m_pPacketParse)
					{
						OUR_DEBUG((LM_DEBUG,"[%t|CConnectHandle::RecvData_et] Open(%d) m_pPacketParse new error.\n", GetConnectID()));
						Close();
						return -1;
					}

					//����ͷ�Ĵ�С��Ӧ��mb
					m_pCurrMessage = App_MessageBlockManager::instance()->Create(m_pPacketParse->GetPacketHeadSrcLen());
					if(m_pCurrMessage == NULL)
					{
						AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, m_u8RecvQueueTimeCost = %dws, m_u4RecvQueueCount = %d, m_u8SendQueueTimeCost = %dws.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, (uint32)m_u8RecvQueueTimeCost, m_u4RecvQueueCount, (uint32)m_u8SendQueueTimeCost);
						OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvData_et] pmb new is NULL.\n"));

						//��֯����
						_MakePacket objMakePacket;

						objMakePacket.m_u4ConnectID       = GetConnectID();
						objMakePacket.m_pPacketParse      = NULL;

						//���Ϳͻ������ӶϿ���Ϣ��
						ACE_Time_Value tvNow = ACE_OS::gettimeofday();
						if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_CDISCONNECT, &objMakePacket, tvNow))
						{
							OUR_DEBUG((LM_ERROR, "[CProConnectHandle::RecvData_et] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
						}

						Close();
						return -1;
					}
				}
				else
				{
					//����������������ȣ�Ϊ�Ƿ�����
					if(u4PacketBodyLen >= m_u4MaxPacketSize)
					{
						m_u4CurrSize = 0;
						OUR_DEBUG((LM_ERROR, "[CConnectHandler::RecvData_et]u4PacketHeadLen(%d) more than %d.\n", u4PacketBodyLen, m_u4MaxPacketSize));

						Close();
						//�رյ�ǰ��PacketParse
						ClearPacketParse();

						return -1;
					}
					else
					{
						//OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvClinetPacket] m_pPacketParse->GetPacketBodyLen())=%d.\n", m_pPacketParse->GetPacketBodyLen()));
						//����ͷ�Ĵ�С��Ӧ��mb
						m_pCurrMessage = App_MessageBlockManager::instance()->Create(m_pPacketParse->GetPacketBodySrcLen());
						if(m_pCurrMessage == NULL)
						{
							m_u4CurrSize = 0;
							//AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, m_u8RecvQueueTimeCost = %d, m_u4RecvQueueCount = %d, m_u8SendQueueTimeCost = %d.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, m_u8RecvQueueTimeCost, m_u4RecvQueueCount, m_u8SendQueueTimeCost);
							OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvData_et] pmb new is NULL.\n"));

							Close();
							//�رյ�ǰ��PacketParse
							ClearPacketParse();

							return -1;
						}
					}
				}
			}
			else
			{
				//��������������ɣ���ʼ�����������ݰ�
				_Body_Info obj_Body_Info;
				bool blStateBody = App_PacketParseLoader::instance()->GetPacketParseInfo()->Parse_Packet_Body_Info(GetConnectID(), m_pCurrMessage, App_MessageBlockManager::instance(), &obj_Body_Info);
				if(false == blStateBody)
				{
					//������ݰ����Ǵ���ģ���Ͽ�����
					m_u4CurrSize = 0;
					OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvData_et]SetPacketBody is false.\n"));

					Close();	
					//�رյ�ǰ��PacketParse
					ClearPacketParse();

					return -1;
				}
				else
				{
					m_pPacketParse->SetPacket_Body_Message(obj_Body_Info.m_pmbBody);
					m_pPacketParse->SetPacket_Body_Curr_Length(obj_Body_Info.m_u4BodyCurrLen);
					if(obj_Body_Info.m_u2PacketCommandID > 0)
					{
						m_pPacketParse->SetPacket_CommandID(obj_Body_Info.m_u2PacketCommandID);
					}
				}

				if(false == CheckMessage())
				{
					Close();
					return -1;
				}

				m_u4CurrSize = 0;

				//�����µİ�
				m_pPacketParse = App_PacketParsePool::instance()->Create();
				if(NULL == m_pPacketParse)
				{
					OUR_DEBUG((LM_DEBUG,"[%t|CConnectHandle::RecvData_et] Open(%d) m_pPacketParse new error.\n", GetConnectID()));
					Close();
					return -1;
				}

				//����ͷ�Ĵ�С��Ӧ��mb
				m_pCurrMessage = App_MessageBlockManager::instance()->Create(m_pPacketParse->GetPacketHeadSrcLen());
				if(m_pCurrMessage == NULL)
				{
					AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, m_u8RecvQueueTimeCost = %dws, m_u4RecvQueueCount = %d, m_u8SendQueueTimeCost = %dws.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, (uint32)m_u8RecvQueueTimeCost, m_u4RecvQueueCount, (uint32)m_u8SendQueueTimeCost);
					OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvData_et] pmb new is NULL.\n"));

					//��֯����
					_MakePacket objMakePacket;

					objMakePacket.m_u4ConnectID       = GetConnectID();
					objMakePacket.m_pPacketParse      = NULL;

					//���Ϳͻ������ӶϿ���Ϣ��
					ACE_Time_Value tvNow = ACE_OS::gettimeofday();
					if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_CDISCONNECT, &objMakePacket, tvNow))
					{
						OUR_DEBUG((LM_ERROR, "[CProConnectHandle::RecvData_et] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
					}

					Close();
					return -1;
				}
			}
		}
		else
		{
			//����ģʽ����
			while(true)
			{
				_Packet_Info obj_Packet_Info;
				uint8 n1Ret = App_PacketParseLoader::instance()->GetPacketParseInfo()->Parse_Packet_Stream(GetConnectID(), m_pCurrMessage, (IMessageBlockManager* )App_MessageBlockManager::instance(), &obj_Packet_Info);
				if(PACKET_GET_ENOUGTH == n1Ret)
				{
					m_pPacketParse->SetPacket_Head_Message(obj_Packet_Info.m_pmbHead);
					m_pPacketParse->SetPacket_Body_Message(obj_Packet_Info.m_pmbBody);
					m_pPacketParse->SetPacket_CommandID(obj_Packet_Info.m_u2PacketCommandID);
					m_pPacketParse->SetPacket_Head_Src_Length(obj_Packet_Info.m_u4HeadSrcLen);
					m_pPacketParse->SetPacket_Head_Curr_Length(obj_Packet_Info.m_u4HeadCurrLen);
					m_pPacketParse->SetPacket_Head_Src_Length(obj_Packet_Info.m_u4BodySrcLen);
					m_pPacketParse->SetPacket_Body_Curr_Length(obj_Packet_Info.m_u4BodyCurrLen);

					if(false == CheckMessage())
					{
						Close();
						return -1;
					}

					m_u4CurrSize = 0;

					//�����µİ�
					m_pPacketParse = App_PacketParsePool::instance()->Create();
					if(NULL == m_pPacketParse)
					{
						OUR_DEBUG((LM_DEBUG,"[%t|CConnectHandle::RecvData_et] Open(%d) m_pPacketParse new error.\n", GetConnectID()));
						return -1;
					}

					//�����Ƿ���������
					if(m_pCurrMessage->length() == 0)
					{
						break;
					}
					else
					{
						//�������ݣ���������
						continue;
					}

				}
				else if(PACKET_GET_NO_ENOUGTH == n1Ret)
				{
					break;
				}
				else
				{
					m_pPacketParse->Clear();

					AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, m_u8RecvQueueTimeCost = %dws, m_u4RecvQueueCount = %d, m_u8SendQueueTimeCost = %dws.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, (uint32)m_u8RecvQueueTimeCost, m_u4RecvQueueCount, (uint32)m_u8SendQueueTimeCost);
					OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvData_et] pmb new is NULL.\n"));

					//��֯����
					_MakePacket objMakePacket;

					objMakePacket.m_u4ConnectID       = GetConnectID();
					objMakePacket.m_pPacketParse      = NULL;

					//���Ϳͻ������ӶϿ���Ϣ��
					ACE_Time_Value tvNow = ACE_OS::gettimeofday();
					if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_CDISCONNECT, &objMakePacket, tvNow))
					{
						OUR_DEBUG((LM_ERROR, "[CProConnectHandle::RecvData_et] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
					}

					Close();
					return -1;
				}
			}

			App_MessageBlockManager::instance()->Close(m_pCurrMessage);
			m_u4CurrSize = 0;

			//����ͷ�Ĵ�С��Ӧ��mb
			m_pCurrMessage = App_MessageBlockManager::instance()->Create(App_MainConfig::instance()->GetServerRecvBuff());
			if(m_pCurrMessage == NULL)
			{
				AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "Close Connection from [%s:%d] RecvSize = %d, RecvCount = %d, SendSize = %d, SendCount = %d, m_u8RecvQueueTimeCost = %dws, m_u4RecvQueueCount = %d, m_u8SendQueueTimeCost = %dws.",m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllRecvCount, m_u4AllSendSize, m_u4AllSendCount, (uint32)m_u8RecvQueueTimeCost, m_u4RecvQueueCount, (uint32)m_u8SendQueueTimeCost);
				OUR_DEBUG((LM_ERROR, "[CConnectHandle::RecvData_et] pmb new is NULL.\n"));

				//��֯����
				_MakePacket objMakePacket;

				objMakePacket.m_u4ConnectID       = GetConnectID();
				objMakePacket.m_pPacketParse      = NULL;

				//���Ϳͻ������ӶϿ���Ϣ��
				ACE_Time_Value vtNow = ACE_OS::gettimeofday();
				if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_CDISCONNECT, &objMakePacket, vtNow))
				{
					OUR_DEBUG((LM_ERROR, "[CProConnectHandle::RecvData_et] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
				}

				Close();
				return -1;
			}
		}
	}

	Close();
	return 0;		
}

//�ر�����
int CConnectHandler::handle_close(ACE_HANDLE h, ACE_Reactor_Mask mask)
{
	if(h == ACE_INVALID_HANDLE)
	{
		OUR_DEBUG((LM_DEBUG,"[CConnectHandler::handle_close] h is NULL mask=%d.\n", (int)mask));
	}

	//OUR_DEBUG((LM_DEBUG,"[CConnectHandler::handle_close]Connectid=[%d] begin(%d)...\n",GetConnectID(), errno));
	//App_ConnectManager::instance()->Close(GetConnectID());
	//OUR_DEBUG((LM_DEBUG,"[CConnectHandler::handle_close] Connectid=[%d] finish ok...\n", GetConnectID()));
	Close(2);

	return 0;
}

bool CConnectHandler::CheckAlive(ACE_Time_Value& tvNow)
{
	//ACE_Time_Value tvNow = ACE_OS::gettimeofday();
	ACE_Time_Value tvIntval(tvNow - m_atvInput);
	if(tvIntval.sec() > m_u2MaxConnectTime)
	{
		//������������ʱ�䣬��������ر�����
		OUR_DEBUG ((LM_ERROR, "[CConnectHandle::CheckAlive] Connectid=%d Server Close!\n", GetConnectID()));
		return false;
	}
	else
	{
		return true;
	}
}

bool CConnectHandler::SetRecvQueueTimeCost(uint32 u4TimeCost)
{
	//���������ֵ�����¼����־��ȥ
	if((uint32)(m_u8RecvQueueTimeout * 1000) <= u4TimeCost)
	{
		AppLogManager::instance()->WriteLog(LOG_SYSTEM_RECVQUEUEERROR, "[TCP]IP=%s,Prot=%d, m_u8RecvQueueTimeout=[%d], Timeout=[%d].", GetClientIPInfo().m_szClientIP, GetClientIPInfo().m_nPort, (uint32)m_u8RecvQueueTimeout, u4TimeCost);
	}

	m_u4RecvQueueCount++;

	return true;
}

bool CConnectHandler::SetSendQueueTimeCost(uint32 u4TimeCost)
{
	//���������ֵ�����¼����־��ȥ
	if((uint32)(m_u8SendQueueTimeout) <= u4TimeCost)
	{
		ACE_Time_Value tvNow = ACE_OS::gettimeofday();
		AppLogManager::instance()->WriteLog(LOG_SYSTEM_SENDQUEUEERROR, "[TCP]IP=%s,Prot=%d,m_u8SendQueueTimeout = [%d], Timeout=[%d].", GetClientIPInfo().m_szClientIP, GetClientIPInfo().m_nPort, (uint32)m_u8SendQueueTimeout, u4TimeCost);

		//��֯����
		_MakePacket objMakePacket;

		objMakePacket.m_u4ConnectID       = GetConnectID();
		objMakePacket.m_pPacketParse      = NULL;

		//���߲�����ӷ��ͳ�ʱ��ֵ����
		if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_SEND_TIMEOUT, &objMakePacket, tvNow))
		{
			OUR_DEBUG((LM_ERROR, "[CProConnectHandle::open] ConnectID = %d, PACKET_CONNECT is error.\n", GetConnectID()));
		}
	}

	return true;
}

uint8 CConnectHandler::GetConnectState()
{
	return m_u1ConnectState;
}

uint8 CConnectHandler::GetSendBuffState()
{
	return m_u1SendBuffState;
}

bool CConnectHandler::SendMessage(uint16 u2CommandID, IBuffPacket* pBuffPacket, bool blState, uint8 u1SendType, uint32& u4PacketSize, bool blDelete)
{
	//OUR_DEBUG((LM_DEBUG,"[CConnectHandler::SendMessage](0x%08x) Connectid=%d,m_nIOCount=%d.\n", this, GetConnectID(), m_nIOCount));
	ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadLock);
	//OUR_DEBUG((LM_DEBUG,"[CConnectHandler::SendMessage]222 Connectid=%d,m_nIOCount=%d.\n", GetConnectID(), m_nIOCount));

	//�����ǰ�����ѱ�����̹߳رգ������ﲻ������ֱ���˳�
	if(m_u1IsActive == 0)
	{
		if(blDelete == true)
		{					
			App_BuffPacketManager::instance()->Delete(pBuffPacket);
		}
		return false;
	}

	uint32 u4SendSuc = pBuffPacket->GetPacketLen();

	ACE_Message_Block* pMbData = NULL;

	//�������ֱ�ӷ������ݣ���ƴ�����ݰ�
	if(blState == PACKET_SEND_CACHE)
	{
		//���ж�Ҫ���͵����ݳ��ȣ������Ƿ���Է��뻺�壬�����Ƿ��Ѿ�������
		uint32 u4SendPacketSize = 0;
		if(u1SendType == SENDMESSAGE_NOMAL)
		{
			u4SendPacketSize = App_PacketParseLoader::instance()->GetPacketParseInfo()->Make_Send_Packet_Length(GetConnectID(), pBuffPacket->GetPacketLen(), u2CommandID);
		}
		else
		{
			u4SendPacketSize = (uint32)pBuffPacket->GetPacketLen();
		}
		u4PacketSize = u4SendPacketSize;

		if(u4SendPacketSize + (uint32)m_pBlockMessage->length() >= m_u4SendMaxBuffSize)
		{
			OUR_DEBUG((LM_DEBUG,"[CConnectHandler::SendMessage] Connectid=[%d] m_pBlockMessage is not enougth.\n", GetConnectID()));
			if(blDelete == true)
			{					
				App_BuffPacketManager::instance()->Delete(pBuffPacket);
			}
			return false;
		}
		else
		{
			//��ӽ�������
			//ACE_Message_Block* pMbBufferData = NULL;

			//SENDMESSAGE_NOMAL����Ҫ��ͷ��ʱ�򣬷��򣬲����ֱ�ӷ���
			if(u1SendType == SENDMESSAGE_NOMAL)
			{
				//������ɷ������ݰ�
				 App_PacketParseLoader::instance()->GetPacketParseInfo()->Make_Send_Packet(GetConnectID(), pBuffPacket->GetData(), pBuffPacket->GetPacketLen(), m_pBlockMessage, u2CommandID);
			}
			else
			{
				//�������SENDMESSAGE_NOMAL����ֱ�����
				memcpy_safe((char* )pBuffPacket->GetData(), pBuffPacket->GetPacketLen(), m_pBlockMessage->wr_ptr(), pBuffPacket->GetPacketLen());
				m_pBlockMessage->wr_ptr(pBuffPacket->GetPacketLen());
			}
		}

		if(blDelete == true)
		{
			//ɾ���������ݰ� 
			App_BuffPacketManager::instance()->Delete(pBuffPacket);
		}

		//������ɣ��������˳�
		return true;
	}
	else
	{
		//OUR_DEBUG((LM_DEBUG,"[CConnectHandler::SendMessage]Connectid=%d,333.\n", GetConnectID()));
		//���ж��Ƿ�Ҫ��װ��ͷ�������Ҫ������װ��m_pBlockMessage��
		uint32 u4SendPacketSize = 0;
		if(u1SendType == SENDMESSAGE_NOMAL)
		{
			u4SendPacketSize = App_PacketParseLoader::instance()->GetPacketParseInfo()->Make_Send_Packet_Length(GetConnectID(), pBuffPacket->GetPacketLen(), u2CommandID);

			if(u4SendPacketSize >= m_u4SendMaxBuffSize)
			{
				OUR_DEBUG((LM_DEBUG,"[CConnectHandler::SendMessage](%d) u4SendPacketSize is more than(%d)(%d).\n", GetConnectID(), u4SendPacketSize, m_u4SendMaxBuffSize));
				if(blDelete == true)
				{
					//ɾ���������ݰ� 
					App_BuffPacketManager::instance()->Delete(pBuffPacket);
				}
				return false;
			}

			//OUR_DEBUG((LM_DEBUG,"[CConnectHandler::SendMessage] Connectid=[%d] aaa m_pBlockMessage=0x%08x.\n", GetConnectID(), m_pBlockMessage));
			App_PacketParseLoader::instance()->GetPacketParseInfo()->Make_Send_Packet(GetConnectID(), pBuffPacket->GetData(), pBuffPacket->GetPacketLen(), m_pBlockMessage, u2CommandID);
			//����MakePacket�Ѿ��������ݳ��ȣ����������ﲻ��׷��
		}
		else
		{
			u4SendPacketSize = (uint32)pBuffPacket->GetPacketLen();

			if(u4SendPacketSize >= m_u4SendMaxBuffSize)
			{
				OUR_DEBUG((LM_DEBUG,"[CConnectHandler::SendMessage](%d) u4SendPacketSize is more than(%d)(%d).\n", GetConnectID(), u4SendPacketSize, m_u4SendMaxBuffSize));
				if(blDelete == true)
				{
					//ɾ���������ݰ� 
					App_BuffPacketManager::instance()->Delete(pBuffPacket);
				}
				return false;
			}

			//OUR_DEBUG((LM_DEBUG,"[CConnectHandler::SendMessage] Connectid=[%d] aaa m_pBlockMessage=0x%08x.\n", GetConnectID(), m_pBlockMessage));
			memcpy_safe((char* )pBuffPacket->GetData(), pBuffPacket->GetPacketLen(), m_pBlockMessage->wr_ptr(), pBuffPacket->GetPacketLen());
			m_pBlockMessage->wr_ptr(pBuffPacket->GetPacketLen());
		}

		//���֮ǰ�л������ݣ���ͻ�������һ����
		u4PacketSize = m_pBlockMessage->length();

		//����϶������0
		if(m_pBlockMessage->length() > 0)
		{
			//��Ϊ���첽���ͣ����͵�����ָ�벻���������ͷţ�������Ҫ�����ﴴ��һ���µķ������ݿ飬�����ݿ���
			pMbData = App_MessageBlockManager::instance()->Create((uint32)m_pBlockMessage->length());
			if(NULL == pMbData)
			{
				OUR_DEBUG((LM_DEBUG,"[CConnectHandler::SendMessage] Connectid=[%d] pMbData is NULL.\n", GetConnectID()));
				if(blDelete == true)
				{
					//ɾ���������ݰ� 
					App_BuffPacketManager::instance()->Delete(pBuffPacket);
				}
				return false;
			}

			//OUR_DEBUG((LM_DEBUG,"[CConnectHandler::SendMessage] Connectid=[%d] m_pBlockMessage=0x%08x.\n", GetConnectID(), m_pBlockMessage));
			memcpy_safe(m_pBlockMessage->rd_ptr(), m_pBlockMessage->length(), pMbData->wr_ptr(), m_pBlockMessage->length());
			pMbData->wr_ptr(m_pBlockMessage->length());
			//������ɣ�����ջ������ݣ�ʹ�����
			m_pBlockMessage->reset();
		}

		if(blDelete == true)
		{
			//ɾ���������ݰ� 
			App_BuffPacketManager::instance()->Delete(pBuffPacket);
		}

		bool blRet = PutSendPacket(pMbData);
		if(true == blRet)
		{
			//��¼�ɹ������ֽ�
			m_u4SuccessSendSize += u4SendSuc;
		}

		return blRet;
	}
}

bool CConnectHandler::PutSendPacket(ACE_Message_Block* pMbData)
{
	if(NULL == pMbData)
	{
		return false;
	}

	//�����DEBUG״̬����¼��ǰ���Ͱ��Ķ���������
	if(App_MainConfig::instance()->GetDebug() == DEBUG_ON || m_blIsLog == true)
	{
		char szDebugData[MAX_BUFF_1024] = {'\0'};
		char szLog[10]  = {'\0'};
		int  nDebugSize = 0; 
		bool blblMore   = false;

		if(pMbData->length() >= MAX_BUFF_200)
		{
			nDebugSize = MAX_BUFF_200;
			blblMore   = true;
		}
		else
		{
			nDebugSize = (int)pMbData->length();
		}

		char* pData = pMbData->rd_ptr();
		for(int i = 0; i < nDebugSize; i++)
		{
			sprintf_safe(szLog, 10, "0x%02X ", (unsigned char)pData[i]);
			sprintf_safe(szDebugData + 5*i, MAX_BUFF_1024 - 5*i, "%s", szLog);
		}

		if(blblMore == true)
		{
			AppLogManager::instance()->WriteLog(LOG_SYSTEM_DEBUG_CLIENTSEND, "[(%s)%s:%d]%s.(���ݰ�����ֻ��¼ǰ200�ֽ�)", m_szConnectName, m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), szDebugData);
		}
		else
		{
			AppLogManager::instance()->WriteLog(LOG_SYSTEM_DEBUG_CLIENTSEND, "[(%s)%s:%d]%s.", m_szConnectName, m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), szDebugData);
		}
	}

	//ͳ�Ʒ�������
	ACE_Date_Time dtNow;
	if(false == m_TimeConnectInfo.SendCheck((uint8)dtNow.minute(), 1, pMbData->length()))
	{
		//�������޶��ķ�ֵ����Ҫ�ر����ӣ�����¼��־
		AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECTABNORMAL, 
			App_MainConfig::instance()->GetClientDataAlert()->m_u4MailID, 
			(char* )"Alert",
			"[TCP]IP=%s,Prot=%d,SendPacketCount=%d, SendSize=%d.", 
			m_addrRemote.get_host_addr(), 
			m_addrRemote.get_port_number(), 
			m_TimeConnectInfo.m_u4SendPacketCount, 
			m_TimeConnectInfo.m_u4SendSize);

		//���÷��ʱ��
		App_ForbiddenIP::instance()->AddTempIP(m_addrRemote.get_host_addr(), App_MainConfig::instance()->GetIPAlert()->m_u4IPTimeout);
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::PutSendPacket] ConnectID = %d, Send Data is more than limit.\n", GetConnectID()));

		App_MessageBlockManager::instance()->Close(pMbData);

		return false;
	}

	//���ͳ�ʱʱ������
	ACE_Time_Value	nowait(0, m_u4SendThresHold*MAX_BUFF_1000);

	if(NULL == pMbData)
	{
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::PutSendPacket] ConnectID = %d, get_handle() == ACE_INVALID_HANDLE.\n", GetConnectID()));
		return false;
	}

	if(get_handle() == ACE_INVALID_HANDLE)
	{
		OUR_DEBUG((LM_ERROR, "[CConnectHandler::PutSendPacket] ConnectID = %d, get_handle() == ACE_INVALID_HANDLE.\n", GetConnectID()));
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectHandler::PutSendPacket] ConnectID = %d, get_handle() == ACE_INVALID_HANDLE.\n", GetConnectID());
		App_MessageBlockManager::instance()->Close(pMbData);
		return false;
	}

	//��������
	int nSendPacketLen = (int)pMbData->length();
	int nIsSendSize    = 0;

	//ѭ�����ͣ�ֱ�����ݷ�����ɡ�
	while(true)
	{
		if(nSendPacketLen <= 0)
		{
			OUR_DEBUG((LM_ERROR, "[CConnectHandler::PutSendPacket] ConnectID = %d, nCurrSendSize error is %d.\n", GetConnectID(), nSendPacketLen));
			App_MessageBlockManager::instance()->Close(pMbData);
			return false;
		}

		int nDataLen = this->peer().send(pMbData->rd_ptr(), nSendPacketLen - nIsSendSize, &nowait);	

		if(nDataLen <= 0)
		{
			int nErrno = errno;
			OUR_DEBUG((LM_ERROR, "[CConnectHandler::PutSendPacket] ConnectID = %d, error = %d.\n", GetConnectID(), nErrno));

			AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "WriteError [%s:%d] nErrno = %d  result.bytes_transferred() = %d, ",
				                                m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), nErrno, 
				                                nIsSendSize);
			m_atvOutput      = ACE_OS::gettimeofday();

			//������Ϣ�ص�
			App_MakePacket::instance()->PutSendErrorMessage(GetConnectID(), pMbData, m_atvOutput);
			//App_MessageBlockManager::instance()->Close(pMbData);
			
			//�رյ�ǰ����
			App_ConnectManager::instance()->CloseUnLock(GetConnectID());

			return false;
		}
		else if(nDataLen >= nSendPacketLen - nIsSendSize)   //�����ݰ�ȫ��������ϣ���ա�
		{
			//OUR_DEBUG((LM_ERROR, "[CConnectHandler::handle_output] ConnectID = %d, send (%d) OK.\n", GetConnectID(), msg_queue()->is_empty()));
			m_u4AllSendCount    += 1;
			m_u4AllSendSize     += (uint32)pMbData->length();
			m_atvOutput         = ACE_OS::gettimeofday();
			App_MessageBlockManager::instance()->Close(pMbData);

			//������Ҫ����Ҫ�ر�����
			/*
			if(CLIENT_CLOSE_SENDOK == m_emStatus)
			{
				if(m_u4ReadSendSize - m_u4SuccessSendSize == 0)
				{
					ServerClose(CLIENT_CLOSE_IMMEDIATLY);
				}
			}
			*/

			return true;
		}
		else
		{
			pMbData->rd_ptr(nDataLen);
			nIsSendSize      += nDataLen;
			m_atvOutput      = ACE_OS::gettimeofday();
			continue;
		}
	}

	return true;
}

bool CConnectHandler::CheckMessage()
{	
	if(m_pPacketParse->GetMessageBody() == NULL)
	{
		m_u4AllRecvSize += (uint32)m_pPacketParse->GetMessageHead()->length();
	}
	else
	{
		m_u4AllRecvSize += (uint32)m_pPacketParse->GetMessageHead()->length() + (uint32)m_pPacketParse->GetMessageBody()->length();
	}
	//OUR_DEBUG((LM_ERROR, "[CConnectHandler::CheckMessage]head length=%d.\n", m_pPacketParse->GetMessageHead()->length()));
	//OUR_DEBUG((LM_ERROR, "[CConnectHandler::CheckMessage]body length=%d.\n", m_pPacketParse->GetMessageBody()->length()));

	m_u4AllRecvCount++;

	//�����Ҫͳ����Ϣ
	//App_IPAccount::instance()->UpdateIP((string)m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4AllRecvSize, m_u4AllSendSize);

	ACE_Time_Value tvCheck = ACE_OS::gettimeofday();
	ACE_Date_Time dtNow(tvCheck);
	if(false == m_TimeConnectInfo.RecvCheck((uint8)dtNow.minute(), 1, m_u4AllRecvSize))
	{
		//�������޶��ķ�ֵ����Ҫ�ر����ӣ�����¼��־
		AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECTABNORMAL, 
			App_MainConfig::instance()->GetClientDataAlert()->m_u4MailID,
			(char* )"Alert", 
			"[TCP]IP=%s,Prot=%d,PacketCount=%d, RecvSize=%d.", 
			m_addrRemote.get_host_addr(), 
			m_addrRemote.get_port_number(), 
			m_TimeConnectInfo.m_u4RecvPacketCount, 
			m_TimeConnectInfo.m_u4RecvSize);

		App_PacketParsePool::instance()->Delete(m_pPacketParse);
		m_pPacketParse = NULL;

		//���÷��ʱ��
		App_ForbiddenIP::instance()->AddTempIP(m_addrRemote.get_host_addr(), App_MainConfig::instance()->GetIPAlert()->m_u4IPTimeout);
		OUR_DEBUG((LM_ERROR, "[CConnectHandle::CheckMessage] ConnectID = %d, PutMessageBlock is check invalid.\n", GetConnectID()));
		return false;
	}

	//��֯����
	_MakePacket objMakePacket;

	objMakePacket.m_pPacketParse      = m_pPacketParse;
	if(ACE_OS::strcmp("INADDR_ANY", m_szLocalIP) == 0)
	{
		objMakePacket.m_AddrListen.set(m_u4LocalPort);
	}
	else
	{
		objMakePacket.m_AddrListen.set(m_u4LocalPort, m_szLocalIP);
	}

	//������Buff������Ϣ����
	if(false == App_MakePacket::instance()->PutMessageBlock(GetConnectID(), PACKET_PARSE, &objMakePacket, tvCheck))
	{
		App_PacketParsePool::instance()->Delete(m_pPacketParse);
		m_pPacketParse = NULL;

		OUR_DEBUG((LM_ERROR, "[CConnectHandle::CheckMessage] ConnectID = %d, PutMessageBlock is error.\n", GetConnectID()));
	}


	App_PacketParsePool::instance()->Delete(m_pPacketParse);

	return true;
}

_ClientConnectInfo CConnectHandler::GetClientInfo()
{
	_ClientConnectInfo ClientConnectInfo;

	ClientConnectInfo.m_blValid             = true;
	ClientConnectInfo.m_u4ConnectID         = GetConnectID();
	ClientConnectInfo.m_addrRemote          = m_addrRemote;
	ClientConnectInfo.m_u4RecvCount         = m_u4AllRecvCount;
	ClientConnectInfo.m_u4SendCount         = m_u4AllSendCount;
	ClientConnectInfo.m_u4AllRecvSize       = m_u4AllSendSize;
	ClientConnectInfo.m_u4AllSendSize       = m_u4AllSendSize;
	ClientConnectInfo.m_u4BeginTime         = (uint32)m_atvConnect.sec();
	ClientConnectInfo.m_u4AliveTime         = (uint32)(ACE_OS::gettimeofday().sec() -  m_atvConnect.sec());
	ClientConnectInfo.m_u4RecvQueueCount    = m_u4RecvQueueCount;
	ClientConnectInfo.m_u8RecvQueueTimeCost = m_u8RecvQueueTimeCost;
	ClientConnectInfo.m_u8SendQueueTimeCost = m_u8SendQueueTimeCost;

	return ClientConnectInfo;
}

_ClientIPInfo  CConnectHandler::GetClientIPInfo()
{
	_ClientIPInfo ClientIPInfo;
	sprintf_safe(ClientIPInfo.m_szClientIP, MAX_BUFF_50, "%s", m_addrRemote.get_host_addr());
	ClientIPInfo.m_nPort = (int)m_addrRemote.get_port_number();
	return ClientIPInfo;
}

_ClientIPInfo  CConnectHandler::GetLocalIPInfo()
{
	_ClientIPInfo ClientIPInfo;
	sprintf_safe(ClientIPInfo.m_szClientIP, MAX_BUFF_50, "%s", m_szLocalIP);
	ClientIPInfo.m_nPort = (int)m_u4LocalPort;
	return ClientIPInfo;
}

bool CConnectHandler::CheckSendMask(uint32 u4PacketLen)
{
	m_u4ReadSendSize += u4PacketLen;
	//OUR_DEBUG ((LM_ERROR, "[CConnectHandler::CheckSendMask]GetSendDataMask = %d, m_u4ReadSendSize=%d, m_u4SuccessSendSize=%d.\n", App_MainConfig::instance()->GetSendDataMask(), m_u4ReadSendSize, m_u4SuccessSendSize));
	if(m_u4ReadSendSize - m_u4SuccessSendSize >= App_MainConfig::instance()->GetSendDataMask())
	{
		OUR_DEBUG ((LM_ERROR, "[CConnectHandler::CheckSendMask]ConnectID = %d, SingleConnectMaxSendBuffer is more than(%d)!\n", GetConnectID(), m_u4ReadSendSize - m_u4SuccessSendSize));
		AppLogManager::instance()->WriteLog(LOG_SYSTEM_SENDQUEUEERROR, "]Connection from [%s:%d], SingleConnectMaxSendBuffer is more than(%d)!.", m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), m_u4ReadSendSize - m_u4SuccessSendSize);
		return false;
	}
	else
	{
		return true;
	}
}

void CConnectHandler::ClearPacketParse()
{
	if(NULL != m_pPacketParse)
	{
		if(m_pPacketParse->GetMessageHead() != NULL)
		{
			App_MessageBlockManager::instance()->Close(m_pPacketParse->GetMessageHead());
		}
	
		if(m_pPacketParse->GetMessageBody() != NULL)
		{
			App_MessageBlockManager::instance()->Close(m_pPacketParse->GetMessageBody());
		}
	
		if(m_pCurrMessage != NULL && m_pPacketParse->GetMessageBody() != m_pCurrMessage && m_pPacketParse->GetMessageHead() != m_pCurrMessage)
		{
			App_MessageBlockManager::instance()->Close(m_pCurrMessage);
		}
		m_pCurrMessage = NULL;
	
		App_PacketParsePool::instance()->Delete(m_pPacketParse);
		m_pPacketParse = NULL;
	}
}

void CConnectHandler::SetConnectName(const char* pName)
{
	sprintf_safe(m_szConnectName, MAX_BUFF_100, "%s", pName);
}

void CConnectHandler::SetIsLog(bool blIsLog)
{
	m_blIsLog = blIsLog;
}

char* CConnectHandler::GetConnectName()
{
	return m_szConnectName;
}

bool CConnectHandler::GetIsLog()
{
	return m_blIsLog;
}

void CConnectHandler::SetLocalIPInfo(const char* pLocalIP, uint32 u4LocalPort)
{
	sprintf_safe(m_szLocalIP, MAX_BUFF_50, "%s", pLocalIP);
	m_u4LocalPort = u4LocalPort;
}

void CConnectHandler::SetSendCacheManager(CSendCacheManager* pSendCacheManager)
{
	if(NULL != pSendCacheManager)
	{
		m_pBlockMessage = pSendCacheManager->GetCacheData(GetConnectID());
	}
}

//***************************************************************************
CConnectManager::CConnectManager(void)
{
	m_u4TimeCheckID      = 0;
	m_szError[0]         = '\0';

	m_pTCTimeSendCheck   = NULL;
	m_tvCheckConnect     = ACE_OS::gettimeofday();
	m_blRun              = false;

	m_u4TimeConnect      = 0;
	m_u4TimeDisConnect   = 0;

	//��ʼ�����Ͷ����
	m_SendMessagePool.Init();
}

CConnectManager::~CConnectManager(void)
{
	OUR_DEBUG((LM_INFO, "[CConnectManager::~CConnectManager].\n"));
	//m_blRun = false;
	//CloseAll();
}

void CConnectManager::CloseAll()
{
	//ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
	msg_queue()->deactivate();

	KillTimer();

	vector<CConnectHandler*> vecCloseConnectHandler;
	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end(); b++)
	{
		mapConnectManager::iterator itr = b;
		CConnectHandler* pConnectHandler = (CConnectHandler* )itr->second;
		if(pConnectHandler != NULL)
		{
			vecCloseConnectHandler.push_back(pConnectHandler);
			m_u4TimeDisConnect++;

			//��������ͳ�ƹ���
			//App_ConnectAccount::instance()->AddDisConnect();
		}
	}

	//��ʼ�ر���������
	for(int i = 0; i < (int)vecCloseConnectHandler.size(); i++)
	{
		CConnectHandler* pConnectHandler = vecCloseConnectHandler[i];
		pConnectHandler->Close();
	}

	m_mapConnectManager.clear();
}

bool CConnectManager::Close(uint32 u4ConnectID)
{
	//OUR_DEBUG((LM_ERROR, "[CConnectManager::Close]ConnectID=%d Begin.\n", u4ConnectID));
	ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);
	//OUR_DEBUG((LM_ERROR, "[CConnectManager::Close]ConnectID=%d Begin 1.\n", u4ConnectID));
	mapConnectManager::iterator f = m_mapConnectManager.find(u4ConnectID);

	if(f != m_mapConnectManager.end())
	{
		CConnectHandler* pConnectHandler = (CConnectHandler* )f->second;
		if(pConnectHandler != NULL)
		{
			//���շ��ͻ���
			m_SendCacheManager.FreeCacheData(u4ConnectID);			
			m_u4TimeDisConnect++;
		}
		m_mapConnectManager.erase(f);

		//��������ͳ�ƹ���
		App_ConnectAccount::instance()->AddDisConnect();

		//OUR_DEBUG((LM_ERROR, "[CConnectManager::Close]ConnectID=%d End.\n", u4ConnectID));
		return true;
	}
	else
	{
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectManager::Close] ConnectID[%d] is not find.", u4ConnectID);
		return true;
	}
}

bool CConnectManager::CloseUnLock(uint32 u4ConnectID)
{
	mapConnectManager::iterator f = m_mapConnectManager.find(u4ConnectID);

	if(f != m_mapConnectManager.end())
	{
		CConnectHandler* pConnectHandler = (CConnectHandler* )f->second;
		if(pConnectHandler != NULL)
		{
			//���շ��ͻ���
			m_SendCacheManager.FreeCacheData(u4ConnectID);			
			m_u4TimeDisConnect++;
		}
		m_mapConnectManager.erase(f);

		//��������ͳ�ƹ���
		App_ConnectAccount::instance()->AddDisConnect();

		OUR_DEBUG((LM_ERROR, "[CConnectManager::CloseUnLock]ConnectID=%d End.\n", u4ConnectID));

		pConnectHandler->ServerClose(CLIENT_CLOSE_IMMEDIATLY);

		return true;
	}
	else
	{
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectManager::Close] ConnectID[%d] is not find.", u4ConnectID);
		return true;
	}
}

bool CConnectManager::CloseConnect(uint32 u4ConnectID, EM_Client_Close_status emStatus)
{
	//OUR_DEBUG((LM_ERROR, "[CConnectManager::CloseConnect]ConnectID=%d Begin.\n", u4ConnectID));
	ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);
	//OUR_DEBUG((LM_ERROR, "[CConnectManager::CloseConnect]ConnectID=%d Begin 1.\n", u4ConnectID));
	mapConnectManager::iterator f = m_mapConnectManager.find(u4ConnectID);
		
	if(emStatus != CLIENT_CLOSE_IMMEDIATLY)
	{
		return false;
	}

	if(f != m_mapConnectManager.end())
	{
		CConnectHandler* pConnectHandler = (CConnectHandler* )f->second;
		if(pConnectHandler != NULL)
		{
			//���շ��ͻ���
			m_SendCacheManager.FreeCacheData(u4ConnectID);
			pConnectHandler->ServerClose(emStatus);
			m_u4TimeDisConnect++;

			//��������ͳ�ƹ���
			App_ConnectAccount::instance()->AddDisConnect();
		}
		m_mapConnectManager.erase(f);
		//OUR_DEBUG((LM_ERROR, "[CConnectManager::CloseConnect]ConnectID=%d End.\n", u4ConnectID));
		return true;
	}
	else
	{
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectManager::CloseConnect] ConnectID[%d] is not find.", u4ConnectID);
		return true;
	}
}

bool CConnectManager::AddConnect(uint32 u4ConnectID, CConnectHandler* pConnectHandler)
{
	//OUR_DEBUG((LM_ERROR, "[CConnectManager::AddConnect]ConnectID=%d Begin.\n", u4ConnectID));
	ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);
	//OUR_DEBUG((LM_ERROR, "[CConnectManager::AddConnect]ConnectID=%d Begin 1.\n", u4ConnectID));

	if(pConnectHandler == NULL)
	{
		OUR_DEBUG((LM_ERROR, "[CConnectManager::AddConnect]ConnectID=%d, pConnectHandler is NULL.\n", u4ConnectID));
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectManager::AddConnect] pConnectHandler is NULL.");
		return false;		
	}

	mapConnectManager::iterator f = m_mapConnectManager.find(u4ConnectID);
	if(f != m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_ERROR, "[CConnectManager::AddConnect]ConnectID=%d is find.\n", u4ConnectID));
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectManager::AddConnect] ConnectID[%d] is exist.", u4ConnectID);
		return false;
	}

	pConnectHandler->SetConnectID(u4ConnectID);
	pConnectHandler->SetSendCacheManager(&m_SendCacheManager);
	//����map
	m_mapConnectManager.insert(mapConnectManager::value_type(u4ConnectID, pConnectHandler));
	m_u4TimeConnect++;
	
	//OUR_DEBUG((LM_ERROR, "[CConnectManager::AddConnect]ConnectID=%d.\n", u4ConnectID));

	//��������ͳ�ƹ���
	App_ConnectAccount::instance()->AddConnect();

	//OUR_DEBUG((LM_ERROR, "[CConnectManager::AddConnect]ConnectID=%d End.\n", u4ConnectID));
	return true;
}

bool CConnectManager::SendMessage(uint32 u4ConnectID, IBuffPacket* pBuffPacket, uint16 u2CommandID, bool blSendState, uint8 u1SendType, ACE_Time_Value& tvSendBegin, bool blDelete)
{
	//��Ϊ�Ƕ��е��ã��������ﲻ��Ҫ�����ˡ�
	if(NULL == pBuffPacket)
	{
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectManager::SendMessage] ConnectID[%d] pBuffPacket is NULL.", u4ConnectID);
		return false;
	}

	m_ThreadWriteLock.acquire();
	mapConnectManager::iterator f = m_mapConnectManager.find(u4ConnectID);

	if(f != m_mapConnectManager.end())
	{
		CConnectHandler* pConnectHandler = (CConnectHandler* )f->second;
		m_ThreadWriteLock.release();

		if(NULL != pConnectHandler)
		{
			uint32 u4PacketSize = 0;
            //OUR_DEBUG((LM_ERROR, "[CConnectManager::SendMessage]ConnectID=%d Begin 1 pConnectHandler.\n", u4ConnectID));
			pConnectHandler->SendMessage(u4ConnectID, pBuffPacket, blSendState, u1SendType, u4PacketSize, blDelete);
            //OUR_DEBUG((LM_ERROR, "[CConnectManager::SendMessage]ConnectID=%d End 1 pConnectHandler.\n", u4ConnectID));
			//��¼��Ϣ��������ʱ��
			ACE_Time_Value tvInterval = ACE_OS::gettimeofday() - tvSendBegin;
			uint32 u4SendCost = (uint32)(tvInterval.msec());
			pConnectHandler->SetSendQueueTimeCost(u4SendCost);
			m_CommandAccount.SaveCommandData(u2CommandID, (uint64)u4SendCost, PACKET_TCP, u4PacketSize, u4PacketSize, COMMAND_TYPE_OUT);
			return true;
		}
		else
		{
			sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectManager::SendMessage] ConnectID[%d] is not find.", u4ConnectID);
			App_BuffPacketManager::instance()->Delete(pBuffPacket);
			return true;
		}
	}
	else
	{
		m_ThreadWriteLock.release();
		sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectManager::SendMessage] ConnectID[%d] is not find.", u4ConnectID);
		App_BuffPacketManager::instance()->Delete(pBuffPacket);
		return true;
	}

	//OUR_DEBUG((LM_ERROR, "[CConnectManager::SendMessage]ConnectID=%d End.\n", u4ConnectID));
	return true;
}

bool CConnectManager::PostMessage(uint32 u4ConnectID, IBuffPacket* pBuffPacket, uint8 u1SendType, uint16 u2CommandID, bool blSendState, bool blDelete)
{
	//OUR_DEBUG((LM_INFO, "[CConnectManager::PostMessage]Begin.\n"));
	ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);
	//OUR_DEBUG((LM_INFO, "[CConnectManager::PostMessage]Begin 1.\n"));

	//���뷢�Ͷ���
	_SendMessage* pSendMessage = m_SendMessagePool.Create();

	ACE_Message_Block* mb = pSendMessage->GetQueueMessage();

	//�ж��Ƿ�ﵽ�˷��ͷ�ֵ������ﵽ�ˣ���ֱ�ӶϿ����ӡ�
	mapConnectManager::iterator f = m_mapConnectManager.find(u4ConnectID);

	if(f != m_mapConnectManager.end())
	{
		CConnectHandler* pConnectHandler = (CConnectHandler* )f->second;
		if(NULL != pConnectHandler)
		{
			bool blState = pConnectHandler->CheckSendMask(pBuffPacket->GetPacketLen());
			if(false == blState)
			{
				//�����˷�ֵ����ر�����
				if(blDelete == true)
				{
					App_BuffPacketManager::instance()->Delete(pBuffPacket);
				}

				pConnectHandler->ServerClose(CLIENT_CLOSE_IMMEDIATLY);
				m_mapConnectManager.erase(f);

				return false;
			}
		}
	}
	else
	{
		//�������ѹ���Ͳ����ڣ��򲻽������ݶ��У�ֱ�Ӷ����������ݣ�������ʧ�ܡ�
		OUR_DEBUG((LM_ERROR,"[CMessageService::PutMessage] u4ConnectID(%d) is not exist.\n", u4ConnectID));
		if(blDelete == true)
		{
			App_BuffPacketManager::instance()->Delete(pBuffPacket);
		}
		return false;
	}

	if(NULL != mb)
	{
		if(NULL == pSendMessage)
		{
			OUR_DEBUG((LM_ERROR,"[CConnectManager::PutMessage] new _SendMessage is error.\n"));
			return false;
		}

		pSendMessage->m_u4ConnectID = u4ConnectID;
		pSendMessage->m_pBuffPacket = pBuffPacket;
		pSendMessage->m_nEvents     = u1SendType;
		pSendMessage->m_u2CommandID = u2CommandID;
		pSendMessage->m_blDelete    = blDelete;
		pSendMessage->m_blSendState = blSendState;
		pSendMessage->m_tvSend      = ACE_OS::gettimeofday();

		//�ж϶����Ƿ����Ѿ����
		int nQueueCount = (int)msg_queue()->message_count();
		if(nQueueCount >= (int)MAX_MSG_THREADQUEUE)
		{
			OUR_DEBUG((LM_ERROR,"[CConnectManager::PutMessage] Queue is Full nQueueCount = [%d].\n", nQueueCount));
			if(blDelete == true)
			{
				App_BuffPacketManager::instance()->Delete(pBuffPacket);
			}
			m_SendMessagePool.Delete(pSendMessage);
			return false;
		}

		ACE_Time_Value xtime = ACE_OS::gettimeofday() + ACE_Time_Value(0, m_u4SendQueuePutTime);
		if(this->putq(mb, &xtime) == -1)
		{
			OUR_DEBUG((LM_ERROR,"[CConnectManager::PutMessage] Queue putq  error nQueueCount = [%d] errno = [%d].\n", nQueueCount, errno));
			if(blDelete == true)
			{
				App_BuffPacketManager::instance()->Delete(pBuffPacket);
			}
			m_SendMessagePool.Delete(pSendMessage);
			return false;
		}
	}
	else
	{
		OUR_DEBUG((LM_ERROR,"[CMessageService::PutMessage] mb new error.\n"));
		if(blDelete == true)
		{
			App_BuffPacketManager::instance()->Delete(pBuffPacket);
		}
		return false;
	}

	//OUR_DEBUG((LM_INFO, "[CConnectManager::PostMessage]End.\n"));
	return true;
}

const char* CConnectManager::GetError()
{
	return m_szError;
}

bool CConnectManager::StartTimer()
{
	//���������߳�
	if(0 != open())
	{
		OUR_DEBUG((LM_ERROR, "[CConnectManager::StartTimer]Open() is error.\n"));
		return false;
	}

	//���ⶨʱ���ظ�����
	KillTimer();
	OUR_DEBUG((LM_ERROR, "CConnectManager::StartTimer()-->begin....\n"));
	//�õ��ڶ���Reactor
	ACE_Reactor* pReactor = App_ReactorManager::instance()->GetAce_Reactor(REACTOR_POSTDEFINE);
	if(NULL == pReactor)
	{
		OUR_DEBUG((LM_ERROR, "CConnectManager::StartTimer() -->GetAce_Reactor(REACTOR_POSTDEFINE) is NULL.\n"));
		return false;
	}

	m_pTCTimeSendCheck = new _TimerCheckID();
	if(NULL == m_pTCTimeSendCheck)
	{
		OUR_DEBUG((LM_ERROR, "CConnectManager::StartTimer() m_pTCTimeSendCheck is NULL.\n"));
		return false;
	}

	m_pTCTimeSendCheck->m_u2TimerCheckID = PARM_CONNECTHANDLE_CHECK;
	m_u4TimeCheckID = pReactor->schedule_timer(this, (const void *)m_pTCTimeSendCheck, ACE_Time_Value(App_MainConfig::instance()->GetCheckAliveTime(), 0), ACE_Time_Value(App_MainConfig::instance()->GetCheckAliveTime(), 0));
	if(0 == m_u4TimeCheckID)
	{
		OUR_DEBUG((LM_ERROR, "CConnectManager::StartTimer()--> Start thread m_u4TimeCheckID error.\n"));
		return false;
	}
	else
	{
		OUR_DEBUG((LM_ERROR, "CConnectManager::StartTimer()--> Start thread time OK.\n"));
		return true;
	}
}

bool CConnectManager::KillTimer()
{
	if(m_u4TimeCheckID > 0)
	{
		App_ReactorManager::instance()->GetAce_Reactor(REACTOR_POSTDEFINE)->cancel_timer(m_u4TimeCheckID);
		m_u4TimeCheckID = 0;
	}

	SAFE_DELETE(m_pTCTimeSendCheck);
	return true;
}

int CConnectManager::handle_timeout(const ACE_Time_Value &tv, const void *arg)
{ 
	//ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);
	ACE_Time_Value tvNow = ACE_OS::gettimeofday();
	vector<CConnectHandler*> vecDelConnectHandler;

	if(arg == NULL)
	{
		OUR_DEBUG((LM_ERROR, "[CConnectManager::handle_timeout]arg is not NULL, tv = %d.\n", tv.sec()));
	}

	_TimerCheckID* pTimerCheckID = (_TimerCheckID*)arg;
	if(NULL == pTimerCheckID)
	{
		return 0;
	}

	//��ʱ��ⷢ�ͣ����ｫ��ʱ��¼������Ϣ�������У�����һ����ʱ��
	if(pTimerCheckID->m_u2TimerCheckID == PARM_CONNECTHANDLE_CHECK)
	{
		if(m_mapConnectManager.size() > 0)
		{
			m_ThreadWriteLock.acquire();
			for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end(); b++)
			{
				CConnectHandler* pConnectHandler = (CConnectHandler* )b->second;
				if(pConnectHandler != NULL)
				{
					if(false == pConnectHandler->CheckAlive(tvNow))
					{
						vecDelConnectHandler.push_back(pConnectHandler);
					}
				}
			}
			m_ThreadWriteLock.release();
		}

		for(uint32 i= 0; i < vecDelConnectHandler.size(); i++)
		{
			//�ر����ù�ϵ
			Close(vecDelConnectHandler[i]->GetConnectID());
			
			//�������ر�����
			vecDelConnectHandler[i]->ServerClose(CLIENT_CLOSE_IMMEDIATLY, PACKET_CHEK_TIMEOUT);
		}

		//�ж��Ƿ�Ӧ�ü�¼������־
		ACE_Time_Value tvNow = ACE_OS::gettimeofday();
		ACE_Time_Value tvInterval(tvNow - m_tvCheckConnect);
		if(tvInterval.sec() >= MAX_MSG_HANDLETIME)
		{
			AppLogManager::instance()->WriteLog(LOG_SYSTEM_CONNECT, "[CConnectManager]CurrConnectCount = %d,TimeInterval=%d, TimeConnect=%d, TimeDisConnect=%d.", 
				GetCount(), MAX_MSG_HANDLETIME, m_u4TimeConnect, m_u4TimeDisConnect);

			//���õ�λʱ���������ͶϿ�������
			m_u4TimeConnect    = 0;
			m_u4TimeDisConnect = 0;
			m_tvCheckConnect   = tvNow;
		}

		//������������Ƿ�Խ��ط�ֵ
		if(App_MainConfig::instance()->GetConnectAlert()->m_u4ConnectAlert > 0)
		{
			if(GetCount() > (int)App_MainConfig::instance()->GetConnectAlert()->m_u4ConnectAlert)
			{
				AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECT,
					App_MainConfig::instance()->GetConnectAlert()->m_u4MailID,
					(char* )"Alert",
					"[CProConnectManager]active ConnectCount is more than limit(%d > %d).", 
					GetCount(),
					App_MainConfig::instance()->GetConnectAlert()->m_u4ConnectAlert);
			}
		}

		//��ⵥλʱ���������Ƿ�Խ��ֵ
		int nCheckRet = App_ConnectAccount::instance()->CheckConnectCount();
		if(nCheckRet == 1)
		{
			AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECT,
				App_MainConfig::instance()->GetConnectAlert()->m_u4MailID,
				(char* )"Alert",
				"[CProConnectManager]CheckConnectCount is more than limit(%d > %d).", 
				App_ConnectAccount::instance()->GetCurrConnect(),
				App_ConnectAccount::instance()->GetConnectMax());
		}
		else if(nCheckRet == 2)
		{
			AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECT,
				App_MainConfig::instance()->GetConnectAlert()->m_u4MailID,
				(char* )"Alert",
				"[CProConnectManager]CheckConnectCount is little than limit(%d < %d).", 
				App_ConnectAccount::instance()->GetCurrConnect(),
				App_ConnectAccount::instance()->Get4ConnectMin());
		}

		//��ⵥλʱ�����ӶϿ����Ƿ�Խ��ֵ
		nCheckRet = App_ConnectAccount::instance()->CheckDisConnectCount();
		if(nCheckRet == 1)
		{
			AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECT,
				App_MainConfig::instance()->GetConnectAlert()->m_u4MailID,
				(char* )"Alert",
				"[CProConnectManager]CheckDisConnectCount is more than limit(%d > %d).", 
				App_ConnectAccount::instance()->GetCurrConnect(),
				App_ConnectAccount::instance()->GetDisConnectMax());
		}
		else if(nCheckRet == 2)
		{
			AppLogManager::instance()->WriteToMail(LOG_SYSTEM_CONNECT,
				App_MainConfig::instance()->GetConnectAlert()->m_u4MailID,
				(char* )"Alert",
				"[CProConnectManager]CheckDisConnectCount is little than limit(%d < %d).", 
				App_ConnectAccount::instance()->GetCurrConnect(),
				App_ConnectAccount::instance()->GetDisConnectMin());
		}

	}

	return 0;
}

int CConnectManager::GetCount()
{
	//ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);
	return (int)m_mapConnectManager.size(); 
}

int CConnectManager::open(void* args)
{
	if(args != NULL)
	{
		OUR_DEBUG((LM_INFO,"[CConnectManager::open]args is not NULL.\n"));
	}

	m_blRun = true;
	msg_queue()->high_water_mark(MAX_MSG_MASK);
	msg_queue()->low_water_mark(MAX_MSG_MASK);

	OUR_DEBUG((LM_INFO,"[CConnectManager::open] m_u4HighMask = [%d] m_u4LowMask = [%d]\n", MAX_MSG_MASK, MAX_MSG_MASK));
	if(activate(THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED | THR_SUSPENDED, MAX_MSG_THREADCOUNT) == -1)
	{
		OUR_DEBUG((LM_ERROR, "[CConnectManager::open] activate error ThreadCount = [%d].", MAX_MSG_THREADCOUNT));
		m_blRun = false;
		return -1;
	}

	m_u4SendQueuePutTime = App_MainConfig::instance()->GetSendQueuePutTime() * 1000;

	resume();

	return 0;
}

int CConnectManager::svc (void)
{
	ACE_Message_Block* mb = NULL;
	ACE_Time_Value xtime;

	while(IsRun())
	{
		mb = NULL;
		if(getq(mb, 0) == -1)
		{
			OUR_DEBUG((LM_ERROR,"[CConnectManager::svc] get error errno = [%d].\n", errno));
			m_blRun = false;
			break;
		}
		if (mb == NULL)
		{
			continue;
		}
		_SendMessage* msg = *((_SendMessage**)mb->base());
		if (! msg)
		{
			continue;
		}

		//����������
		SendMessage(msg->m_u4ConnectID, msg->m_pBuffPacket, msg->m_u2CommandID, msg->m_blSendState, msg->m_nEvents, msg->m_tvSend);

		m_SendMessagePool.Delete(msg);

	}

	OUR_DEBUG((LM_INFO,"[CConnectManager::svc] svc finish!\n"));
	return 0;
}

bool CConnectManager::IsRun()
{
	return m_blRun;
}

void CConnectManager::CloseQueue()
{
	this->msg_queue()->deactivate();
}

int CConnectManager::close(u_long)
{
	m_blRun = false;
	OUR_DEBUG((LM_INFO,"[CConnectManager::close] close().\n"));
	return 0;
}

void CConnectManager::SetRecvQueueTimeCost(uint32 u4ConnectID, uint32 u4TimeCost)
{
	//ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);
	mapConnectManager::iterator f = m_mapConnectManager.find(u4ConnectID);

	if(f != m_mapConnectManager.end())
	{
		CConnectHandler* pConnectHandler = (CConnectHandler* )f->second;
		if(NULL != pConnectHandler)
		{
			pConnectHandler->SetRecvQueueTimeCost(u4TimeCost);
		}
	}
}

void CConnectManager::GetConnectInfo(vecClientConnectInfo& VecClientConnectInfo)
{
	//ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);

	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end(); b++)
	{
		CConnectHandler* pConnectHandler = (CConnectHandler* )b->second;
		if(pConnectHandler != NULL)
		{
			VecClientConnectInfo.push_back(pConnectHandler->GetClientInfo());
		}
	}
}

_ClientIPInfo CConnectManager::GetClientIPInfo(uint32 u4ConnectID)
{
	//ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);
	mapConnectManager::iterator f = m_mapConnectManager.find(u4ConnectID);

	if(f != m_mapConnectManager.end())
	{
		CConnectHandler* pConnectHandler = (CConnectHandler* )f->second;
		if(NULL != pConnectHandler)
		{
			return pConnectHandler->GetClientIPInfo();
		}
		else
		{
			_ClientIPInfo ClientIPInfo;
			return ClientIPInfo;
		}
	}
	else
	{
		_ClientIPInfo ClientIPInfo;
		return ClientIPInfo;
	}
}

_ClientIPInfo CConnectManager::GetLocalIPInfo(uint32 u4ConnectID)
{
	//ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);
	mapConnectManager::iterator f = m_mapConnectManager.find(u4ConnectID);

	if(f != m_mapConnectManager.end())
	{
		CConnectHandler* pConnectHandler = (CConnectHandler* )f->second;
		if(NULL != pConnectHandler)
		{
			return pConnectHandler->GetLocalIPInfo();
		}
		else
		{
			_ClientIPInfo ClientIPInfo;
			return ClientIPInfo;
		}
	}
	else
	{
		_ClientIPInfo ClientIPInfo;
		return ClientIPInfo;
	}
}

bool CConnectManager::PostMessageAll(IBuffPacket* pBuffPacket, uint8 u1SendType, uint16 u2CommandID, bool blSendState, bool blDelete)
{
	m_ThreadWriteLock.acquire();
	vecConnectManager objvecConnectManager;
	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end(); b++)
	{
		objvecConnectManager.push_back((uint32)b->first);
	}
	m_ThreadWriteLock.release();

	uint32 u4ConnectID = 0;
	for(uint32 i = 0; i < (uint32)objvecConnectManager.size(); i++)
	{
		IBuffPacket* pCurrBuffPacket = App_BuffPacketManager::instance()->Create();
		if(NULL == pCurrBuffPacket)
		{
			OUR_DEBUG((LM_INFO, "[CConnectManager::PostMessage]pCurrBuffPacket is NULL.\n"));
			if(blDelete == true)
			{
				App_BuffPacketManager::instance()->Delete(pBuffPacket);
			}
			return false;
		}

		pCurrBuffPacket->WriteStream(pBuffPacket->GetData(), pBuffPacket->GetPacketLen());

		u4ConnectID = objvecConnectManager[i];

		//�ж��Ƿ�ﵽ�˷��ͷ�ֵ������ﵽ�ˣ���ֱ�ӶϿ����ӡ�
		mapConnectManager::iterator f = m_mapConnectManager.find(u4ConnectID);

		if(f != m_mapConnectManager.end())
		{
			CConnectHandler* pConnectHandler = (CConnectHandler* )f->second;
			if(NULL != pConnectHandler)
			{
				bool blState = pConnectHandler->CheckSendMask(pBuffPacket->GetPacketLen());
				if(false == blState)
				{
					//�����˷�ֵ����ر�����
					if(blDelete == true)
					{
						App_BuffPacketManager::instance()->Delete(pBuffPacket);
					}

					pConnectHandler->ServerClose(CLIENT_CLOSE_IMMEDIATLY);
					m_mapConnectManager.erase(f);

					continue;
				}
			}
		}
		else
		{
			//�������ѹ���Ͳ����ڣ��򲻽������ݶ��У�ֱ�Ӷ����������ݣ�������ʧ�ܡ�
			OUR_DEBUG((LM_ERROR,"[CMessageService::PutMessage] u4ConnectID(%d) is not exist.\n", u4ConnectID));
			if(blDelete == true)
			{
				App_BuffPacketManager::instance()->Delete(pBuffPacket);
			}
			continue;
		}

		//���뷢�Ͷ���
		_SendMessage* pSendMessage = m_SendMessagePool.Create();

		ACE_Message_Block* mb = pSendMessage->GetQueueMessage();

		if(NULL != mb)
		{
			if(NULL == pSendMessage)
			{
				OUR_DEBUG((LM_ERROR,"[CConnectManager::PutMessage] new _SendMessage is error.\n"));
				if(blDelete == true)
				{
					App_BuffPacketManager::instance()->Delete(pBuffPacket);
				}
				return false;
			}

			pSendMessage->m_u4ConnectID = u4ConnectID;
			pSendMessage->m_pBuffPacket = pCurrBuffPacket;
			pSendMessage->m_nEvents     = u1SendType;
			pSendMessage->m_u2CommandID = u2CommandID;
			pSendMessage->m_blDelete    = blDelete;
			pSendMessage->m_blSendState = blSendState;
			pSendMessage->m_tvSend      = ACE_OS::gettimeofday();

			//�ж϶����Ƿ����Ѿ����
			int nQueueCount = (int)msg_queue()->message_count();
			if(nQueueCount >= (int)MAX_MSG_THREADQUEUE)
			{
				OUR_DEBUG((LM_ERROR,"[CConnectManager::PutMessage] Queue is Full nQueueCount = [%d].\n", nQueueCount));
				if(blDelete == true)
				{
					App_BuffPacketManager::instance()->Delete(pBuffPacket);
				}
				m_SendMessagePool.Delete(pSendMessage);
				return false;
			}

			ACE_Time_Value xtime = ACE_OS::gettimeofday() + ACE_Time_Value(0, MAX_MSG_PUTTIMEOUT);
			if(this->putq(mb, &xtime) == -1)
			{
				OUR_DEBUG((LM_ERROR,"[CConnectManager::PutMessage] Queue putq  error nQueueCount = [%d] errno = [%d].\n", nQueueCount, errno));
				if(blDelete == true)
				{
					App_BuffPacketManager::instance()->Delete(pBuffPacket);
				}
				m_SendMessagePool.Delete(pSendMessage);
				return false;
			}
		}
		else
		{
			OUR_DEBUG((LM_ERROR,"[CMessageService::PutMessage] mb new error.\n"));
			if(blDelete == true)
			{
				App_BuffPacketManager::instance()->Delete(pBuffPacket);
			}
			return false;
		}
	}

	return true;
}

bool CConnectManager::SetConnectName(uint32 u4ConnectID, const char* pName)
{
	mapConnectManager::iterator f = m_mapConnectManager.find(u4ConnectID);

	if(f != m_mapConnectManager.end())
	{
		CConnectHandler* pConnectHandler = (CConnectHandler* )f->second;
		if(NULL != pConnectHandler)
		{
			pConnectHandler->SetConnectName(pName);
			return true;
		}
		else
		{
			return false;
		}
	}	
	else
	{
		return false;
	}
}

bool CConnectManager::SetIsLog(uint32 u4ConnectID, bool blIsLog)
{
	mapConnectManager::iterator f = m_mapConnectManager.find(u4ConnectID);

	if(f != m_mapConnectManager.end())
	{
		CConnectHandler* pConnectHandler = (CConnectHandler* )f->second;
		if(NULL != pConnectHandler)
		{
			pConnectHandler->SetIsLog(blIsLog);
			return true;
		}
		else
		{
			return false;
		}
	}	
	else
	{
		return false;
	}	
}

void CConnectManager::GetClientNameInfo(const char* pName, vecClientNameInfo& objClientNameInfo)
{
	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end(); b++)
	{
		CConnectHandler* pConnectHandler = (CConnectHandler* )b->second;
		if(NULL != pConnectHandler && ACE_OS::strcmp(pConnectHandler->GetConnectName(), pName) == 0)
		{
			_ClientNameInfo ClientNameInfo;
			ClientNameInfo.m_nConnectID = (int)pConnectHandler->GetConnectID();
			sprintf_safe(ClientNameInfo.m_szName, MAX_BUFF_100, "%s", pConnectHandler->GetConnectName());
			sprintf_safe(ClientNameInfo.m_szClientIP, MAX_BUFF_50, "%s", pConnectHandler->GetClientIPInfo().m_szClientIP);
			ClientNameInfo.m_nPort =  pConnectHandler->GetClientIPInfo().m_nPort;
			if(pConnectHandler->GetIsLog() == true)
			{
				ClientNameInfo.m_nLog = 1;
			}
			else
			{
				ClientNameInfo.m_nLog = 0;
			}

			objClientNameInfo.push_back(ClientNameInfo);
		}
	}
}

_CommandData* CConnectManager::GetCommandData( uint16 u2CommandID )
{
	return m_CommandAccount.GetCommandData(u2CommandID);
}

void CConnectManager::Init( uint16 u2Index )
{
	//�����̳߳�ʼ��ͳ��ģ�������
	char szName[MAX_BUFF_50] = {'\0'};
	sprintf_safe(szName, MAX_BUFF_50, "�����߳�(%d)", u2Index);
	m_CommandAccount.InitName(szName);

	//��ʼ��ͳ��ģ�鹦��
	m_CommandAccount.Init(App_MainConfig::instance()->GetCommandAccount(), 
		App_MainConfig::instance()->GetCommandFlow(), 
		App_MainConfig::instance()->GetPacketTimeOut());
			
	//��ʼ�����ͻ���
	m_SendCacheManager.Init(App_MainConfig::instance()->GetBlockCount(), App_MainConfig::instance()->GetBlockSize());
}

uint32 CConnectManager::GetCommandFlowAccount()
{
	return m_CommandAccount.GetFlowOut();
}

EM_Client_Connect_status CConnectManager::GetConnectState(uint32 u4ConnectID)
{
	mapConnectManager::iterator f = m_mapConnectManager.find(u4ConnectID);

	if(f != m_mapConnectManager.end())
	{
		return CLIENT_CONNECT_EXIST;
	}
	else
	{
		return CLIENT_CONNECT_NO_EXIST;
	}
}

//*********************************************************************************

CConnectHandlerPool::CConnectHandlerPool(void)
{
	//ConnectID��������1��ʼ
	m_u4CurrMaxCount = 1;
}

CConnectHandlerPool::~CConnectHandlerPool(void)
{
	OUR_DEBUG((LM_INFO, "[CConnectHandlerPool::~CConnectHandlerPool].\n"));
	Close();
	OUR_DEBUG((LM_INFO, "[CConnectHandlerPool::~CConnectHandlerPool]End.\n"));
}

void CConnectHandlerPool::Init(int nObjcetCount)
{
	Close();

	for(int i = 0; i < nObjcetCount; i++)
	{
		CConnectHandler* pPacket = new CConnectHandler();
		if(NULL != pPacket)
		{
			//��ӵ�Free map����
			mapHandle::iterator f = m_mapMessageFree.find(pPacket);
			if(f == m_mapMessageFree.end())
			{
				pPacket->Init(m_u4CurrMaxCount);
				m_mapMessageFree.insert(mapHandle::value_type(pPacket, pPacket));
				m_u4CurrMaxCount++;
			}
		}
	}
}

void CConnectHandlerPool::Close()
{
	//���������Ѵ��ڵ�ָ��
	for(mapHandle::iterator itorFreeB = m_mapMessageFree.begin(); itorFreeB != m_mapMessageFree.end(); itorFreeB++)
	{
		CConnectHandler* pObject = (CConnectHandler* )itorFreeB->second;
		SAFE_DELETE(pObject);
	}

	for(mapHandle::iterator itorUsedB = m_mapMessageUsed.begin(); itorUsedB != m_mapMessageUsed.end(); itorUsedB++)
	{
		CConnectHandler* pPacket = (CConnectHandler* )itorUsedB->second;
		SAFE_DELETE(pPacket);
	}

	m_u4CurrMaxCount = 0;
	m_mapMessageFree.clear();
	m_mapMessageUsed.clear();
}

int CConnectHandlerPool::GetUsedCount()
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);

	return (int)m_mapMessageUsed.size();
}

int CConnectHandlerPool::GetFreeCount()
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);

	return (int)m_mapMessageFree.size();
}

CConnectHandler* CConnectHandlerPool::Create()
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);

	//���free�����Ѿ�û���ˣ�����ӵ�free���С�
	if(m_mapMessageFree.size() <= 0)
	{
		CConnectHandler* pPacket = new CConnectHandler();

		if(pPacket != NULL)
		{
			//��ӵ�Free map����
			mapHandle::iterator f = m_mapMessageFree.find(pPacket);
			if(f == m_mapMessageFree.end())
			{
				pPacket->Init(m_u4CurrMaxCount);
				m_mapMessageFree.insert(mapHandle::value_type(pPacket, pPacket));
				m_u4CurrMaxCount++;
			}
		}
		else
		{
			return NULL;
		}
	}

	//��free�����ó�һ��,���뵽used����
	mapHandle::iterator itorFreeB = m_mapMessageFree.begin();
	CConnectHandler* pPacket = (CConnectHandler* )itorFreeB->second;
	m_mapMessageFree.erase(itorFreeB);
	//��ӵ�used map����
	mapHandle::iterator f = m_mapMessageUsed.find(pPacket);
	if(f == m_mapMessageUsed.end())
	{
		m_mapMessageUsed.insert(mapHandle::value_type(pPacket, pPacket));
	}

	return (CConnectHandler* )pPacket;
}

bool CConnectHandlerPool::Delete(CConnectHandler* pObject)
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);

	if(NULL == pObject)
	{
		return false;
	}

	mapHandle::iterator f = m_mapMessageUsed.find(pObject);
	if(f != m_mapMessageUsed.end())
	{
		m_mapMessageUsed.erase(f);

		//��ӵ�Free map����
		mapHandle::iterator f = m_mapMessageFree.find(pObject);
		if(f == m_mapMessageFree.end())
		{
			m_mapMessageFree.insert(mapHandle::value_type(pObject, pObject));
		}
	}

	return true;
}

//==============================================================
CConnectManagerGroup::CConnectManagerGroup()
{
	m_u4CurrMaxCount     = 0;
	m_u2ThreadQueueCount = SENDQUEUECOUNT;
}

CConnectManagerGroup::~CConnectManagerGroup()
{
	OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::~CConnectManagerGroup].\n"));

	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end();b++)
	{
		CConnectManager* pConnectManager = (CConnectManager* )b->second;
		SAFE_DELETE(pConnectManager);
	}

	m_mapConnectManager.clear();
}

void CConnectManagerGroup::Init(uint16 u2SendQueueCount)
{
	for(int i = 0; i < u2SendQueueCount; i++)
	{
		CConnectManager* pConnectManager = new CConnectManager();
		if(NULL != pConnectManager)	
		{
			//��ʼ��ͳ����
			pConnectManager->Init((uint16)i);
			//����map
			m_mapConnectManager.insert(mapConnectManager::value_type(i, pConnectManager));
			OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::Init]Creat %d SendQueue OK.\n", i));
		}
	}

	m_u2ThreadQueueCount = (uint16)m_mapConnectManager.size();
}

uint32 CConnectManagerGroup::GetGroupIndex()
{
	//�������ӻ�����У��������������㷨��
	ACE_Guard<ACE_Recursive_Thread_Mutex> WGuard(m_ThreadWriteLock);
	m_u4CurrMaxCount++;
	return m_u4CurrMaxCount;
}

bool CConnectManagerGroup::AddConnect(CConnectHandler* pConnectHandler)
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> WGrard(m_ThreadWriteLock);

	uint32 u4ConnectID = GetGroupIndex();

	//�ж����е���һ���߳�������ȥ
	uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

	mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
	if(f == m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::AddConnect]Out of range Queue ID.\n"));
		return false;
	}

	CConnectManager* pConnectManager = (CConnectManager* )f->second;
	if(NULL == pConnectManager)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::AddConnect]No find send Queue object.\n"));
		return false;		
	}

	//OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::Init]u4ConnectID=%d, u2ThreadIndex=%d.\n", u4ConnectID, u2ThreadIndex));

	return pConnectManager->AddConnect(u4ConnectID, pConnectHandler);
}

bool CConnectManagerGroup::PostMessage(uint32 u4ConnectID, IBuffPacket* pBuffPacket, uint8 u1SendType, uint16 u2CommandID, bool blSendState, bool blDelete)
{
	//�ж����е���һ���߳�������ȥ
	uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

	mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
	if(f == m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessage]Out of range Queue ID.\n"));
		return false;
	}

	CConnectManager* pConnectManager = (CConnectManager* )f->second;
	if(NULL == pConnectManager)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessage]No find send Queue object.\n"));
		return false;		
	}

	//OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessage]u4ConnectID=%d, u2ThreadIndex=%d.\n", u4ConnectID, u2ThreadIndex));

	return pConnectManager->PostMessage(u4ConnectID, pBuffPacket, u1SendType, u2CommandID, blSendState, blDelete);
}

bool CConnectManagerGroup::PostMessage( uint32 u4ConnectID, const char* pData, uint32 nDataLen, uint8 u1SendType, uint16 u2CommandID, bool blSendState, bool blDelete)
{
	//�ж����е���һ���߳�������ȥ
	uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

	mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
	if(f == m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessage]Out of range Queue ID.\n"));

		if(blDelete == true)
		{
			SAFE_DELETE_ARRAY(pData);
		}

		return false;
	}

	CConnectManager* pConnectManager = (CConnectManager* )f->second;
	if(NULL == pConnectManager)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessage]No find send Queue object.\n"));

		if(blDelete == true)
		{
			SAFE_DELETE_ARRAY(pData);
		}

		return false;		
	}

	//OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessage]u4ConnectID=%d, u2ThreadIndex=%d.\n", u4ConnectID, u2ThreadIndex));
	IBuffPacket* pBuffPacket = App_BuffPacketManager::instance()->Create();
	if(NULL != pBuffPacket)
	{
		pBuffPacket->WriteStream(pData, nDataLen);

		if(blDelete == true)
		{
			SAFE_DELETE_ARRAY(pData);
		}

		return pConnectManager->PostMessage(u4ConnectID, pBuffPacket, u1SendType, u2CommandID, blSendState, true);
	} 
	else
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessage]pBuffPacket is NULL.\n"));

		if(blDelete == true)
		{
			SAFE_DELETE_ARRAY(pData);
		}

		return false;
	}
}

bool CConnectManagerGroup::PostMessage( vector<uint32> vecConnectID, IBuffPacket* pBuffPacket, uint8 u1SendType, uint16 u2CommandID, bool blSendState, bool blDelete)
{
	uint32 u4ConnectID = 0;
	for(uint32 i = 0; i < (uint32)vecConnectID.size(); i++)
	{
		//�ж����е���һ���߳�������ȥ
		u4ConnectID = vecConnectID[i];
		uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

		mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
		if(f == m_mapConnectManager.end())
		{
			//OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessage]Out of range Queue ID.\n"));
			continue;
		}

		CConnectManager* pConnectManager = (CConnectManager* )f->second;
		if(NULL == pConnectManager)
		{
			//OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessage]No find send Queue object.\n"));
			continue;		
		}
		
		//Ϊÿһ��Connect���÷��Ͷ������ݰ�
		IBuffPacket* pCurrBuffPacket = App_BuffPacketManager::instance()->Create();
		if(NULL == pCurrBuffPacket)
		{
			continue;
		}
		pCurrBuffPacket->WriteStream(pBuffPacket->GetData(), pBuffPacket->GetWriteLen());

		pConnectManager->PostMessage(u4ConnectID, pCurrBuffPacket, u1SendType, u2CommandID, blSendState, true);
	}

	if(true == blDelete)
	{
		App_BuffPacketManager::instance()->Delete(pBuffPacket);
	}

	return true;
}

bool CConnectManagerGroup::PostMessage( vector<uint32> vecConnectID, const char* pData, uint32 nDataLen, uint8 u1SendType, uint16 u2CommandID, bool blSendState, bool blDelete)
{
	uint32 u4ConnectID = 0;

	for(uint32 i = 0; i < (uint32)vecConnectID.size(); i++)
	{
		//�ж����е���һ���߳�������ȥ
		u4ConnectID = vecConnectID[i];
		uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

		mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
		if(f == m_mapConnectManager.end())
		{
			OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessage]Out of range Queue ID.\n"));
			continue;
		}

		CConnectManager* pConnectManager = (CConnectManager* )f->second;
		if(NULL == pConnectManager)
		{
			OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessage]No find send Queue object.\n"));
			continue;
		}

		//Ϊÿһ��Connect���÷��Ͷ������ݰ�
		IBuffPacket* pBuffPacket = App_BuffPacketManager::instance()->Create();
		if(NULL == pBuffPacket)
		{
			continue;
		}
		pBuffPacket->WriteStream(pData, nDataLen);

		pConnectManager->PostMessage(u4ConnectID, pBuffPacket, u1SendType, u2CommandID, blSendState, true);
	}

	if(true == blDelete)
	{
		SAFE_DELETE_ARRAY(pData);
	}

	return true;
}

bool CConnectManagerGroup::CloseConnect(uint32 u4ConnectID, EM_Client_Close_status emStatus)
{

	//�ж����е���һ���߳�������ȥ
	uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

	mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
	if(f == m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::CloseConnect]Out of range Queue ID.\n"));
		return false;
	}

	CConnectManager* pConnectManager = (CConnectManager* )f->second;
	if(NULL == pConnectManager)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::CloseConnect]No find send Queue object.\n"));
		return false;		
	}	

	return pConnectManager->CloseConnect(u4ConnectID, emStatus);
}

bool CConnectManagerGroup::CloseConnectByClient(uint32 u4ConnectID)
{
	//�ж����е���һ���߳�������ȥ
	uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

	mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
	if(f == m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::CloseConnect]Out of range Queue ID.\n"));
		return false;
	}

	CConnectManager* pConnectManager = (CConnectManager* )f->second;
	if(NULL == pConnectManager)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::CloseConnect]No find send Queue object.\n"));
		return false;		
	}	

	return pConnectManager->Close(u4ConnectID);
}

_ClientIPInfo CConnectManagerGroup::GetClientIPInfo(uint32 u4ConnectID)
{
	_ClientIPInfo objClientIPInfo;
	//�ж����е���һ���߳�������ȥ
	uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

	mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
	if(f == m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::GetClientIPInfo]Out of range Queue ID.\n"));
		return objClientIPInfo;
	}

	CConnectManager* pConnectManager = (CConnectManager* )f->second;
	if(NULL == pConnectManager)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::GetClientIPInfo]No find send Queue object.\n"));
		return objClientIPInfo;		
	}	

	return pConnectManager->GetClientIPInfo(u4ConnectID);	
}

_ClientIPInfo CConnectManagerGroup::GetLocalIPInfo(uint32 u4ConnectID)
{
	_ClientIPInfo objClientIPInfo;
	//�ж����е���һ���߳�������ȥ
	uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

	mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
	if(f == m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::GetLocalIPInfo]Out of range Queue ID.\n"));
		return objClientIPInfo;
	}

	CConnectManager* pConnectManager = (CConnectManager* )f->second;
	if(NULL == pConnectManager)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::GetLocalIPInfo]No find send Queue object.\n"));
		return objClientIPInfo;		
	}	

	return pConnectManager->GetLocalIPInfo(u4ConnectID);	
}


void CConnectManagerGroup::GetConnectInfo(vecClientConnectInfo& VecClientConnectInfo)
{
	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end(); b++)
	{
		CConnectManager* pConnectManager = (CConnectManager* )b->second;
		if(NULL != pConnectManager)
		{
			pConnectManager->GetConnectInfo(VecClientConnectInfo);
		}
	}
}

int CConnectManagerGroup::GetCount()
{
	uint32 u4Count = 0;
	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end(); b++)
	{
		CConnectManager* pConnectManager = (CConnectManager* )b->second;
		if(NULL != pConnectManager)
		{
			u4Count += pConnectManager->GetCount();
		}
	}

	return u4Count;
}

void CConnectManagerGroup::CloseAll()
{
	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end(); b++)
	{
		CConnectManager* pConnectManager = (CConnectManager* )b->second;
		if(NULL != pConnectManager)
		{
			pConnectManager->CloseAll();
		}
	}	
}

bool CConnectManagerGroup::StartTimer()
{
	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end();b++)
	{
		CConnectManager* pConnectManager = (CConnectManager* )b->second;
		if(NULL != pConnectManager)
		{
			pConnectManager->StartTimer();
		}
	}

	return true;	
}

bool CConnectManagerGroup::Close(uint32 u4ConnectID)
{
	//�ж����е���һ���߳�������ȥ
	uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

	mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
	if(f == m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::GetClientIPInfo]Out of range Queue ID.\n"));
		return false;
	}

	CConnectManager* pConnectManager = (CConnectManager* )f->second;
	if(NULL == pConnectManager)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::GetClientIPInfo]No find send Queue object.\n"));
		return false;		
	}	

	return pConnectManager->Close(u4ConnectID);
}

bool CConnectManagerGroup::CloseUnLock(uint32 u4ConnectID)
{
	//�ж����е���һ���߳�������ȥ
	uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

	mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
	if(f == m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::GetClientIPInfo]Out of range Queue ID.\n"));
		return false;
	}

	CConnectManager* pConnectManager = (CConnectManager* )f->second;
	if(NULL == pConnectManager)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::GetClientIPInfo]No find send Queue object.\n"));
		return false;		
	}	

	return pConnectManager->CloseUnLock(u4ConnectID);
}

const char* CConnectManagerGroup::GetError()
{
	return (char* )"";
}

void CConnectManagerGroup::SetRecvQueueTimeCost(uint32 u4ConnectID, uint32 u4TimeCost)
{
	//�ж����е���һ���߳�������ȥ
	uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

	mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
	if(f == m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::GetClientIPInfo]Out of range Queue ID.\n"));
		return;
	}

	CConnectManager* pConnectManager = (CConnectManager* )f->second;
	if(NULL == pConnectManager)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::GetClientIPInfo]No find send Queue object.\n"));
		return;		
	}		

	pConnectManager->SetRecvQueueTimeCost(u4ConnectID, u4TimeCost);
}

bool CConnectManagerGroup::PostMessageAll( IBuffPacket* pBuffPacket, uint8 u1SendType, uint16 u2CommandID, bool blSendState, bool blDelete)
{
	//ȫ��Ⱥ��
	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end(); b++)
	{
		CConnectManager* pConnectManager = (CConnectManager* )b->second;
		if(NULL == pConnectManager)
		{
			OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessage]No find send Queue object.\n"));
			continue;		
		}

		pConnectManager->PostMessageAll(pBuffPacket, u1SendType, u2CommandID, blSendState, false);
	}

	//�����˾�ɾ��
	if(true == blDelete)
	{
		App_BuffPacketManager::instance()->Delete(pBuffPacket);
	}

	return true;
}

bool CConnectManagerGroup::PostMessageAll( const char* pData, uint32 nDataLen, uint8 u1SendType, uint16 u2CommandID, bool blSendState, bool blDelete)
{
	IBuffPacket* pBuffPacket = App_BuffPacketManager::instance()->Create();
	if(NULL == pBuffPacket)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessageAll]pBuffPacket is NULL.\n"));

		if(blDelete == true)
		{
			SAFE_DELETE_ARRAY(pData);
		}

		return false;
	}
	else
	{
		pBuffPacket->WriteStream(pData, nDataLen);
	}

	//ȫ��Ⱥ��
	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end(); b++)
	{
		CConnectManager* pConnectManager = (CConnectManager* )b->second;
		if(NULL == pConnectManager)
		{
			OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::PostMessage]No find send Queue object.\n"));
			continue;	
		}

		pConnectManager->PostMessageAll(pBuffPacket, u1SendType, u2CommandID, blSendState, false);
	}

	App_BuffPacketManager::instance()->Delete(pBuffPacket);

	//�����˾�ɾ��
	if(true == blDelete)
	{
		SAFE_DELETE_ARRAY(pData);
	}

	return true;
}

bool CConnectManagerGroup::SetConnectName(uint32 u4ConnectID, const char* pName)
{
	//�ж����е���һ���߳�������ȥ
	uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

	mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
	if(f == m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::CloseConnect]Out of range Queue ID.\n"));
		return false;
	}

	CConnectManager* pConnectManager = (CConnectManager* )f->second;
	if(NULL == pConnectManager)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::CloseConnect]No find send Queue object.\n"));
		return false;		
	}	

	return pConnectManager->SetConnectName(u4ConnectID, pName);	
}

bool CConnectManagerGroup::SetIsLog(uint32 u4ConnectID, bool blIsLog)
{
	//�ж����е���һ���߳�������ȥ
	uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

	mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
	if(f == m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::CloseConnect]Out of range Queue ID.\n"));
		return false;
	}

	CConnectManager* pConnectManager = (CConnectManager* )f->second;
	if(NULL == pConnectManager)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::CloseConnect]No find send Queue object.\n"));
		return false;		
	}	

	return pConnectManager->SetIsLog(u4ConnectID, blIsLog);		
}

void CConnectManagerGroup::GetClientNameInfo(const char* pName, vecClientNameInfo& objClientNameInfo)
{
	objClientNameInfo.clear();
	//ȫ������
	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end(); b++)
	{
		CConnectManager* pConnectManager = (CConnectManager* )b->second;
		if(NULL != pConnectManager)
		{
			pConnectManager->GetClientNameInfo(pName, objClientNameInfo);
		}
	}	
}

void CConnectManagerGroup::GetCommandData( uint16 u2CommandID, _CommandData& objCommandData )
{
	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end(); b++)
	{
		CConnectManager* pConnectManager = (CConnectManager* )b->second;
		if(NULL != pConnectManager)
		{
			_CommandData* pCommandData = pConnectManager->GetCommandData(u2CommandID);
			if(pCommandData != NULL)
			{
				objCommandData += (*pCommandData);
			}
		}
	}
}

void CConnectManagerGroup::GetCommandFlowAccount(_CommandFlowAccount& objCommandFlowAccount)
{
	for(mapConnectManager::iterator b = m_mapConnectManager.begin(); b != m_mapConnectManager.end(); b++)
	{
		CConnectManager* pConnectManager = (CConnectManager* )b->second;
		if(NULL != pConnectManager)
		{
			uint32 u4FlowOut = pConnectManager->GetCommandFlowAccount();
			objCommandFlowAccount.m_u4FlowOut += u4FlowOut;
		}
	}
}

EM_Client_Connect_status CConnectManagerGroup::GetConnectState(uint32 u4ConnectID)
{
	//�ж����е���һ���߳�������ȥ
	uint16 u2ThreadIndex = u4ConnectID % m_u2ThreadQueueCount;

	mapConnectManager::iterator f = m_mapConnectManager.find(u2ThreadIndex);
	if(f == m_mapConnectManager.end())
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::CloseConnect]Out of range Queue ID.\n"));
		return CLIENT_CONNECT_NO_EXIST;
	}

	CConnectManager* pConnectManager = (CConnectManager* )f->second;
	if(NULL == pConnectManager)
	{
		OUR_DEBUG((LM_INFO, "[CConnectManagerGroup::CloseConnect]No find send Queue object.\n"));
		return CLIENT_CONNECT_NO_EXIST;		
	}

	return pConnectManager->GetConnectState(u4ConnectID);
}
