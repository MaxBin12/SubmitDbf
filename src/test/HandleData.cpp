#include <string.h>
#include <string>
#include <stdlib.h>
#include <assert.h>
#include <dirent.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include "DbfRead.h"
#include "inifile.h"
#include <chrono>
#include <iostream>
#include <list>

// #include "dao_base.h"
// #include "dao_dynamicsql.h"
using namespace inifile;

int g_count = 0;
int g_nCommitCount =0;
int fileNo = 0;
FILE *pf;
FILE *fpInsert;
IniFile ini;
string g_strFile;

std::vector<char*> vecRes;
std::vector<stFieldHead> vecColumns; // 保存dbf文件的字段属性
string strSqlFileCnt = "100000";
string strTable = "tmp_qtzhzl_181123";
string strColumns = "@";

typedef void (*pLineCallback)(int iCnt, const char *pcszContent);

// CDAOBase g_oDaoBaseHandler;
// CDAODynamicSql g_oDaoDynamicSqlHandler;

string GetMsgValue(string strOrig, string strKey, string strSplit = ",")
{
	string strRetValue = "";
	int iStrOrigLen;
	int iStrKeyLen;
	size_t uiPosKeyBegin;
	size_t uiPosKeyEnd;
	size_t uiPosStrSplit;

	iStrOrigLen = strOrig.length();
	iStrKeyLen = strKey.length();
	uiPosKeyBegin = strOrig.find(strKey);

	if (uiPosKeyBegin != string::npos)
	{
		// 从key的位置开始,第一次出现 str_split 的位置
		uiPosStrSplit =  strOrig.substr(uiPosKeyBegin).find(strSplit);
		if (uiPosStrSplit != string::npos)
		{
			uiPosKeyEnd = uiPosKeyBegin + uiPosStrSplit;
		}
		else
		{
			uiPosKeyEnd = iStrOrigLen;
		}
		int pos_begin = uiPosKeyBegin + iStrKeyLen + 1; // +1 跳过'='字符
		int value_len = uiPosKeyEnd - pos_begin;
		strRetValue = strOrig.substr(pos_begin, value_len);
		return strRetValue;
	}
	return strRetValue;
}


void trim(std::string &s)
{
    if (!s.empty())
    {
        s.erase(0, s.find_first_not_of(" "));
        s.erase(s.find_last_not_of(" ") + 1);
    }
}

int GetConfigValue(string &strValue, string strKey, string strSection="CONFIG")
{
    int iRetCode;

    iRetCode = ini.getValue(strSection, strKey, strValue);
    if (iRetCode != RET_OK)
    {
        return RET_ERR;
    }
    return RET_OK;
}


// 变长结构体
typedef struct stContent
{
    int nSize;
    char buffer[0];
}stContent, *p_stContent;


// 获取要生成的文件的文件名
const char *GetFileName(void)
{
    string strValue;
    char szFileName[10] = {0};
    if (GetConfigValue(strValue, "SqlFileFolder") != RET_OK)
    {
        printf("Get Config \"SqlFilePath\" Failed!\n");
        abort();
    }

    memset(szFileName, 0, sizeof(szFileName));
    snprintf(szFileName, sizeof(szFileName), "/sql_%4.4d", fileNo);

    // XXX 为什么我返回tmp.cstr() 就不行呢
    // 如果g_strFile不是全局变量, 会报错
    g_strFile = strValue + szFileName + ".sql";
    // return szFileName;
    return g_strFile.c_str();
}

/*
 * @brief	: 生成sql文件
 * @param	:
 * @return	:
 */
void GenerateSql(std::vector<stFieldHead> vecFieldHead, char *pcszContent)
{
    int iColumnLen = 0;
    char szSql[2048] = {0};
    char chBuff[2] = {0};
    char buffer[1024] = {0};
    char szTempBuff[128] = {0};
    string strTmp;

    g_count++;

    /*
    if (GetConfigValue(strSqlFileCnt, "SqlFileCnt") != RET_OK)
    {
        printf("Get Config \"SqlFileCnt\" Failed!");
        abort();
    }
    if (GetConfigValue(strTable, "TableName") != RET_OK)
    {
        printf("Get Config \"TableName\" Failed!\n");
        abort();
    }

    if (GetConfigValue(strColumns, "Columns") != RET_OK)
    {
        printf("Get Config \"Columns\" Failed!\n");
        abort();
    }
    */

    // 每n条数据输出到一个文件
    // XXX 这里有点问题, 第一个文件会比 MaxCnt 小1
    if (g_count % atoi(strSqlFileCnt.c_str()) == 0)
    {
        g_count = 0;
        fclose(pf);
        fileNo++;
        pf = fopen(GetFileName(), "w+");
        assert(pf);
    }

    // 组装sql语句  --  根据m_vecFieldHead从pcszContent提取记录数据


    snprintf(szSql, sizeof(szSql), "insert into %s values(", strTable.c_str());

    for (int i = 0; i < vecFieldHead.size(); i++)
    {
        if (vecFieldHead[i].szName[0] != 0x00 && vecFieldHead[i].szName[0] != '\r')
        {
            // sprintf(chBuff, "%d", i + 1);
            snprintf(chBuff, sizeof(chBuff), "%d", i + 1);
            memcpy(&iColumnLen, vecFieldHead[i].szLen, 1);
            memset(buffer, 0x00, sizeof(buffer));

            // 按照需要指定需要插入的列
            if ((strstr(strColumns.c_str(), chBuff) != NULL) || strcmp(strColumns.c_str(), "@") == 0)
            {
                memcpy(buffer, pcszContent, iColumnLen); // 按照字段长度拷贝内存
                strTmp = string(buffer);
                trim(strTmp);
            }
            else
            {
                strTmp  = "";
            }
            strncat(szSql, "\'", 1);
            snprintf(szTempBuff, sizeof(szTempBuff), "%s", strTmp.c_str());
            strncat(szSql, szTempBuff, sizeof(szSql) - strlen(szSql) - 1);

            if(i< vecFieldHead.size() - 1)
            {
                strncat(szSql, "\', ", 3);
            }
            else
            {
                strncat(szSql, "\'", 1);
            }
        }
        pcszContent += iColumnLen; // 跳到下一个字段
    }
    snprintf(szTempBuff, sizeof(szTempBuff), "%s", ")");
    strncat(szSql, szTempBuff, sizeof(szSql) - strlen(szSql) - 1);

    // fprintf(pf, "%s\n", szSql);
    // fwrite(szSql.c_str(), strlen(szSql.c_str()), 1, pf);
    fwrite(szSql, strlen(szSql), 1, pf);
    fwrite(";\n", strlen(";\n"), 1, pf);
    // fflush(pf);
    vecColumns = vecFieldHead;
}

void StringSplitC(const char* pszString, const char* pszFlag, std::vector<char*>& vecRes)
{
   const char* p = strtok((char*)pszString, pszFlag);
   while (p != NULL)
   {
     if(strlen(p) > 0)
     {
        vecRes.push_back((char*)p);
     }

     p = strtok(NULL, pszFlag);
   }
}


/*
 * @brief	: csv格式文件(默认用双引号包括数据, 用逗号分隔数据)
 * @param	:
 * @return	:
 */
void GenerateSqlLoaderData(std::vector<stFieldHead> vecFieldHead, char *pcszContent)
{
    int iColumnLen = 0;
    char szSql[2048] = {0};
    char chBuff[2] = {0};
    char buffer[1024] = {0};
    char szTempBuff[128] = {0};
    string strTmp;

    g_count++;

    /*
    if (GetConfigValue(strSqlFileCnt, "SqlFileCnt") != RET_OK)
    {
        printf("Get Config \"SqlFileCnt\" Failed!");
        abort();
    }
    if (GetConfigValue(strTable, "TableName") != RET_OK)
    {
        printf("Get Config \"TableName\" Failed!\n");
        abort();
    }

    if (GetConfigValue(strColumns, "Columns") != RET_OK)
    {
        printf("Get Config \"Columns\" Failed!\n");
        abort();
    }
    */

    // 每n条数据输出到一个文件
    // XXX 这里有点问题, 第一个文件会比 MaxCnt 小1
    /*
    if (g_count % atoi(strSqlFileCnt.c_str()) == 0)
    {
        g_count = 0;
        fclose(pf);
        fileNo++;
        pf = fopen(GetFileName(), "w+");
        assert(pf);
    }
    */

    // 组装sql语句  --  根据m_vecFieldHead从pcszContent提取记录数据


    // snprintf(szSql, sizeof(szSql), "insert into %s values(", strTable.c_str());

    for (int i = 0; i < vecFieldHead.size(); i++)
    {
        if (vecFieldHead[i].szName[0] != 0x00 && vecFieldHead[i].szName[0] != '\r')
        {
            // sprintf(chBuff, "%d", i + 1);
            snprintf(chBuff, sizeof(chBuff), "%d", i + 1);
            memcpy(&iColumnLen, vecFieldHead[i].szLen, 1);
            memset(buffer, 0x00, sizeof(buffer));

            // 按照需要指定需要插入的列
            if ((strstr(strColumns.c_str(), chBuff) != NULL) || strcmp(strColumns.c_str(), "@") == 0)
            {
                memcpy(buffer, pcszContent, iColumnLen); // 按照字段长度拷贝内存
                strTmp = string(buffer);
                trim(strTmp);
            }
            else
            {
                strTmp  = "";
            }
            strncat(szSql, "\"", 1);
            snprintf(szTempBuff, sizeof(szTempBuff), "%s", strTmp.c_str());
            strncat(szSql, szTempBuff, sizeof(szSql) - strlen(szSql) - 1);
            if (i < vecFieldHead.size() - 1)
            {
                strncat(szSql, "\",", 2);
            }
            else
            {
                strncat(szSql, "\"", 1);
            }
        }
        pcszContent += iColumnLen; // 跳到下一个字段
    }

    // fprintf(pf, "%s\n", szSql);
    // fwrite(szSql.c_str(), strlen(szSql.c_str()), 1, pf);
    fwrite(szSql, strlen(szSql), 1, pf);
    // fwrite(";\n", strlen(";\n"), 1, pf);
    fwrite("\n", strlen("\n"), 1, pf);
    // fflush(pf);
    vecColumns = vecFieldHead;
}

// void GenSql(const char* pcszContent)
void GenSql(std::vector<stFieldHead> vecFieldHead, char *pcszContent)
{
	std::vector<std::string> vecFieldVal;
	char* pos = (char*)pcszContent;
    // std::vector<stFieldHead> stFieldArr = pDbfR->GetFieldArray();
    std::vector<stFieldHead> stFieldArr = vecFieldHead;


    // FIXME 在里面创建文件非常慢
    g_count++;

    /*
    if (GetConfigValue(strValue, "SqlFileCnt") != RET_OK)
    {
        printf("Get Config \"SqlFileCnt\" Failed!");
        abort();
    }
    if (GetConfigValue(strTable, "TableName") != RET_OK)
    {
        printf("Get Config \"TableName\" Failed!\n");
        abort();
    }
    */
    if (g_count % atoi(strSqlFileCnt.c_str()) == 0)
    {
        g_count = 0;
        fclose(pf);
        fileNo++;
        pf = fopen(GetFileName(), "w+");
        assert(pf);
    }

	std::vector<stFieldHead>::iterator ite = stFieldArr.begin();
	for(; ite != stFieldArr.end(); ite++)
	{
		char chLen;
		memcpy(&chLen, (*ite).szLen, 1);

		char szTmp[1024] = {0};
		strncpy(szTmp, pos, (int)chLen);
		pos = pos + (int)chLen;

		vecFieldVal.push_back(szTmp);
	}

    std::string strSql = "insert into " + strTable + " values(";
	std::vector<char*>::iterator itf = vecRes.begin();
	for(; itf != vecRes.end(); itf++)
	{
		int index = atoi(*itf);
		std::string strTmp = vecFieldVal[index-1];
		trim(strTmp);

		if(itf != vecRes.end() - 1)
		{
			strSql = strSql + "'";
			strSql = strSql + strTmp;
			strSql = strSql + "', ";
		}
		else
		{
			strSql = strSql + "'";
			strSql = strSql + strTmp;
			strSql = strSql + "'";
		}
	}

	strSql = strSql + ")";

	fwrite(strSql.c_str(), strlen(strSql.c_str()), 1, pf);
    fwrite("\n", strlen("\n"), 1, pf);
}

void ReadDbf(std::vector<stFieldHead> vecFieldHead, char *pcszContent)
{
	if(g_count >= 100000)
	{
        g_count = 0;
		fclose(pf);
		fileNo++;
		pf = fopen(GetFileName(), "w+");
		assert(pf);
	}

    // GenerateSql(vecFieldHead,  pcszContent);
    // GenSql(vecFieldHead,  pcszContent);

	g_count++;
}

// 连接数据库
/*
// TODO 加密 肯定不能明文放在配置文件啊
int ConnectOracle(void)
{
    string strDbName;
    string strDbUserName;
    string strDbPwd;

    if (GetConfigValue(strDbName, "Db_Name") != RET_OK || GetConfigValue(strDbUserName, "Db_UserName") != RET_OK || GetConfigValue(strDbPwd, "Db_Pwd") != RET_OK)
    {
        printf("Get Oracle Connect Config Failed!");
        abort();
    }

    int ret = g_oDaoBaseHandler.Connect(strDbUserName.c_str(), strDbPwd.c_str(), strDbName.c_str());
    return ret;
}


// 执行sql语句
void SqlCommit(int iCommitCnt, const char* pcszSql)
{
    int iRetCode;
    iRetCode = g_oDaoDynamicSqlHandler.StmtExecute(pcszSql);
    if (0 != iRetCode)
    {
        printf("insert failed! iRetCode:%d\n", iRetCode);
        fprintf(fpInsert, "%s\n", pcszSql);
        fflush(fpInsert);
    }

    // 这个全局变量没啥用, 多进程下各自有各自的 g_nCommitCcount
    // TODO 要想缩短时间的话, 不能一条条commit, 现在要打印出来看看commit了多少次
    g_nCommitCount++;
    if (g_nCommitCount % iCommitCnt == 0)
    {
        if ((iRetCode = g_oDaoDynamicSqlHandler.StmtExecute("commit")) != 0)
        {
            printf("Commit to Db Failed!");
            abort();
        }
    }

}

// 读入strFile文件行交给pf函数处理
void ReadFile(const std::string &strFile, int iCommitCnt, pLineCallback pf)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    size_t read;

    fp = fopen(strFile.c_str(), "r");
    assert(fp);

    while ((read = getline(&line, &len, fp)) != -1)
    {
        pf(iCommitCnt, line);
    }

    if (line)
    {
        free(line);
    }
}
*/

// 删除文件夹下所有文件
void DeleteAllFile(const char* dirPath, const char *extenStr="sql")
{
    DIR *dir  =  opendir(dirPath);
    string strSplit  =  "/";
    dirent *pDirent = NULL;
    while((pDirent = readdir(dir)) != NULL)
    {
        if(strstr(pDirent->d_name, extenStr))
        {
            string FilePath= dirPath + strSplit + string(pDirent->d_name);
            remove(FilePath.c_str());
        }
    }
    closedir(dir);
}


// 获取文件夹下所有extenStr后缀的文件名列表
void GetAllFile(const char *dirPath, vector<string> &vecFileList, const char *extenStr = "sql")
{
    DIR *dir = opendir(dirPath);
    string strSplit  =  "/";
    dirent *pDirent = NULL;

    while ((pDirent = readdir(dir)) != NULL)
    {
        if (strstr(pDirent->d_name, extenStr))
        {
            string pathFile = dirPath + strSplit  +  string(pDirent->d_name);
            vecFileList.push_back(pathFile);
        }
    }
    closedir(dir);
}

// main函数命令
int GeneraCommand(string strFilePath)
{
    int iRetCode;
    string strSqlFileFolder;
    string strColumns;
    if (GetConfigValue(strSqlFileFolder, "SqlFileFolder") != RET_OK)
    {
        printf("Get Config \"SqlFilePath\" Failed!\n");
        abort();
    }
    DeleteAllFile(strSqlFileFolder.c_str());

    pf = fopen(GetFileName(), "w+");

    if (!pf)
    {
        return -1;
    }
    assert(pf);

    if (GetConfigValue(strColumns, "Columns") != RET_OK)
    {
        printf("Get Config \"Columns\" Failed!\n");
        abort();
    }

    StringSplitC(strColumns.c_str(), ",", vecRes);

    CDbfRead dbf(strFilePath);

    // 本打算根据dbf信息自动建表, 不太好, 因为多次执行的时候会冲突
    dbf.ReadHead();

    // dbf.Read(GenerateSql);
    dbf.Read(GenerateSqlLoaderData);
    // dbf.Read(GenSql);
    // dbf.Read(ReadDbf);

    fflush(pf);
    fclose(pf);
    return 0;
}


void SetDate(unsigned char* date)
{
	time_t now;
	time(&now);
	tm* tp = localtime(&now);

	date[0] = tp->tm_year % 100;
	date[1] = tp->tm_mon + 1;
	date[2] = tp->tm_mday;
}

int Csv2DbfCommand(string strFilePath)
{
    int iRetCode;
    int iTmp;
    int iOffset = 1;
    string strTmp;
    stFieldHead stFieldTmp;
    vector<string> vecDbfColumns;
    std::vector<stFieldHead> vecFieldHead;

    iRetCode = ini.getValues("DBF", "FIELD", vecDbfColumns);
    if(iRetCode != 0)
    {
        printf("Get Config \"FIELD\" Failed!\n");
        abort();
    }

    // 解析配置中的字段属性组装成 stFieldHead 结构体
    for (auto x : vecDbfColumns)
    {
        memset(&stFieldTmp, 0, sizeof(struct stFieldHead));
        if (x.find("NAME:") != string::npos)
        {
            strTmp = GetMsgValue(x, "NAME", ",");
            memcpy(stFieldTmp.szName, strTmp.c_str(), strTmp.length());
        }
        if (x.find("TYPE:") != string::npos)
        {
            strTmp = GetMsgValue(x, "TYPE", ",");
            memcpy(stFieldTmp.szType, strTmp.c_str(), strTmp.length());
        }
        if (x.find("OFFSET:") != string::npos)
        {
            strTmp = GetMsgValue(x, "OFFSET", ",");
            memcpy(stFieldTmp.szOffset, &strTmp, sizeof(stFieldTmp.szOffset));
        }
        if (x.find("LEN:") != string::npos)
        {
            iTmp = atoi(GetMsgValue(x, "LEN", ",").c_str());
            memcpy(stFieldTmp.szLen, &iTmp, sizeof(stFieldTmp.szLen));
            memcpy(stFieldTmp.szOffset, &iOffset, sizeof(stFieldTmp.szOffset));
            iOffset += iTmp;
        }
        if (x.find("PRECISION:") != string::npos)
        {
            iTmp = atoi(GetMsgValue(x, "PRECISION", ",").c_str());
            memcpy(stFieldTmp.szPrecision, &iTmp, strTmp.length());
        }
        if (x.find("RESV2:") != string::npos)
        {
            strTmp =  GetMsgValue(x, "RESV2",",");
            memcpy(stFieldTmp.szResv2, strTmp.c_str(), strTmp.length());
        }
        if (x.find("ID:") != string::npos)
        {
            strTmp =  GetMsgValue(x, "ID",",");
            memcpy(stFieldTmp.szId, strTmp.c_str(), strTmp.length());
        }
        if (x.find("RESV3:") != string::npos)
        {
            strTmp =  GetMsgValue(x, "RESV3",",");
            memcpy(stFieldTmp.szResv3, strTmp.c_str(), strTmp.length());
        }
        vecFieldHead.push_back(stFieldTmp);
    }

    CDbfRead dbf;
    dbf.AddHead(vecFieldHead);
    string s1[5] = {"1", "Ric G", "210.123456789123456", "43", "T"};
    string s2[5] = {"1000", "Paul F", "196.2", "33", "T"};
    dbf.AppendRec(s1);
    dbf.AppendRec(s2);

    return 0;
}

// main函数命令
/*
int RunSqlCommand()
{
    int iRetCode;
    int iCommitCnt;
    string strCommitValue;
    string strSqlFileFolder;
    string strLogFile;
    vector<string> vecFileList;

    if (GetConfigValue(strSqlFileFolder, "SqlFileFolder") != RET_OK)
    {
        printf("Get Config \"SqlFilePath\" Failed!\n");
        abort();
    }

    if (GetConfigValue(strCommitValue, "MaxCommitCnt") != RET_OK)
    {
        printf("Get Config \"MaxCommitCnt\" Failed!\n");
        abort();
    }
    iCommitCnt  =  atoi(strCommitValue.c_str());

    if (GetConfigValue(strLogFile, "LogFile") != RET_OK)
    {
        printf("Get Config \"LogFile\" Failed!\n");
        abort();
    }

    fpInsert = fopen(strLogFile.c_str(), "w+");

    if (!fpInsert)
    {
        return -1;
    }

    GetAllFile(strSqlFileFolder.c_str(), vecFileList);

    // 单进程:
    // begin
    /*
    if ((iRetCode = ConnectOracle()) != RET_OK)
    {
        printf("Fail to connect Oracle, ErrCode:%d", iRetCode);
        return iRetCode;
    }
    for(int i = 0; i < vecFileList.size(); i++ )
    {
        ReadFile(vecFileList[i].c_str(), iCommitCnt, SqlCommit);
    }
    // 少于iCommitCnt的和大于iCommitCnt的余数部分在这里提交
    if (g_nCommitCount <  iCommitCnt || g_nCommitCount / iCommitCnt >0)
    {
        if ((iRetCode = g_oDaoDynamicSqlHandler.StmtExecute("commit")) != 0)
        {
            printf("Commit to Db Failed!");
            return iRetCode;
        }
        g_nCommitCount = 0;
    }
    *
    // end

    // TODO 多进程, 每个进程分别调用ReadFile处理一个文件
    // begin
    int status, i;
    for(i = 0; i < vecFileList.size(); i++ )
    {
        status = fork();
        // 确保所有子进程都是由父进程fork出来的
        if(status == 0 || status == -1)
        {
            break;
        }
    }
    if(status == -1)
    {
        printf("error on fork");
    }
    else if(status == 0) // 每个子进程都会运行的代码
    {
        // sub process
        // 多进程如果共用一个连接会导致锁表
        if ((iRetCode = ConnectOracle()) != RET_OK)
        {
            printf("Fail to connect Oracle, ErrCode:%d", iRetCode);
            return iRetCode;
        }

        ReadFile(vecFileList[i].c_str(), iCommitCnt, SqlCommit);

        // 少于iCommitCnt的和大于iCommitCnt的余数部分在这里提交
        if (g_nCommitCount <  iCommitCnt || g_nCommitCount / iCommitCnt >0)
        {
            if ((iRetCode = g_oDaoDynamicSqlHandler.StmtExecute("commit")) != 0)
            {
                printf("Commit to Db Failed!");
                return iRetCode;
            }
            g_nCommitCount = 0;
        }
        exit(0);
    }
    else
    {
        // parent process
        printf("par process:%d\t%d\t\n", getpid(), i);
    }
    // end

    fclose(fpInsert);
    // fflush(fpInsert);
    return 0;
}
*/

/*----------------------------------------------------------------------------
导入文本
为了批量导入，在此我调用的sqlldr工具
首先生成SQL*Loader控制文件，后运行sqlldr
----------------------------------------------------------------------------*/
/*
 * @brief	: 利用 sqlldr 导入文本到 oracle
 * @param   : vecFieldHead  字段vector
 * @param	: TableName     导入目标表
 * @param	: FileName      需要导入的文件
 * @param	: strTok        文件字段间分隔符
 * @param	: strScope      字段值的包含, 比如用双引号括住
 * @return	:
 */
int ImportDB(vector<stFieldHead> vecFieldHead, string TableName, string FileName, string strTok, string strSplit)
{
    // 产生SQL*Loader控制文件
    FILE *fctl;
    FILE *fp;
    int iStart = 1;
    char Execommand[256];
    string sqlload = "./sql/sqlload.ctl";

    if ((fctl = fopen(sqlload.c_str(), "w")) == NULL)
    {
        return -1 ;
    }
    // fprintf(fctl, "OPTIONS(rows=128)\n");
    fprintf(fctl, "LOAD DATA\n");
    fprintf(fctl, "INFILE '%s'\n", FileName.c_str());
    fprintf(fctl, "APPEND INTO TABLE %s\n", TableName.c_str());
    fprintf(fctl, "FIELDS TERMINATED BY \"%s\"\n", strTok.c_str());   // 用于分割一行中各个属性值的符号
    fprintf(fctl, "Optionally enclosed by '%s'\n", strSplit.c_str()); // 数据中每个字段用 '"' 框起
    fprintf(fctl, "TRAILING NULLCOLS\n");                             // 表的字段没有对应的值时允 许为空
    fprintf(fctl, "(\n");

    for (int i = 0; i < vecFieldHead.size(); i++)
    {
        if (vecFieldHead[i].szName[0] != 0x00 && vecFieldHead[i].szName[0] != '\r')
        {
            // fprintf(fctl, "%11s POSITION(%d:%d)", vecFieldHead[i].szName, iStart, *(int *)vecFieldHead[i] + iStart - 1);
            // iStart += *(int *)vecFieldHead[i];
            fprintf(fctl, "%11s", vecFieldHead[i].szName);
            if (i < vecFieldHead.size() - 1)
            {
                fprintf(fctl, ",\n");
            }
        }
    }
    fprintf(fctl, "\n)\n");
    fclose(fctl);

    // 执行系统命令
    // sprintf(Execommand, "sqlldr userid=%s/%s@%s control=%s", User, Pwd, DB, sqlload.c_str());
    if (system(Execommand) == -1)
    {
        // SQL*Loader 执行错误
        return -1;
    }
    return 0 ;
}

int main(int argc, char *argv[])
{
    int iRetCode;
    long lBeginStampTimes;
    long lEndStampTimes;

    iRetCode = ini.load("./config.ini");
    if (iRetCode != RET_OK)
    {
        return RET_ERR;
    }

    if (2 != argc && 3 != argc)
    {
        printf("param error!\n");
        printf("Generate SqlFile:\t[exec] gene [path/to/dbf]\n");
        printf("Run SqlFile:\t[exec] run\n");
        printf("Generate & Run:\t[exec] batch [path/to/dbf]\n");
        return -1;
    }

    lBeginStampTimes = time(NULL);
    if (strcmp(argv[1], "gene") == 0 || strcmp(argv[1], "batch") == 0)
    {
        if (GeneraCommand(argv[2]) != 0)
        {
            printf("Error while generate sql file");
        }
    }
    if (strcmp(argv[1], "sqlldr") == 0)
    {
        if (ImportDB(vecColumns, "tmp_data_check_rst", "data.dat", ",", "\""))
        {
            printf("Error while generate sql control file");
        }
    }
    // csv转dbf
    if (strcmp(argv[1], "2dbf") == 0 )
    {
        if (Csv2DbfCommand(argv[2]) != 0)
        {
            printf("\nError while run sql, errorcode:%d", iRetCode);
        }
    }

    lEndStampTimes = time(NULL);

    printf("Consume: %lds\n", (lEndStampTimes - lBeginStampTimes));

    return 0;
}
