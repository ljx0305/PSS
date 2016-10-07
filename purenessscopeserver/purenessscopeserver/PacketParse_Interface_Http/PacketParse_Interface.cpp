//ʵ������PSS��PacketParse����
//�򻯽ӿڣ��Ժ����ķ�ʽʵ��
//add by freeeyes

#include "define.h"
#include "HttpInfo.h"
#include "IMessageBlockManager.h"

#include "ace/svc_export.h"

#ifdef WIN32
#ifdef PACKETPARSE_INTERFACE_EXPORTS
#define DECLDIR __declspec(dllexport)
#else
#define DECLDIR __declspec(dllimport)
#endif
#else
#define DECLDIR ACE_Svc_Export
#endif

//HTTP�õ����жϱ�ʶ
#define HTTP_HEAD_END    "\r\n\r\n"
#define HTTP_BODY_LENGTH "Content-Length: "

extern "C"
{ 
	DECLDIR bool Parse_Packet_Head_Info(uint32 u4ConnectID, ACE_Message_Block* pmbHead, IMessageBlockManager* pMessageBlockManager, _Head_Info* pHeadInfo);
	DECLDIR bool Parse_Packet_Body_Info(uint32 u4ConnectID, ACE_Message_Block* pmbbody, IMessageBlockManager* pMessageBlockManager, _Body_Info* pBodyInfo);
	DECLDIR uint8 Parse_Packet_Stream(uint32 u4ConnectID, ACE_Message_Block* pCurrMessage, IMessageBlockManager* pMessageBlockManager, _Packet_Info* pPacketInfo);
	DECLDIR bool Make_Send_Packet(uint32 u4ConnectID, const char* pData, uint32 u4Len, ACE_Message_Block* pMbData, uint16 u2CommandID);
	DECLDIR uint32 Make_Send_Packet_Length(uint32 u4ConnectID, uint32 u4DataLen, uint16 u2CommandID);
	DECLDIR bool Connect(uint32 u4ConnectID, _ClientIPInfo objClientIPInfo, _ClientIPInfo objLocalIPInfo);
	DECLDIR void DisConnect(uint32 u4ConnectID);

	uint8 HttpDispose(_HttpInfo* pHttpInfo, ACE_Message_Block* pCurrMessage, IMessageBlockManager* pMessageBlockManager, _Packet_Info* pPacketInfo);
	uint8 GetHttpBodyLen(char* pData, uint32 u4Len, uint32 u4HeadLen, uint32& u4BodyLen, _Packet_Info* pPacketInfo);

	//������ͷ����Ҫ���pHeadInfo���ݽṹ����ɺ����_Head_Info�����ݽṹ
	bool Parse_Packet_Head_Info(uint32 u4ConnectID, ACE_Message_Block* pmbHead, IMessageBlockManager* pMessageBlockManager, _Head_Info* pHeadInfo)
	{
		if(NULL == pHeadInfo || NULL == pMessageBlockManager)
		{
			return false;
		}

		//HTTP��ʹ�ô˲���

		return true;
	}

	//�������壬��Ҫ���pBodyInfo���ݽṹ����ɺ����_Body_Info�����ݽṹ
	bool Parse_Packet_Body_Info(uint32 u4ConnectID, ACE_Message_Block* pmbbody, IMessageBlockManager* pMessageBlockManager, _Body_Info* pBodyInfo)
	{
		if(NULL == pBodyInfo || NULL == pMessageBlockManager)
		{
			return false;
		}

		//HTTP��ʹ�ô˲���

		return true;
	}

	//��ģʽ�ݽ����������ɹ���Ҫ���_Packet_Info�ṹ
	uint8 Parse_Packet_Stream(uint32 u4ConnectID, ACE_Message_Block* pCurrMessage, IMessageBlockManager* pMessageBlockManager, _Packet_Info* pPacketInfo)
	{
		//������������Ĵ���
		_HttpInfo* pHttpInfo = App_HttpInfoManager::instance()->GetHttpInfo(u4ConnectID);
		if(NULL == pHttpInfo)
		{
			//û���ҵ����������Ϣ��˵���д��󣬶Ͽ�����
			return PACKET_GET_ERROR;
		}

		//�ж�http����ͷ�Ƿ�����
		return HttpDispose(pHttpInfo, pCurrMessage, pMessageBlockManager, pPacketInfo);
	}

	//ƴ�����ݷ��ذ������еķ������ݰ�����������
	bool Make_Send_Packet(uint32 u4ConnectID, const char* pData, uint32 u4Len, ACE_Message_Block* pMbData, uint16 u2CommandID)
	{
		if(pMbData == NULL && u2CommandID == 0)
		{
			return false;
		}

		//ƴװ���ݰ�
		memcpy_safe((char* )&u4Len, (uint32)sizeof(uint32), pMbData->wr_ptr(), (uint32)sizeof(uint32));
		pMbData->wr_ptr(sizeof(uint32));
		memcpy_safe((char* )pData, u4Len, pMbData->wr_ptr(), u4Len);
		pMbData->wr_ptr(u4Len);

		return true;
	}

	//�õ��������ݰ��ĳ���
	uint32 Make_Send_Packet_Length(uint32 u4ConnectID, uint32 u4DataLen, uint16 u2CommandID)
	{
		return u4DataLen + sizeof(uint32);
	}

	//�����ӵ�һ�ν�����ʱ�򣬷��صĽӿ��������Լ��Ĵ���
	bool Connect(uint32 u4ConnectID, _ClientIPInfo objClientIPInfo, _ClientIPInfo objLocalIPInfo)
	{
		//���ҵ�ǰ���Ӽ�¼�������޵�ǰ��¼
		return App_HttpInfoManager::instance()->Insert(u4ConnectID);
	}

	//�����ӶϿ���ʱ�򣬷������Լ��Ĵ���
	void DisConnect(uint32 u4ConnectID)
	{
		//�������������ӶϿ����߼�����
		App_HttpInfoManager::instance()->Delete(u4ConnectID);
	}

	//Http����Э�鲿��
	uint8 HttpDispose(_HttpInfo* pHttpInfo, ACE_Message_Block* pCurrMessage, IMessageBlockManager* pMessageBlockManager, _Packet_Info* pPacketInfo)
	{
		char* pData   = pCurrMessage->rd_ptr();     //�õ�������ݿ�����ֽ�
		uint32 u4Data = pCurrMessage->length();     //�õ�������ݿ�ĳ���

		//���յ�������ճ�뻺��ȴ����а�����
		if(pHttpInfo->m_u4DataLength + u4Data > MAX_DECRYPTLENGTH)
		{
			OUR_DEBUG((LM_ERROR, "[CPacketParse::HttpDispose]pWebSocketInfo is full.\n"));
			return PACKET_GET_ERROR;
		}

		//OUR_DEBUG((LM_ERROR, "[CPacketParse::HttpDispose]sizeof()=%d.\n", sizeof(pHttpInfo->m_szData)));
		//OUR_DEBUG((LM_ERROR, "[CPacketParse::HttpDispose]m_u4DataLength=%d.\n", pHttpInfo->m_u4DataLength));
		//OUR_DEBUG((LM_ERROR, "[CPacketParse::HttpDispose]u4Data=%d.\n", u4Data));
		
		ACE_OS::memcpy(&pHttpInfo->m_szData[pHttpInfo->m_u4DataLength], pData, u4Data);
		pHttpInfo->m_u4DataLength += u4Data;
		pHttpInfo->m_szData[pHttpInfo->m_u4DataLength] = '\0';

		//Ѱ��Http��ͷ
		char* pHttpHead = ACE_OS::strstr(pHttpInfo->m_szData, HTTP_HEAD_END);
		if(NULL == pHttpHead)
		{
			return PACKET_GET_NO_ENOUGTH;
		}

		uint32 u4HttpHeadLen = pHttpHead - pHttpInfo->m_szData - 4;
		//OUR_DEBUG((LM_ERROR, "[CPacketParse::HttpDispose]u4HttpHeadLen=%d.\n", u4HttpHeadLen));

		//�ҵ��������İ�ͷ
		pPacketInfo->m_pmbHead = pMessageBlockManager->Create(u4HttpHeadLen);
		if(NULL == pPacketInfo->m_pmbHead)
		{
			OUR_DEBUG((LM_ERROR, "[CPacketParse::HttpDispose]m_pmbHead is NULL.\n"));
			return PACKET_GET_ERROR;
		}

		memcpy(pPacketInfo->m_pmbHead->wr_ptr(), (char*)pHttpInfo->m_szData, u4HttpHeadLen);
		pPacketInfo->m_pmbHead->wr_ptr(u4HttpHeadLen);

		//����������(HTTP�̶�������)
		pPacketInfo->m_u2PacketCommandID = 0xea01;
		pPacketInfo->m_u4HeadSrcLen      = u4HttpHeadLen;
		pPacketInfo->m_u4HeadCurrLen     = u4HttpHeadLen;

		//�鿴��û�а���
		uint32 u4HttpBodyLength = 0;
		uint8 u1Ret = GetHttpBodyLen(pHttpInfo->m_szData, pHttpInfo->m_u4DataLength, u4HttpHeadLen, u4HttpBodyLength, pPacketInfo);
		if(u1Ret != PACKET_GET_ENOUGTH)
		{
			return u1Ret;
		}

		//OUR_DEBUG((LM_ERROR, "[CPacketParse::HttpDispose]u4HttpBodyLength=%d.\n", u4HttpBodyLength));
		if(u4HttpBodyLength == 0)
		{
			//��ð���
			pPacketInfo->m_pmbBody = pMessageBlockManager->Create(sizeof(uint32));
			if(NULL == pPacketInfo->m_pmbBody)
			{
				OUR_DEBUG((LM_ERROR, "[CPacketParse::WebSocketDisposeHandIn]m_pmbBody is NULL.\n"));
				return PACKET_GET_ERROR;
			}

			memcpy(pPacketInfo->m_pmbBody->wr_ptr(), (char*)&u4HttpHeadLen, sizeof(uint32));
			pPacketInfo->m_pmbBody->wr_ptr(sizeof(uint32));

			pPacketInfo->m_u4BodySrcLen  = (uint32)sizeof(uint32);
			pPacketInfo->m_u4BodyCurrLen = (uint32)sizeof(uint32);
		}
		else
		{
			//�а��壬��������
			pPacketInfo->m_pmbBody = pMessageBlockManager->Create(u4HttpBodyLength);
			if(NULL == pPacketInfo->m_pmbBody)
			{
				OUR_DEBUG((LM_ERROR, "[CPacketParse::HttpDispose]m_pmbBody is NULL.\n"));
				return PACKET_GET_ERROR;
			}

			memcpy(pPacketInfo->m_pmbBody->wr_ptr(), (char*)pHttpHead, u4HttpBodyLength);
			pPacketInfo->m_pmbBody->wr_ptr(u4HttpBodyLength);

			pPacketInfo->m_u4BodySrcLen  = (uint32)u4HttpBodyLength;
			pPacketInfo->m_u4BodyCurrLen = (uint32)u4HttpBodyLength;
		}

		//����������ݴӳ����Ƴ�
		pCurrMessage->rd_ptr(u4Data);
		
		pHttpInfo->m_u4DataLength = 0;
		return (uint8)PACKET_GET_ENOUGTH;
	}

	uint8 GetHttpBodyLen(char* pData, uint32 u4Len, uint32 u4HeadLen, uint32& u4BodyLen, _Packet_Info* pPacketInfo)
	{
		char szBodyLen[10] = {'\0'};
		int nNameLen = ACE_OS::strlen(HTTP_BODY_LENGTH);

		//OUR_DEBUG((LM_ERROR, "[CPacketParse::GetHttpBodyLen]nNameLen=%d.\n", nNameLen));
		//OUR_DEBUG((LM_ERROR, "[CPacketParse::GetHttpBodyLen]pData=%s.\n", pData));

		//����������Http����
		char* pLength = ACE_OS::strstr(pData, HTTP_BODY_LENGTH);
		if(NULL != pLength)
		{
			uint8 u1LengthLen = 0;
			//�����˰����ֶ�,�����������ֶ�
			for(int i = nNameLen; i < nNameLen + 9; i++)
			{
				if(pLength[i] == '\r')
				{
					break;
				}
				u1LengthLen++;
			}

			//OUR_DEBUG((LM_ERROR, "[CPacketParse::GetHttpBodyLen]u1LengthLen=%d.\n", u1LengthLen));
			ACE_OS::memcpy(szBodyLen, &pLength[nNameLen], u1LengthLen);

			u4BodyLen = ACE_OS::atoi(szBodyLen);
			//OUR_DEBUG((LM_ERROR, "[CPacketParse::GetHttpBodyLen]u4BodyLen=%d.\n", u4BodyLen));
			if(u4BodyLen == 0)
			{
				return PACKET_GET_ERROR;
			}

			//������ܵ��ֽڱ�http��������ݳ��ȶ̣�˵��û������
			if(u4BodyLen > u4Len - u4HeadLen)
			{
				return PACKET_GET_NO_ENOUGTH;
			}
		}
		else
		{
			//�Ҳ����������Ͱ�����������������
			u4BodyLen = u4Len - u4HeadLen;
		}

		return PACKET_GET_ENOUGTH;
	}
}
