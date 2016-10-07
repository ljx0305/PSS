#ifndef _WRITE_PLUGIN_COMMAND_H
#define _WRITE_PLUGIN_COMMAND_H

#include "XmlOpeation.h"

void Gen_2_Cpp_Command_H(_Project_Info& objProjectInfo, vecClassInfo& objvecClassInfo)
{
	char szTemp[200]     = {'\0'};
	char szPathFile[200] = {'\0'};

	sprintf_safe(szPathFile, 200, "%s/BaseCommand.h", 
		objProjectInfo.m_szProjectName);

	//�������������ļ���
	FILE* pFile = fopen(szPathFile, "w");
	if(NULL == pFile)
	{
		return;
	}

	//������ת���ɴ�д
	char szHText[100] = {'\0'};
	To_Upper_String(objProjectInfo.m_szProjectName, szHText);
	sprintf_safe(szTemp, 200, "#ifndef _%s_COMMAND_H\n", szHText);
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "#define _%s_COMMAND_H\n\n", szHText);
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);

	sprintf_safe(szTemp, 200, "#pragma once\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "#include \"%s_Logic.h\"\n",
		objProjectInfo.m_szProjectName);
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "#include \"ClientCommand.h\"\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "#include \"IObject.h\"\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "#include <string>\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "#define MESSAGE_FUNCTION_BEGIN(x) switch(x) {\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "#define MESSAGE_FUNCTION(x,y,z) case x: { y(z); break; }\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "#define MESSAGE_FUNCTION_END }\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "using namespace std;\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);

	sprintf_safe(szTemp, 200, "class CBaseCommand : public CClientCommand\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "{\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "public:\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\tCBaseCommand(void);\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\t~CBaseCommand(void);\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\tint DoMessage(IMessage* pMessage, bool& bDeleteFlag);\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\tvoid SetServerObject(CServerObject* pServerObject);\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "public:\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	for(int i = 0; i < (int)objProjectInfo.m_objCommandList.size(); i++)
	{
		if(strlen(objProjectInfo.m_objCommandList[i].m_szCommandInID) > 0)
		{
			sprintf_safe(szTemp, 200, "\tint %s(IMessage* pMessage);\n", 
				objProjectInfo.m_objCommandList[i].m_szCommandFuncName);
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
		}
		else
		{
			char* pObjectRetun = NULL;
			for(int j = 0; j < (int)objProjectInfo.m_objCommandList[i].m_vecObjectInfo.size(); j++)
			{
				if(objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_emPacketType == PACKET_TYPE_RETURN)
				{
					pObjectRetun = objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName;
					break;
				}
			}

			//ֻ�����������
			sprintf_safe(szTemp, 200, "\tint %s(uint32 u4ConnectID, %s obj%s);\n",
				objProjectInfo.m_objCommandList[i].m_szCommandFuncName,
				pObjectRetun, 
				pObjectRetun);
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
		}
	}
	sprintf_safe(szTemp, 200, "\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "private:\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\tCServerObject* m_pServerObject;\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\tint            m_nCount;\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "};\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "#endif\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	fclose(pFile);
}

void Gen_2_Cpp_Command_Cpp(_Project_Info& objProjectInfo, vecClassInfo& objvecClassInfo)
{
	char szTemp[200]     = {'\0'};
	char szPathFile[200] = {'\0'};

	sprintf_safe(szPathFile, 200, "%s/BaseCommand.cpp", 
		objProjectInfo.m_szProjectName);

	//�������������ļ���
	FILE* pFile = fopen(szPathFile, "w");
	if(NULL == pFile)
	{
		return;
	}

	sprintf_safe(szTemp, 200, "#include \"BaseCommand.h\"\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "CBaseCommand::CBaseCommand(void)\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "{\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\tm_pServerObject = NULL;\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\tm_nCount        = 0;\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "}\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "CBaseCommand::~CBaseCommand(void)\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "{\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "}\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "void CBaseCommand::SetServerObject(CServerObject* pServerObject)\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "{\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\tm_pServerObject = pServerObject;\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "}\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "int CBaseCommand::DoMessage(IMessage* pMessage, bool& bDeleteFlag)\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "{\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\t__ENTER_FUNCTION();\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\tif(m_pServerObject == NULL)\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\t{\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\t\tOUR_DEBUG((LM_ERROR, \"[CBaseCommand::DoMessage] m_pServerObject is NULL(%%d).\\n\", bDeleteFlag));\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\t\treturn -1;\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\t}\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\tif(pMessage == NULL)\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\t{\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\t\tOUR_DEBUG((LM_ERROR, \"[CBaseCommand::DoMessage] pMessage is NULL.\\n\"));\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\t}\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\tMESSAGE_FUNCTION_BEGIN(pMessage->GetMessageBase()->m_u2Cmd);\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	for(int i = 0; i < (int)objProjectInfo.m_objCommandList.size(); i++)
	{
		if(strlen(objProjectInfo.m_objCommandList[i].m_szCommandInID) > 0)
		{
			sprintf_safe(szTemp, 200, "\tMESSAGE_FUNCTION((uint16)%s,  %s, pMessage);\n",
				objProjectInfo.m_objCommandList[i].m_szCommandInID,
				objProjectInfo.m_objCommandList[i].m_szCommandFuncName);
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);

		}
	}
	sprintf_safe(szTemp, 200, "\tMESSAGE_FUNCTION_END;\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\treturn 0;\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "\t__LEAVE_FUNCTION_WITHRETURN(0);\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	sprintf_safe(szTemp, 200, "}\n\n");
	fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);

	for(int i = 0; i < (int)objProjectInfo.m_objCommandList.size(); i++)
	{
		if(strlen(objProjectInfo.m_objCommandList[i].m_szCommandInID) > 0)
		{
			sprintf_safe(szTemp, 200, "int CBaseCommand::%s(IMessage* pMessage)\n",
				objProjectInfo.m_objCommandList[i].m_szCommandFuncName);
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "{\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);

			//��ͷ
			sprintf_safe(szTemp, 200, "\tIBuffPacket* pHeadPacket = m_pServerObject->GetPacketManager()->Create();\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\tif(NULL == pHeadPacket)\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\t{\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\t\tOUR_DEBUG((LM_ERROR, \"[CBaseCommand::%s] pHeadPacket is NULL.\\n\"));\n",
				objProjectInfo.m_objCommandList[i].m_szCommandFuncName);
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\t\treturn -1;\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\t}\n\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);

			//����
			sprintf_safe(szTemp, 200, "\tIBuffPacket* pBodyPacket = m_pServerObject->GetPacketManager()->Create();\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\tif(NULL == pBodyPacket)\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\t{\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\t\tOUR_DEBUG((LM_ERROR, \"[CBaseCommand::%s] pBodyPacket is NULL.\\n\"));\n",
				objProjectInfo.m_objCommandList[i].m_szCommandFuncName);
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\t\treturn -1;\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\t}\n\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);

			sprintf_safe(szTemp, 200, "\t_PacketInfo objHeadPacket;\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\t_PacketInfo objBodyPacket;\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\tpMessage->GetPacketHead(objHeadPacket);\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\tpMessage->GetPacketBody(objBodyPacket);\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);

			sprintf_safe(szTemp, 200, "\tpHeadPacket->WriteStream(objHeadPacket.m_pData, objHeadPacket.m_nDataLen);\n\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\tpBodyPacket->WriteStream(objBodyPacket.m_pData, objBodyPacket.m_nDataLen);\n\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);

			//������ͷ
			char* pHeadClassName = NULL;
			for(int j = 0; j < (int)objProjectInfo.m_objCommandList[i].m_vecObjectInfo.size(); j++)
			{
				if(objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_emPacketType == PACKET_TYPE_HEAD)
				{
					sprintf_safe(szTemp, 200, "\t%s obj%s;\n",
						objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName,
						objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName);
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t%s_Out_Stream(obj%s, pHeadPacket);\n",
						objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName,
						objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName);
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					pHeadClassName = objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName;
					break;
				}
			}

			//��������
			char* pBodyClassName = NULL;
			for(int j = 0; j < (int)objProjectInfo.m_objCommandList[i].m_vecObjectInfo.size(); j++)
			{
				if(objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_emPacketType == PACKET_TYPE_BODY)
				{
					sprintf_safe(szTemp, 200, "\t%s obj%s;\n",
						objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName,
						objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName);
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t%s_Out_Stream(obj%s, pBodyPacket);\n",
						objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName,
						objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName);
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					pBodyClassName = objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName;
					break;
				}
			}

			sprintf_safe(szTemp, 200, "\tm_pServerObject->GetPacketManager()->Delete(pHeadPacket);\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\tm_pServerObject->GetPacketManager()->Delete(pBodyPacket);\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\tbool blState = false;\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			if(strlen(objProjectInfo.m_objCommandList[i].m_szCommandOutID) > 0)
			{
				//ƴ�ӷ��ذ�������
				for(int j = 0; j < (int)objProjectInfo.m_objCommandList[i].m_vecObjectInfo.size(); j++)
				{
					//Ѱ���Ƿ��з��ذ���
					if(objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_emPacketType == PACKET_TYPE_RETURN)
					{
						if(pHeadClassName != NULL && pBodyClassName != NULL)
						{
							sprintf_safe(szTemp, 200, "\t%s obj%s;\n",
								objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName,
								objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName);
							fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);

							sprintf_safe(szTemp, 200, "\tblState = Logic_%s(m_pServerObject, pMessage->GetMessageBase()->m_u4ConnectID, obj%s, obj%s, obj%s);\n",
								objProjectInfo.m_objCommandList[i].m_szCommandFuncName, 
								pHeadClassName,
								pBodyClassName,
								objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName);
							fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
						}
						else
						{
							sprintf_safe(szTemp, 200, "\t%s obj%s;\n",
								objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName,
								objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName);
							fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);

							sprintf_safe(szTemp, 200, "\tblState = Logic_%s(m_pServerObject, pMessage->GetMessageBase()->m_u4ConnectID, obj%s, obj%s);\n",
								objProjectInfo.m_objCommandList[i].m_szCommandFuncName,
								pHeadClassName,
								objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName);
							fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
						}
						break;
					}
				}
			}
			else
			{
				if(pHeadClassName != NULL && pBodyClassName != NULL)
				{
					sprintf_safe(szTemp, 200, "\tblState = Logic_%s(m_pServerObject, pMessage->GetMessageBase()->m_u4ConnectID, obj%s, obj%s);\n", 
						objProjectInfo.m_objCommandList[i].m_szCommandFuncName,
						pHeadClassName,
						pBodyClassName);
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
				}
				else
				{
					if(pHeadClassName != NULL)
					{
						sprintf_safe(szTemp, 200, "\tblState = Logic_%s(m_pServerObject, pMessage->GetMessageBase()->m_u4ConnectID, obj%s);\n", 
							objProjectInfo.m_objCommandList[i].m_szCommandFuncName,
							pHeadClassName);
						fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					}
					else
					{
						sprintf_safe(szTemp, 200, "\tblState = Logic_%s(m_pServerObject, pMessage->GetMessageBase()->m_u4ConnectID);\n", 
							objProjectInfo.m_objCommandList[i].m_szCommandFuncName);
						fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					}
				}
			}
			sprintf_safe(szTemp, 200, "\tif(false == blState)\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\t{\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\t\treturn -1;\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "\t}\n\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
		}
		else
		{
			char* pObjectRetun = NULL;
			for(int j = 0; j < (int)objProjectInfo.m_objCommandList[i].m_vecObjectInfo.size(); j++)
			{
				if(objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_emPacketType == PACKET_TYPE_RETURN)
				{
					pObjectRetun = objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName;
					break;
				}
			}

			//ֻ�����������
			sprintf_safe(szTemp, 200, "int CBaseCommand::%s(uint32 u4ConnectID, %s obj%s)\n",
				objProjectInfo.m_objCommandList[i].m_szCommandFuncName,
				pObjectRetun, 
				pObjectRetun);
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
			sprintf_safe(szTemp, 200, "{\n");
			fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
		}

		if(strlen(objProjectInfo.m_objCommandList[i].m_szCommandOutID) > 0)
		{
			for(int j = 0; j < (int)objProjectInfo.m_objCommandList[i].m_vecObjectInfo.size(); j++)
			{
				if(objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_emPacketType == PACKET_TYPE_RETURN)
				{
					sprintf_safe(szTemp, 200, "\tIBuffPacket* pSendPacket = m_pServerObject->GetPacketManager()->Create();\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\tif(NULL == pSendPacket)\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t{\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t\tOUR_DEBUG((LM_ERROR, \"[CBaseCommand::%s] pSendPacket is NULL.\\n\"));\n",
						objProjectInfo.m_objCommandList[i].m_szCommandFuncName);
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t\treturn -1;\n\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t}\n\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t%s_In_Stream(obj%s, pSendPacket);\n\n",
						objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName,
						objProjectInfo.m_objCommandList[i].m_vecObjectInfo[j].m_szClassName);
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					//��������
					sprintf_safe(szTemp, 200, "\tif(NULL != m_pServerObject->GetConnectManager())\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t{\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					if(strlen(objProjectInfo.m_objCommandList[i].m_szCommandInID) == 0)
					{
						sprintf_safe(szTemp, 200, "\t\tm_pServerObject->GetConnectManager()->PostMessage(u4ConnectID,\n");
						fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					}
					else
					{
						sprintf_safe(szTemp, 200, "\t\tm_pServerObject->GetConnectManager()->PostMessage(pMessage->GetMessageBase()->m_u4ConnectID,\n");
						fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					}

					sprintf_safe(szTemp, 200, "\t\t\tpSendPacket,\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);

					sprintf_safe(szTemp, 200, "\t\t\tSENDMESSAGE_JAMPNOMAL,\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	
					sprintf_safe(szTemp, 200, "\t\t\t(uint16)%s,\n", 
						objProjectInfo.m_objCommandList[i].m_szCommandOutID);
					sprintf_safe(szTemp, 200, "\t\t\tPACKET_SEND_IMMEDIATLY,\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t\t\tPACKET_IS_FRAMEWORK_RECYC);\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t}\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\telse\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t{\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t\tOUR_DEBUG((LM_INFO, \"[CBaseCommand::%s] m_pConnectManager = NULL.\\n\"));\n",
						objProjectInfo.m_objCommandList[i].m_szCommandFuncName);
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t\tm_pServerObject->GetPacketManager()->Delete(pSendPacket);\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					sprintf_safe(szTemp, 200, "\t}\n\n");
					fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
					break;
				}
			}
		}
		sprintf_safe(szTemp, 200, "\treturn 0;\n");
		fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
		sprintf_safe(szTemp, 200, "}\n\n");
		fwrite(szTemp, strlen(szTemp), sizeof(char), pFile);
	}
	

	fclose(pFile);
}

#endif
