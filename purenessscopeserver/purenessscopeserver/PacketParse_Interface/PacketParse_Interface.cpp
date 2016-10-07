//ʵ������PSS��PacketParse����
//�򻯽ӿڣ��Ժ����ķ�ʽʵ��
//add by freeeyes

#include "define.h"
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

extern "C"
{
	DECLDIR bool Parse_Packet_Head_Info(uint32 u4ConnectID, ACE_Message_Block* pmbHead, IMessageBlockManager* pMessageBlockManager, _Head_Info* pHeadInfo);
	DECLDIR bool Parse_Packet_Body_Info(uint32 u4ConnectID, ACE_Message_Block* pmbbody, IMessageBlockManager* pMessageBlockManager, _Body_Info* pBodyInfo);
	DECLDIR uint8 Parse_Packet_Stream(uint32 u4ConnectID, ACE_Message_Block* pCurrMessage, IMessageBlockManager* pMessageBlockManager, _Packet_Info* pPacketInfo);
	DECLDIR bool Make_Send_Packet(uint32 u4ConnectID, const char* pData, uint32 u4Len, ACE_Message_Block* pMbData, uint16 u2CommandID);
	DECLDIR uint32 Make_Send_Packet_Length(uint32 u4ConnectID, uint32 u4DataLen, uint16 u2CommandID);
	DECLDIR bool Connect(uint32 u4ConnectID, _ClientIPInfo objClientIPInfo, _ClientIPInfo objLocalIPInfo);
	DECLDIR void DisConnect(uint32 u4ConnectID);

	//������ͷ����Ҫ���pHeadInfo���ݽṹ����ɺ����_Head_Info�����ݽṹ
	bool Parse_Packet_Head_Info(uint32 u4ConnectID, ACE_Message_Block* pmbHead, IMessageBlockManager* pMessageBlockManager, _Head_Info* pHeadInfo)
	{
		if(NULL == pHeadInfo || NULL == pMessageBlockManager)
		{
			return false;
		}

		//������������Ĵ���
		char* pData  = (char* )pmbHead->rd_ptr();
		uint32 u4Len = pmbHead->length();
		uint32 u4Pos = 0;

		uint32 u2Version     = 0;           //Э��汾��
		uint32 u2CmdID       = 0;           //CommandID 
		uint32 u4BodyLen     = 0;           //���峤��  
		char   szSession[33] = {'\0'};      //Session�ַ��� 

		//������ͷ
		memcpy_safe((char* )&pData[u4Pos], (uint32)sizeof(uint16), (char* )&u2Version, (uint32)sizeof(uint16));
		u4Pos += sizeof(uint16);
		memcpy_safe((char* )&pData[u4Pos], (uint32)sizeof(uint16), (char* )&u2CmdID, (uint32)sizeof(uint16));
		u4Pos += sizeof(uint16);
		memcpy_safe((char* )&pData[u4Pos], (uint32)sizeof(uint32), (char* )&u4BodyLen, (uint32)sizeof(uint32));
		u4Pos += sizeof(uint32);
		memcpy_safe((char* )&pData[u4Pos], (uint32)(sizeof(char)*32), (char* )&szSession, (uint32)(sizeof(char)*32));
		u4Pos += sizeof(char)*32;

		OUR_DEBUG((LM_INFO,"[CPacketParse::SetPacketHead]m_u2Version=%d,m_u2CmdID=%d,m_u4BodyLen=%d.\n",
			u2Version,
			u2CmdID,
			u4BodyLen));

		//��䷵�ظ���ܵ����ݰ�ͷ��Ϣ
		pHeadInfo->m_u4HeadSrcLen      = u4Len;
		pHeadInfo->m_u4HeadCurrLen     = u4Len;
		pHeadInfo->m_u2PacketCommandID = u2CmdID;
		pHeadInfo->m_pmbHead           = pmbHead;
		pHeadInfo->m_u4BodySrcLen      = u4BodyLen;

		return true;
	}

	//�������壬��Ҫ���pBodyInfo���ݽṹ����ɺ����_Body_Info�����ݽṹ
	bool Parse_Packet_Body_Info(uint32 u4ConnectID, ACE_Message_Block* pmbbody, IMessageBlockManager* pMessageBlockManager, _Body_Info* pBodyInfo)
	{
		if(NULL == pBodyInfo || NULL == pMessageBlockManager)
		{
			return false;
		}

		//��䷵�ظ���ܵİ�����Ϣ
		pBodyInfo->m_u4BodySrcLen  = pmbbody->length();
		pBodyInfo->m_u4BodyCurrLen = pmbbody->length();
		pBodyInfo->m_pmbBody       = pmbbody;

		return true;
	}

	//��ģʽ�ݽ����������ɹ���Ҫ���_Packet_Info�ṹ
	uint8 Parse_Packet_Stream(uint32 u4ConnectID, ACE_Message_Block* pCurrMessage, IMessageBlockManager* pMessageBlockManager, _Packet_Info* pPacketInfo)
	{
		//������������Ĵ���

		return 0;
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
		return true;
	}

	//�����ӶϿ���ʱ�򣬷������Լ��Ĵ���
	void DisConnect(uint32 u4ConnectID)
	{

	}
}
