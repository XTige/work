#include "NCS_TransTfrCheck.h"

char myModuleId[256];
rpt_info_t zdogsInfo;   /*报错信息结构欧体*/
rpt_err_info rptErrInfo; /*报错信息结构体*/

int g_sqlCode;
int g_subBjCheck = 0;

static MYSQL_DB_INFO selectConn1;
#define INSERT 1
#define SELECT 2
#define SELECT_1 3

CT_PROC_ST_MAP mapCtProcSt;
char prevSubSettleDt[9];
char oldPrevSubSettleDt[9];
tbl_qimgm_settle_dt_st_def mySettleDtSt;

tblQimgmSettleDtStOpr(tbl_qimgm_settle_dt_st_def *p_tblQimgmSettleDtSt,int *errCode)
{
	glbMysqlSetIsolation(&selectConn1);
	char buff[16 * 1024] = {0};
}
