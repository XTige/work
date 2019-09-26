#include "NCS_FileCheck.h"

char myModuleId[256];
rpt_info_t zdogsInfo;
rpt_err_info rptErrInfo;

int g_sqlCode;
static MYSQL_DB_INFO selectConn1;
#define INSERT 1
#define SELECT 2

DbLocation dbLocation2;
DbLocation dbLocation3;
int mdb_flag = 0, bdb_flag = 0;
vector<string> CFile;
vector<string> FileInf;

tbl_qimgm_settle_dt_st_def mySettleDtSt;

int initConn()
{
	KnowledgeDist file_inf_dist;
	BlwNodeSt connNode;
	int errCode = 0;
	memset((char *)&connNode, '\0', sizeof(connNode));
	memset((char *)&file_inf_dist, '\0', sizeof(KnowledgeDist));
	strcpy(file_inf_dist.plat_id, GLOBAL_PLAT_ID);
	strcpy(file_inf_dist.unit_tp, UNIT_TP_VERTICAL_CUT);
	strcpy(file_inf_dist.unit_value, UNIT_VAL_VERTICAL_CUT);

	if (getTabUsage("tbl_qistl_file_inf", file_inf_dist.usage_index) != SUCCESS)
	{
		memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_FAIL_PARA, NULL, "getTabUsage失败");
	}

	if (BLWKnowledgeQueryT(QUERYBYPK, &file_inf_dist, &errCode))
	{
		memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_OPSYS, NULL, "file_inf_dist.unit_value=[%s]", file_inf_dist.unit_value);
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_OPSYS, NULL, "search for file_inf_dist failed! errno:%d\n", errCode);
		return FAILURE;
	}

	if (file_inf_dist.wrt_in[0] == '1')
	{
		mdb_flag = 1;
		errCode = 0;
		memset((char *)&connNode, '\0', sizeof(connNode));
		strcpy(connNode.dbname, rtrim(file_inf_dist.db_id));
		strcpy(connNode.plat_id, GLOBAL_PLAT_ID);
		if (BLWNodeStQueryT(QUERYBYPK, &connNode, &errCode))
		{
			memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_OPSYS, NULL, "[%s][%s] search for connNode failed! errno:%d\n", connNode.dbname, connNode.plat_id, errCode);
			return FAILURE;
		}
		dbLocation2.SetDbId(rtrim(file_inf_dist.db_id));
		dbLocation2.SetIp(rtrim(connNode.ip));
		dbLocation2.SetPort(rtrim(connNode.port));
		dbLocation2.SetUser(rtrim(connNode.dbuser));
		dbLocation2.SetPassword(rtrim(connNode.dbpwd));
	}

	if (file_inf_dist.alt_db_wrt_in[0] == '1')
	{
		bdb_flag = 1;
		errCode = 0;
		memset((char *)&connNode, '\0', sizeof(connNode));
		strcpy(connNode.dbname, rtrim(file_inf_dist.alt_db_id));
		strcpy(connNode.plat_id, GLOBAL_PLAT_ID);
		if (BLWNodeStQueryT(QUERYBYPK, &connNode, &errCode))
		{
			memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_OPSYS, NULL, "[%s][%s] search for connNode failed! errno:%d\n", connNode.dbname, connNode.plat_id, errCode);
			return FAILURE;
		}
		dbLocation3.SetDbId(rtrim(file_inf_dist.alt_db_id));
		dbLocation3.SetIp(rtrim(connNode.ip));
		dbLocation3.SetPort(rtrim(connNode.port));
		dbLocation3.SetUser(rtrim(connNode.dbuser));
		dbLocation3.SetPassword(rtrim(connNode.dbpwd));
	}

	if (file_inf_dist.wrt_in[0] != '1' && file_inf_dist.alt_db_wrt_in[0] != '1')
	{
		ERRORLOG(BIZ_LOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_FAIL_PARA, NULL, "wrt_in & alt_db_wrt_in is 0");
		return FAILURE;
	}

	return SUCCESS;
}

int getFileInf(const char *ftype)
{
	char fileDt[8 + 1];
	memset(fileDt, 0, sizeof(fileDt));
	if (ftype == "CUSS")
	{
		memcpy(fileDt, mySettleDtSt.prev_settle_dt + 2, 6);
	}
	else if (ftype == "UASS" || ftype == "UTSS")
	{
		memcpy(fileDt, mySettleDtSt.prev_settle_dt, 8);
	}

	vector<DbLocation> dbLocations_array_read;
	if (mdb_flag == 1)
	{
		dbLocations_array_read.push_back(dbLocation2);
	}
	else if (bdb_flag == 1)
	{
		dbLocations_array_read.push_back(dbLocation3);
	}

	try
	{
		tr1::shared_ptr<IDbTasks> mysqlTasksreadSum(new MysqlDbTasks(dbLocations_array_read, true));
		mysqlTasksreadSum->Connect();

		bool success = false;

		DbExecuteAction *sum_mod_action = mysqlTasksreadSum->Execute();
		ExecuteFilter sum_mod_filter("set SQL_MODE='PAD_CHAR_TO_FULL_LENGTH,STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'");
		sum_mod_action->Do(&sum_mod_filter);
		sum_mod_action->EndAction();

		/* 设置字符集 */
		DbExecuteAction *sum_char_action = mysqlTasksreadSum->Execute();
		ExecuteFilter sum_char_filter("set names gbk");
		sum_char_action->Do(&sum_char_filter);
		sum_char_action->EndAction();

		DbQueryAction *Sum_query_action = mysqlTasksreadSum->Select();
		FileInf.clear();
		char sqlbuff[1024] = {0};
		if (ftype == "CUSS")
		{
			sprintf(sqlbuff, "select file_nm from viw_qistl_file_inf where file_proc_st='12' and file_tfr_direct='I' and (file_nm like 'IND%s%%ACOMN' or file_nm like 'IND%s%%ALFEE' or file_nm like 'IND%s%%AERRN')", fileDt, fileDt, fileDt);
		}
		else if (ftype == "UASS")
		{
			sprintf(sqlbuff, "select file_nm from viw_qistl_file_inf where file_proc_st='12' and file_tfr_direct='I' and (file_nm like 'QRSA_%s_00.csv.zip' or file_nm like 'QRSA_%s_ERR.csv.zip')", fileDt, fileDt);
		}
		else if (ftype == "UTSS")
		{
			sprintf(sqlbuff, "select file_nm from viw_qistl_file_inf where file_proc_st='12' and file_tfr_direct='I' and file_nm like 'QRST_%s_00.csv.zip'", fileDt);
		}

		QueryFilter selectSumFilter(sqlbuff);
		Sum_query_action->Do(&selectSumFilter);
		DEBUGLOG(BIZLOG, "sqlbuff[%s]!", sqlbuff);

		// 获取Sum内容
		DbQueryRslt *Sum_query_rslt = (DbQueryRslt *)Sum_query_action->GetRslt();

		Row Sumrslt;
		Sumrslt = Sum_query_rslt->Fetch(success);
		if ((char **)Sumrslt == NULL)
		{
			INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_OPSYS, NULL, "当天没有%s文件上送记录，断开EndAction，数据库，返回\n", ftype);
			Sum_query_action->EndAction();
			mysqlTasksreadSum->Disconnect();
			return SUCCESS;
		}
		while ((char **)Sumrslt != NULL)
		{
			unsigned long *lengths = Sum_query_rslt->GetCurrentRowColumnsLength(success);
			/* 将读出的数据赋值 */
			char fileNm[100];
			memset(fileNm, 0, sizeof(fileNm));
			strcpy(fileNm, Sumrslt["file_nm"]);
			rtrim(fileNm);
			FileInf.push_back(string(fileNm));

			/* 取下一条数据 */
			Sumrslt = Sum_query_rslt->Fetch(success);
		}

		Sum_query_action->EndAction();
		mysqlTasksreadSum->Disconnect();
	}
	catch (ThrowableException &e)
	{
		memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "%s", e.What().c_str());
		return FAILURE;
	}
	catch (...)
	{
		memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "%s", "unknown exception");
		return FAILURE;
	}

	return SUCCESS;
}

/*--------------------  以下是数据分发转移完成判断要用到的函数  ------------------------*/
int tblQimgmSettleDtStOpr(tbl_qimgm_settle_dt_st_def *p_tblQimgmSettleDtSt, int *errCode)
{
	glbMysqlSetIsolation(&selectConn1);
	char buff[16 * 1024] = {0};
	memset(buff, 0, sizeof(buff));
	sprintf(buff, "SELECT INS_TP,CURRENT_SETTLE_DT,PREV_SETTLE_DT,CUT_OFF_ST,SYS_ST,CURRENT_SMS_CYCLE,CURRENT_LOG_NO,PREV_LOG_NO,CUTOFF_BEGIN_TS,CUTOFF_END_TS,PRE_PROC_DT,PRE_PROC_LOG_NO,PARA_BEGIN_IN,OPER_IN,EVENT_ID,REC_ID,REC_UPD_USR_ID,REC_UPD_TS,REC_CRT_TS from `tbl_qimgm_settle_dt_st` where ins_tp='00'");
	if (SUCCESS != glbdbExecuteQuery(&selectConn1, buff, errCode))
	{
		glbdbGetLastExecutionErrNo(&selectConn1, errCode);
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "tbl_qimgm_settle_dt_st select失败 [%d]\n", *errCode);
		glbdbCloseExecutionResult(&selectConn1, errCode);
		return FAILURE;
	}
	if (SUCCESS != glbdbOpenExecutionResult(&selectConn1, errCode))
	{
		glbdbGetLastExecutionErrNo(&selectConn1, errCode);
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "tbl_qimgm_settle_dt_st open失败[%d]\n", *errCode);
		glbdbCloseExecutionResult(&selectConn1, errCode);
		return FAILURE;
	}

	while (TRUE)
	{
		char **row = NULL;
		int sql_code;
		glbdbFetchOneRowFromOpenResult(&selectConn1, &row, errCode);
		if (row != NULL)
		{
			// prepare for the VARCHAR
			unsigned long *lengths = NULL;
			glbdbFetchResultFieldslengths(&selectConn1, &lengths, errCode);
			strcpy(p_tblQimgmSettleDtSt->ins_tp, row[0]);
			strcpy(p_tblQimgmSettleDtSt->current_settle_dt, row[1]);
			strcpy(p_tblQimgmSettleDtSt->prev_settle_dt, row[2]);
			strcpy(p_tblQimgmSettleDtSt->cut_off_st, row[3]);
			strcpy(p_tblQimgmSettleDtSt->sys_st, row[4]);
			strcpy(p_tblQimgmSettleDtSt->current_log_no, row[6]);
			strcpy(p_tblQimgmSettleDtSt->prev_log_no, row[7]);
			strcpy(p_tblQimgmSettleDtSt->cutoff_begin_ts, row[8]);
			strcpy(p_tblQimgmSettleDtSt->cutoff_end_ts, row[9]);
			break;
		}
		else
		{
			glbdbGetLastExecutionErrNo(&selectConn1, errCode);
			if (*errCode == 0)
			{
				//取到最后一条了
				break;
			}
			else
			{
				char sqlErrMsg[200] = {0};
				glbdbGetLastExecutionErrMsg(&selectConn1, sqlErrMsg);
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "fetch tbl_qimgm_settle_dt_st error");
				glbdbCloseExecutionResult(&selectConn1, errCode);
				return FAILURE;
			}
		}
	}
	glbdbCloseExecutionResult(&selectConn1, errCode);
	return SUCCESS;
}

INT32 createFileName(char *matchStr, char *outStr, const char *ftype)
{
	INT32 i, j, len;
	j = 0;
	len = strlen(matchStr);

	for (i = 0; i < len; i++)
	{
		switch (matchStr[i])
		{
		case '*': // 清算日期YYMMDD
			for (j = i; j < len; j++)
			{
				if (matchStr[j] != '*')
				{
					i = j - 1;
					break;
				}
			}
			if (ftype == "CUSS")
			{
				strcat(outStr, mySettleDtSt.prev_settle_dt + 2);
			}
			else if (ftype == "UASS" || ftype == "UTSS")
			{
				strcat(outStr, mySettleDtSt.prev_settle_dt);
			}
			break;
		default:
			outStr[strlen(outStr)] = matchStr[i];
			break;
		}
		if (j >= len) // 已搜索到串尾, 结束
			i = len;
	}
	return SUCCESS;
}

int getFileList(const char *ftype)
{
	char cfgFileName[1024];
	char FileName[40 + 1];
	char outFileName[40 + 1];
	char segName[100];
	int FileNum = 0;

	CFile.clear();

	if (NULL == getenv("CONFIG_PATH"))
	{
		memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_GET_FAIL_ENV, NULL, "glbPflGetInt...getenv CONFIG_PATH failed\n");
		return FAILURE;
	}
	memset(cfgFileName, '\0', sizeof(cfgFileName));
	sprintf(cfgFileName, "%s/%s", getenv("CONFIG_PATH"), "NCS_FileCheck.cfg");

	char typeName[20];
	memset(typeName, '\0', sizeof(typeName));
	char typeNum[20];
	memset(typeNum, '\0', sizeof(typeNum));
	sprintf(typeName, "%sFILELIST", ftype);
	sprintf(typeNum, "%sFILENUM", ftype);
	// if(ftype == "CUSS")
	// {
	// 	sprintf(typeName,"CUSSFILELIST");
	// 	sprintf(typeNum,"CUSSFILENUM");
	// }
	// else if(ftype == "UASS")
	// {
	// 	sprintf(typeName,"UASSFILELIST");
	// 	sprintf(typeNum,"UASSFILENUM");
	// }
	// else if(ftype == "UTSS")
	// {
	// 	sprintf(typeName,"UTSSFILELIST");
	// 	sprintf(typeNum,"UTSSFILENUM");
	// }

	if (glbPflGetInt(typeName, typeNum, cfgFileName, &FileNum) != SUCCESS)
	{
		memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_OPSYS, NULL,
				 "glbPflGetInt DBCONNSLEEP END fail,cfgFileName=[%s]", cfgFileName);
		return FAILURE;
	}
	INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "config data %sFILENUM [%d]\n", ftype, FileNum);

	for (int id = 0; id < FileNum; id++)
	{
		memset(FileName, 0, sizeof(FileName));
		memset(segName, 0, sizeof(segName));
		sprintf(segName, "%sFILENAME%02d", ftype, id + 1);

		if (glbPflGetString(typeName, segName, cfgFileName, FileName) != SUCCESS)
		{
			memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_OPSYS, NULL, "读取配置参数 [%sFILELIST] FILENAME fail,配置文件名=[%s]\n", ftype, cfgFileName);
			return FAILURE;
		}

		memset(outFileName, 0, sizeof(outFileName));
		createFileName(FileName, outFileName,ftype);
		INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "config data %sFILENAME  [%s]\n", ftype, outFileName);
		CFile.push_back(string(outFileName));
	}

	return SUCCESS;
}

int tblQimgmCtProcStOpr(int opr, tbl_qimgm_ct_proc_st_def *p_ctprocSt, int *errCode)
{
	char buff[16 * 1024] = {0};
	switch (opr)
	{
	case INSERT:
		memset(buff, 0, sizeof(buff));
		sprintf(buff, "replace into tbl_qimgm_ct_proc_st(CURRENT_SETTLE_DT,CYCLE_NO,MSG_INDEX,BAT_NO,PROC_END_IN,REC_UPD_TS,REC_CRT_TS) \
											values('%s','%s','%s',0,'%s','%s','%s')",
				p_ctprocSt->current_settle_dt,
				p_ctprocSt->cycle_no,
				p_ctprocSt->msg_index,
				p_ctprocSt->proc_end_in,
				getHostDateTimeUsec(), getHostDateTimeUsec());
		if (glbdbExecuteQuery(&selectConn1, buff, errCode) != SUCCESS)
		{
			memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "glbdbExecuteQuery失败,错误码为:[%d][%s]\n", *errCode, buff);
			return FAILURE;
		}
		glbdbMysqlCommit(&selectConn1, errCode);
		break;
	case SELECT:
		glbMysqlSetIsolation(&selectConn1);
		memset(buff, 0, sizeof(buff));
		sprintf(buff,
				"SELECT  ifnull(max(current_settle_dt),'') from `tbl_qimgm_ct_proc_st` WHERE  msg_index ='%s' ", p_ctprocSt->msg_index); // omit other where sub-clause

		if (SUCCESS != glbdbExecuteQuery(&selectConn1, buff, errCode))
		{
			glbdbGetLastExecutionErrNo(&selectConn1, &g_sqlCode);
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "tbl_qimgm_ct_proc_st select失败 [%d][%s]\n", *errCode, buff);
			glbdbCloseExecutionResult(&selectConn1, errCode);

			return FAILURE;
		}
		if (SUCCESS != glbdbOpenExecutionResult(&selectConn1, errCode))
		{
			glbdbGetLastExecutionErrNo(&selectConn1, errCode);
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "tbl_qimgm_ct_proc_st open失败 [%d][%s]\n", *errCode, buff);
			glbdbCloseExecutionResult(&selectConn1, errCode);

			return FAILURE;
		}
		while (TRUE)
		{
			char **row = NULL;
			int sql_code;
			glbdbFetchOneRowFromOpenResult(&selectConn1, &row, errCode);
			if (row != NULL)
			{
				strcpy(p_ctprocSt->current_settle_dt, row[0]);
				break;
			}
			else
			{
				glbdbGetLastExecutionErrNo(&selectConn1, errCode);
				if (*errCode == 0)
				{
					//取到最后一条了
					break;
				}
				else
				{
					char sqlErrMsg[200] = {0};
					glbdbGetLastExecutionErrMsg(&selectConn1, sqlErrMsg);
					ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "fetch tbl_qimgm_ct_proc_st error");
					glbdbCloseExecutionResult(&selectConn1, errCode);
					return FAILURE;
				}
			}
		}
		glbdbCloseExecutionResult(&selectConn1, errCode);
		//disconnectMySQL(&selectConn1);

		break;
	default:
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "fetch tbl_qimgm_ct_proc_st error");
		return FAILURE;
	}

	return SUCCESS;
}

int insertCtProcSt(char settleDT[9], int *p_sqlCode, const char *ftype)
{
	int sql_code = 0;
	tbl_qimgm_ct_proc_st_def CtprocSt;
	memset(&CtprocSt, 0, sizeof(CtprocSt));
	if (ftype == "CUSS")
	{
		strcpy(CtprocSt.current_settle_dt, settleDT);
		strcpy(CtprocSt.cycle_no, "OF");
		strcpy(CtprocSt.msg_index, "QICS");
		strcpy(CtprocSt.proc_end_in, "1");
	}
	else if (ftype == "UASS")
	{
		strcpy(CtprocSt.current_settle_dt, settleDT);
		strcpy(CtprocSt.cycle_no, "OF");
		strcpy(CtprocSt.msg_index, "QIUA");
		strcpy(CtprocSt.proc_end_in, "1");
	}
	else if (ftype == "UTSS")
	{
		strcpy(CtprocSt.current_settle_dt, settleDT);
		strcpy(CtprocSt.cycle_no, "OF");
		strcpy(CtprocSt.msg_index, "QIUT");
		strcpy(CtprocSt.proc_end_in, "1");
	}
	if (tblQimgmCtProcStOpr(INSERT, &CtprocSt, p_sqlCode) != SUCCESS)
	{
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "tblQimgmCtProcStOpr INSERT1[%d]", p_sqlCode);
		return FAILURE;
	}

	return SUCCESS;
}

int insertDb2CtProcSt1(char settleDT[9], int *p_sqlCode, const char *ftype)
{
	int sql_code = 0;
	tbl_cumgm_ct_proc_st_def CtprocSt;
	memset(&CtprocSt, 0, sizeof(CtprocSt));
	if (ftype == "CUSS")
	{
		strcpy(CtprocSt.current_settle_dt, settleDT);
		strcpy(CtprocSt.cycle_no, "OF");
		strcpy(CtprocSt.msg_index, "QICS");
		strcpy(CtprocSt.proc_end_in, "1");
	}
	else if (ftype == "UASS")
	{
		strcpy(CtprocSt.current_settle_dt, settleDT);
		strcpy(CtprocSt.cycle_no, "OF");
		strcpy(CtprocSt.msg_index, "QIUA");
		strcpy(CtprocSt.proc_end_in, "1");
	}
	else if (ftype == "UTSS")
	{
		strcpy(CtprocSt.current_settle_dt, settleDT);
		strcpy(CtprocSt.cycle_no, "OF");
		strcpy(CtprocSt.msg_index, "QIUT");
		strcpy(CtprocSt.proc_end_in, "1");
	}

	if (glbdbConnect(CUMGMDB, CUMGMDBID, &g_sqlCode) != SUCCESS)
	{
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "CUMGMDB连接参数库失败:[%d]!", g_sqlCode);
		return FAILURE;
	}

	if (cussdbTblCumgmCtProcStOpr(INSERT1, &CtprocSt, p_sqlCode) != SUCCESS)
	{
		if (*p_sqlCode == -803)
		{
			glbdbCommit(&sql_code);
			if (cussdbTblCumgmCtProcStOpr(UPDATE4, &CtprocSt, p_sqlCode) != SUCCESS)
			{
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "tblQimgmCtProcStOpr UPDATE4[%d]", p_sqlCode);
				glbdbDisconnect(&sql_code);
				return FAILURE;
			}
		}
		else
		{
			glbdbCommit(&sql_code);
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "tblQimgmCtProcStOpr INSERT1[%d]", p_sqlCode);
			glbdbDisconnect(&sql_code);
			return FAILURE;
		}
	}

	if (glbdbCommit(&sql_code) != SUCCESS)
	{
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "数据库COMMIT操作失败[%d]!", sql_code);
	}
	glbdbDisconnect(&sql_code);

	return SUCCESS;
}

void getLocalTime(char *lctime)
{
	time_t timep;
	struct tm tm1;
	time(&timep);

	tm1 = *localtime(&timep);

	sprintf(lctime, "%04d%02d%02d %02d:%02d:%02d",
			tm1.tm_year + 1900, tm1.tm_mon + 1, tm1.tm_mday,
			tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
}

/*  计算YYYYMMDD(需大于19700101)格式字符串日期的后一天  */
char *getPastDate(char *date)
{
	struct tm tm1, *tm2;
	time_t clock1;
	char buf[5];

	tm1.tm_sec = 0;
	tm1.tm_min = 0;
	tm1.tm_hour = 0;

	memcpy(buf, date, 4);
	buf[4] = '\0';
	tm1.tm_year = atoi(buf) - 1900;

	memcpy(buf, date + 4, 2);
	buf[2] = '\0';
	tm1.tm_mon = atoi(buf) - 1;

	memcpy(buf, date + 6, 2);
	buf[2] = '\0';
	tm1.tm_mday = atoi(buf);

	clock1 = mktime(&tm1) + 60L * 60L * 24L;
	tm2 = localtime(&clock1);

	sprintf(date, "%04d%02d%02d",
			tm2->tm_year + 1900, tm2->tm_mon + 1, tm2->tm_mday);

	return date;
}

int check(const char *ftype, char *zy_now_time, char *cfgTmWarnStart, char *cfgTmWarnEnd)
{
	char timestamp[20];
	memset(timestamp, '\0', sizeof(timestamp));
	const char *MSG_INDEX;
	if (ftype == "CUSS")
	{
		MSG_INDEX = "QICS";
	}
	else if (ftype == "UASS")
	{
		MSG_INDEX = "QIUA";
	}
	else if (ftype == "UTSS")
	{
		MSG_INDEX = "QIUT";
	}

	char max_current_dt_chl[9];
	memset(max_current_dt_chl, '\0', sizeof(max_current_dt_chl));
	tbl_qimgm_ct_proc_st_def tblQimgmCtProcSt;
	memset(&tblQimgmCtProcSt, 0, sizeof(tblQimgmCtProcSt));
	memcpy(tblQimgmCtProcSt.msg_index, MSG_INDEX, sizeof(tblQimgmCtProcSt.msg_index));
	tblQimgmCtProcStOpr(SELECT, &tblQimgmCtProcSt, &g_sqlCode);
	if (g_sqlCode != SUCCESS)
	{
		if (g_sqlCode == 100)
		{
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "tbl_qimgm_ct_proc_st表 的记录为空,(首次运行需要插入current_settle_dt为T-1日的UTF1一行信息,日切发生在T日\n");
			return FAILURE;
		}
		else
		{
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "tblQimgmCtProcStOpr SELECTMAX1 错误[%d]", g_sqlCode);
			return FAILURE;
		}
	}

	memcpy(max_current_dt_chl, tblQimgmCtProcSt.current_settle_dt, sizeof(tblQimgmCtProcSt.current_settle_dt));
	if (NULL == max_current_dt_chl || 0 == strlen(max_current_dt_chl))
	{
		ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "tbl_qimgm_ct_proc_st表 的记录为空,(首次运行需要插入current_settle_dt为T-1日的UTF1信息,日切发生在T日)");
		return FAILURE;
	}

	char tmp_time[9];
	memset(tmp_time, '\0', sizeof(tmp_time));
	memcpy(tmp_time, mySettleDtSt.prev_settle_dt, 8);
	if (0 == memcmp(getPastDate(max_current_dt_chl), tmp_time, 8))
	{
		CFile.clear();
		FileInf.clear();
		if (getFileList(ftype) != SUCCESS)
		{
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "getFileList 错误!");
			return FAILURE;
		}
		mdb_flag = 0;
		bdb_flag = 0;
		if (initConn() != SUCCESS)
		{
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "initConn 错误!");
			return FAILURE;
		}
		if (getFileInf(ftype) != SUCCESS)
		{
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "getFileInf 错误!");
			return FAILURE;
		}
		vector<string>::iterator it;
		if (FileInf.size() >= CFile.size())
		{
			int checkFlag = 0;
			int checkNum = 0;
			for (int bk = 0; bk < CFile.size(); bk++)
			{
				it = find(FileInf.begin(), FileInf.end(), CFile[bk]);
				if (it == FileInf.end())
				{
					checkFlag = 0;
					INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "%s系统业务文件尚未上送齐全 [%s]", ftype, CFile[bk].c_str());
					break;
				}
				else
					checkNum++;
			}
			if (checkNum == CFile.size())
			{
				checkFlag = 1;
			}

			if (checkFlag != 1)
			{
				if ((memcmp(&zy_now_time[9], cfgTmWarnStart, 8) > 0) && (memcmp(&zy_now_time[9], cfgTmWarnEnd, 8) < 0))
				{
					ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "%s系统业务文件尚未上送齐全 SettleDt[%s]", ftype, mySettleDtSt.prev_settle_dt);
				}
				return FAILURE;
			}

			if (FileInf.size() > CFile.size())
			{
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "%s系统业务上送文件数超过预期,实际[%d]预期[%d]", ftype, FileInf.size(), CFile.size());
				for (int ii = 0; ii < FileInf.size(); ii++)
					INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "%s系统业务上送文件 [%s]", ftype, FileInf[ii].c_str());
				for (int ii = 0; ii < CFile.size(); ii++)
					INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "预期%s系统文件 [%s]", ftype, CFile[ii].c_str());
				return FAILURE;
			}

			if (insertDb2CtProcSt1(mySettleDtSt.prev_settle_dt, &g_sqlCode, ftype) != SUCCESS)
			{
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "insertDb2CtProcSt1 fail ErrNo[%d]", g_sqlCode);
				return FAILURE;
			}

			if (insertCtProcSt(mySettleDtSt.prev_settle_dt, &g_sqlCode, ftype) != SUCCESS)
			{
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "insertCtProcSt fail ErrNo[%d]", g_sqlCode);
				return FAILURE;
			}

			INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "%s系统业务文件上送齐全  [%d]", ftype, FileInf.size());
			for (int ii = 0; ii < FileInf.size(); ii++)
			{
				INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "%s系统业务上送文件 [%s]", ftype, FileInf[ii].c_str());
			}
			return SUCCESS;
		}
		else
		{
			INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "%s系统业务文件尚未上送齐全", ftype);
			for (int ii = 0; ii < CFile.size(); ii++)
			{
				INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "预期%s系统文件 [%s]", ftype, CFile[ii].c_str());
			}
			for (int ii = 0; ii < FileInf.size(); ii++)
			{
				INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "%s系统业务上送文件 [%s]", ftype, FileInf[ii].c_str());
			}
			if ((memcmp(&zy_now_time[9], cfgTmWarnStart, 8) > 0) && (memcmp(&zy_now_time[9], cfgTmWarnEnd, 8) < 0))
			{
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "%s系统业务文件尚未上送齐全 SettleDt[%s]", ftype, mySettleDtSt.prev_settle_dt);
			}
			return FAILURE;
		}
	}
	else
	{
		INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "******** %s系统业务文件上送检查判断结束 ********", ftype);
		return SUCCESS;
	}
}

int NCS_FileCheck(void)
{
	int ret;
	memset(myModuleId, '\0', sizeof(myModuleId));
	strcpy(myModuleId, "NCS_FileCheck");

	ret = CUSSLOGINIT(myModuleId);
	if (ret != SUCCESS)
	{
		(void)fprintf(stderr, "引用报错结果错误:[%s].", zDogstrerror(ret));
		return -1;
	}

	INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "CUSS、UASS、UTSS系统业务文件上送检查开始...");
	while (true)
	{
		char cfgFileName_t[1024];
		int cfgDbConnSleep = 0;

		if (NULL == getenv("CONFIG_PATH"))
		{
			memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_GET_FAIL_ENV, NULL, "glbPflGetInt...getenv CONFIG_PATH failed\n", __LINE__);
			return FAILURE;
		}
		memset(cfgFileName_t, '\0', sizeof(cfgFileName_t));
		sprintf(cfgFileName_t, "%s/%s", getenv("CONFIG_PATH"), "NCS_FileCheck.cfg");
		if (glbPflGetInt("TIMEPERIOD", "DBCONNSLEEP", cfgFileName_t, &cfgDbConnSleep) != SUCCESS)
		{
			memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
			ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_OPSYS, NULL, "glbPflGetInt DBCONNSLEEP END fail,cfgFileName=[%s]", cfgFileName_t);
			return FAILURE;
		}
		INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "config data DBCONNSLEEP  [%d] s\n", cfgDbConnSleep);

		while (true)
		{
			if (connectMySQL(&selectConn1, QIMGMDBID) != SUCCESS)
			{
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "QIMGMDB连接参数库失败:[%d]!", g_sqlCode);
				sleep(cfgDbConnSleep);
			}
			else
			{
				break;
			}
		}
		char settleDt[9] = {0};

		while (true)
		{
			char cfgFileName[1024];
			char cfgTmStart[9];
			char cfgTmEnd[9];
			char cfgTmWarnStart[9];
			char cfgTmWarnEnd[9];
			int cfgsleepSecond = 0;
			char cfgswitch[3];

			if (NULL == getenv("CONFIG_PATH"))
			{
				memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_GET_FAIL_ENV, NULL, "glbPflGetInt...getenv CONFIG_PATH failed\n");
				return FAILURE;
			}
			memset(cfgFileName, '\0', sizeof(cfgFileName));
			sprintf(cfgFileName, "%s/%s", getenv("CONFIG_PATH"), "NCS_FileCheck.cfg");

			memset(cfgTmStart, '\0', sizeof(cfgTmStart));
			memset(cfgTmEnd, '\0', sizeof(cfgTmEnd));
			memset(cfgTmWarnStart, '\0', sizeof(cfgTmWarnStart));
			memset(cfgTmWarnEnd, '\0', sizeof(cfgTmWarnEnd));
			memset(cfgswitch, '\0', sizeof(cfgswitch));
			//读取配置文件etc/NCS_FileCheck.cfg中的开始解释日期
			if (glbPflGetString("TIMEPERIOD", "START", cfgFileName, cfgTmStart) != SUCCESS)
			{
				memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_OPSYS, NULL, "读取配置参数 [TIMEPERIOD] START fail,配置文件名=[%s]\n", cfgFileName);
				return FAILURE;
			}
			if (glbPflGetString("TIMEPERIOD", "END", cfgFileName, cfgTmEnd) != SUCCESS)
			{
				memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_OPSYS, NULL, "读取配置参数 [TIMEPERIOD] END fail,配置文件名=[%s]\n", cfgFileName);
				return FAILURE;
			}
			if (glbPflGetString("TIMEPERIOD", "WARNSTART", cfgFileName, cfgTmWarnStart) != SUCCESS)
			{
				memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_OPSYS, NULL, "读取配置参数 [TIMEPERIOD] WARNSTART fail,配置文件名=[%s]\n", cfgFileName);
				return FAILURE;
			}
			if (glbPflGetString("TIMEPERIOD", "WARNEND", cfgFileName, cfgTmWarnEnd) != SUCCESS)
			{
				memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_OPSYS, NULL, "读取配置参数 [TIMEPERIOD] WARNEND fail,配置文件名=[%s]\n", cfgFileName);
				return FAILURE;
			}

			if (glbPflGetInt("TIMEPERIOD", "SLEEPSECOND", cfgFileName, &cfgsleepSecond) != SUCCESS)
			{
				cfgsleepSecond = 60; // 读取配置参数失败后，该函数会把变量置为0的，所以需要这么赋值一下
				memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_OPSYS, NULL, "读取配置参数 [TIMEPERIOD] SLEEPSECOND fail,配置文件名=[%s]\n", cfgFileName);
				return FAILURE;
			}
			if (glbPflGetString("SWITCH", "BUTTON", cfgFileName, cfgswitch) != SUCCESS)
			{
				memset(&zdogsInfo, '\0', sizeof(rpt_info_t));
				ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_OPSYS, NULL, "读取配置参数 [SWITCH] BUTTON fail,配置文件名=[%s]\n", cfgFileName);
				return FAILURE;
			}
			INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "config data START        [%s]", cfgTmStart);
			INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "config data END          [%s]", cfgTmEnd);
			INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "config data WARNSTART    [%s]", cfgTmWarnStart);
			INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "config data WARNEND      [%s]", cfgTmWarnEnd);
			INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "config data SLEEP        [%d]s", cfgsleepSecond);
			INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "config data  SWITCH      [%s]", cfgswitch);

			char zy_now_time[LONG_TIME_LEN] = {0};
			memset(zy_now_time, 0, sizeof(zy_now_time));
			getLocalTime(zy_now_time);
			char now_time_yyyymmddhhmmss[15];
			memset(now_time_yyyymmddhhmmss, 0, sizeof(now_time_yyyymmddhhmmss));
			memcpy(now_time_yyyymmddhhmmss, zy_now_time, 8);
			memcpy(now_time_yyyymmddhhmmss + 8, zy_now_time + 9, 2);
			memcpy(now_time_yyyymmddhhmmss + 8 + 2, zy_now_time + 12, 2);
			memcpy(now_time_yyyymmddhhmmss + 8 + 2 + 2, zy_now_time + 15, 2);
			//zy_now_time 20180202 18:29:54
			INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "zy_now_time [%s] now_time_yyyymmddhhmmss[%s] \n", zy_now_time, now_time_yyyymmddhhmmss);
			if (memcmp(cfgTmEnd, cfgTmStart, 8) > 0)
			{
				INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "cfgTmEnd > cfgTmStart");
				INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL,
						"memcmp(&zy_now_time[9],cfgTmStart,8)=[%d] memcmp(&zy_now_time[9],cfgTmEnd,8)=[%d]", memcmp(&zy_now_time[9], cfgTmStart, 8), memcmp(&zy_now_time[9], cfgTmEnd, 8));
			}
			if (memcmp(cfgTmEnd, cfgTmStart, 8) < 0)
			{
				INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "cfgTmEnd cfgTmStart in two days");
				INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "memcmp(&zy_now_time[9],cfgTmStart,8)=[%d] memcmp(&zy_now_time[9],cfgTmEnd,8)=[%d]",
						memcmp(&zy_now_time[9], cfgTmStart, 8), memcmp(&zy_now_time[9], cfgTmEnd, 8));
			}
			if (memcmp(cfgTmEnd, cfgTmStart, 8) == 0)
			{
				WARNLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "config file must be false,cfgTmEnd == cfgTmStart");
			}

			if (((memcmp(cfgTmEnd, cfgTmStart, 8) > 0) && ((memcmp(&zy_now_time[9], cfgTmStart, 8) > 0) && (memcmp(&zy_now_time[9], cfgTmEnd, 8) < 0))) ||
				((memcmp(cfgTmEnd, cfgTmStart, 8) < 0) && ((memcmp(&zy_now_time[9], cfgTmStart, 8) > 0) || (memcmp(&zy_now_time[9], cfgTmEnd, 8) < 0))))
			{
				INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "当前系统时间在窗口时间内");
				char cutoff_time[15];  //  记录YYYYMMDD格式
				char cutoff_time2[20]; //  记录YYYY-MM-DD HH:MM:SS格式
				char current_log_no[2];
				char buff[16 * 1024] = {0};

				memset(&mySettleDtSt, '\0', sizeof(tbl_qimgm_settle_dt_st_def));
				strcpy(mySettleDtSt.ins_tp, CUPSINSTP);
				if (tblQimgmSettleDtStOpr(&mySettleDtSt, &g_sqlCode) != SUCCESS)
				{
					ERRORLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "读取清算日期表错误:[%d]!", g_sqlCode);
					disconnectMySQL(&selectConn1);
					if (selectConn1.mysql != NULL)
					{
						free(selectConn1.mysql);
						selectConn1.mysql = NULL;
					}
					if (selectConn1.res != NULL)
					{
						free(selectConn1.res);
						selectConn1.res = NULL;
					}
					mysql_library_end();
					break;
				}
				INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_DATABASE, NULL, "********CUSS、UASS、UTSS系统业务文件上送检查判断开始 *******");
				if (0 == memcmp("ON", cfgswitch, 2))
				{
					int cuss = check("CUSS", zy_now_time, cfgTmWarnStart, cfgTmWarnEnd);
					int uass = check("UASS", zy_now_time, cfgTmWarnStart, cfgTmWarnEnd);
					int utss = check("UTSS", zy_now_time, cfgTmWarnStart, cfgTmWarnEnd);
					if ((cuss != SUCCESS) || (uass != SUCCESS) || (utss = SUCCESS))
					{
						disconnectMySQL(&selectConn1);
						if (selectConn1.mysql != NULL)
						{
							free(selectConn1.mysql);
							selectConn1.mysql = NULL;
						}
						if (selectConn1.res != NULL)
						{
							free(selectConn1.res);
							selectConn1.res = NULL;
						}
						mysql_library_end();
						break;
					}
				}
				else
				{
					WARNLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "配置文件中 开关不为 ON，不进行 CUSS、UASS、UTSS系统业务文件上送检查完成判断");
				}
			}
			else
			{
				INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "当前时间不在窗口时间内,sleep() 循环等待");
			}
			INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "sleep( %d s )", cfgsleepSecond);
			sleep(cfgsleepSecond);
		}
		sleep(5);
	}
	INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, myModuleId, EID_SYSINFO, NULL, "CUSS、UASS、UTSS系统业务文件上送检查监控退出...");
	CUSSLOGDONE();
	return 0;
}

int svr_init(int argc, char **argv)
{
	int ret = CUSSLOGINIT("NCS_FileCheck");
	if (ret != SUCCESS)
	{
		(void)fprintf(stderr, "引用报错结果错误:[%s].", zDogstrerror(ret));
	}

	INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_OPSYS, NULL, "CUSS、UASS、UTSS系统业务文件上送检查监控服务启动成功");
	CUSSLOGDONE();
	return SUCCESS;
}
void svr_term()
{
	int ret = CUSSLOGINIT("NCS_FileCheck");
	if (ret != SUCCESS)
	{
		(void)fprintf(stderr, "引用报错结果错误:[%s].", zDogstrerror(ret));
	}
	INFOLOG(BIZLOG, &zdogsInfo, &rptErrInfo, "NCS_FileCheck", EID_SYSINFO, NULL, "CUSS、UASS、UTSS系统业务文件上送检查监控服务终止\n");

	DEBUGLOG(BIZLOG, "NCS_FileCheck 成功结束  ...\n");
	CUSSLOGDONE();
}

int main(int argc, char **argv)
{
	if (argc < 6)
	{ //调试用
		svr_init(argc, argv);
		NCS_FileCheck();
	}
	else
		return (startdemon(argc, argv, svr_init, NCS_FileCheck, (void (*)())svr_term));
}