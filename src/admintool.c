#include <stdio.h>
#include "bbs.h"
#include "fbbs/board.h"
#include "fbbs/fbbs.h"
#include "fbbs/register.h"
#include "fbbs/status.h"
#include "fbbs/string.h"
#include "fbbs/terminal.h"

extern int cmpbnames();
extern int dowall();
extern int t_cmpuids();
extern void rebuild_brdshm();
int showperminfo(int, int);
char cexplain[STRLEN];
char buf2[STRLEN];
char lookgrp[30];
char bnames[3][STRLEN]; //存放用户担任版主的版名,最多为三
FILE *cleanlog;

//在userid的主目录下 打开.bmfile文件,并将里面的内容与bname相比较
//              find存放从1开始返回所任版面的序号,为0表示没找到
//函数的返回值为userid担任版主的版面数
static int getbnames(const char *userid, const char *bname, int *find)
{
	int oldbm = 0;
	FILE *bmfp;
	char bmfilename[STRLEN], tmp[20];
	*find = 0;
	sethomefile(bmfilename, userid, ".bmfile");
	bmfp = fopen(bmfilename, "r");
	if (!bmfp) {
		return 0;
	}
	for (oldbm = 0; oldbm < 3;) {
		fscanf(bmfp, "%s\n", tmp);
		if (!strcmp(bname, tmp)) {
			*find = oldbm + 1;
		}
		strcpy(bnames[oldbm++], tmp);
		if (feof(bmfp)) {
			break;
		}
	}
	fclose(bmfp);
	return oldbm;
}

static int get_grp(char *seekstr)
{
	FILE   *fp;
	char    buf[STRLEN];
	char   *namep;
	if ((fp = fopen("0Announce/.Search", "r")) == NULL)
		return 0;
	while (fgets(buf, STRLEN, fp) != NULL) {
		namep = strtok(buf, ": \n\r\t");
		if (namep != NULL && strcasecmp(namep, seekstr) == 0) {
			fclose(fp);
			strtok(NULL, "/");
			namep = strtok(NULL, "/");
			if (strlen(namep) < 30) {
				strcpy(lookgrp, namep);
				return 1;
			} else
				return 0;
		}
	}
	fclose(fp);
	return 0;
}

//      修改使用者资料
int m_info() {
	struct userec uinfo;
	char reportbuf[30];
	int id;

	if (!(HAS_PERM(PERM_USER)))
		return 0;

	set_user_status(ST_ADMIN);
	if (!check_systempasswd()) {
		return 0;
	}
	clear();
	stand_title("修改使用者资料");
	if (!gettheuserid(1, "请输入使用者代号: ", &id))
		return -1;
	memcpy(&uinfo, &lookupuser, sizeof (uinfo));
	sprintf(reportbuf, "check info: %s", uinfo.userid);
	report(reportbuf, currentuser.userid);

	move(1, 0);
	clrtobot();
	disply_userinfo(&uinfo);
	uinfo_query(&uinfo, 1, id);
	return 0;
}

static const char *ordain_bm_check(const board_t *board, const char *uname)
{
	if (strneq(board->bms, "SYSOPs", 6))
		return "讨论区的版主是 SYSOPs 你不能再任命版主";
	if (strlen(uname) + strlen(board->bms) > BMNAMEMAXLEN)
		return "讨论区版主列表太长,无法加入!";
	if (streq(uname, "guest"))
		return "你不能任命 guest 当版主";

	int find;
	int bms = getbnames(lookupuser.userid, board->name, &find);
	if (find || bms >= 3)
		return "已经是该/三个版的版主了";

	bms = 1;
	for (const char *s = board->bms; *s; ++s) {
		if (*s == ' ')
			++bms;
	}
	if (bms >= BMMAXNUM)
		return "讨论区已有 5 名版主";

	return NULL;
}

static bool ordain_bm(int bid, const char *uname)
{
	user_id_t uid = get_user_id(uname);
	if (uid <= 0)
		return false;

	db_res_t *res = db_cmd("INSERT INTO bms b (user_id, board_id, stamp) "
			"VALUES (%d, %d, current_timestamp) ", uid, bid);
	db_clear(res);
	return res;
}

int tui_ordain_bm(const char *cmd)
{
	if (!(HAS_PERM(PERM_USER)))
		return 0;

	set_user_status(ST_ADMIN);
	if (!check_systempasswd())
		return 0;

	clear();
	stand_title("任命版主\n");
	clrtoeol();

	int id;
	if (!gettheuserid(2, "输入欲任命的使用者帐号: ", &id))
		return 0;

	char bname[BOARD_NAME_LEN];
	board_t board;
	board_complete(3, "输入该使用者将管理的讨论区名称: ", bname, sizeof(bname),
			AC_LIST_BOARDS_ONLY);
	if (!*bname || !get_board(bname, &board))
		return -1;
	board_to_gbk(&board);

	const char *error = ordain_bm_check(&board, lookupuser.userid);
	if (error) {
		move(5, 0);
		outs(error);
		pressanykey();
		clear();
		return -1;
	}

	bool bm1 = !board.bms[0];
	const char *bm_s = bm1 ? "主" : "副";
	prints("\n你将任命 %s 为 %s 版版%s.\n", lookupuser.userid, bname, bm_s);
	if (askyn("你确定要任命吗?", NA, NA) == NA) {
		prints("取消任命版主");
		pressanykey();
		clear();
		return -1;
	}

	if (!ordain_bm(board.id, lookupuser.userid)) {
		prints("Error");
		pressanykey();
		clear();
		return -1;
	}

	if (!HAS_PERM2(PERM_BOARDS, &lookupuser)) {
		lookupuser.userlevel |= PERM_BOARDS;
		substitut_record(PASSFILE, &lookupuser, sizeof(struct userec), id);

		char buf[STRLEN];
		snprintf(buf, sizeof(buf), "版主任命, 给予 %s 版主权限",
				lookupuser.userid);
		securityreport(buf, 0, 1);
		move(15, 0);
		outs(buf);
		pressanykey();
		clear();
	}

	char old_descr[STRLEN];
	snprintf(old_descr, sizeof(old_descr), "○ %s", board.descr);

	//sprintf(genbuf, "%-38.38s(BM: %s)", fh.title +8, fh.BM);
	//精华区的显示: 动态分配        显示10个空格 printf("%*c",10,' ');
	{
		int blanklen; //前两个空间大小
		static const char BLANK = ' ';
		blanklen = STRLEN - strlen(old_descr) - strlen(board.bms) - 7;
		blanklen /= 2;
		blanklen = (blanklen > 0) ? blanklen : 1;
		sprintf(genbuf, "%s%*c(BM: %s)",
				old_descr, blanklen, BLANK, board.bms);
	}

	get_grp(board.name);
	edit_grp(board.name, lookgrp, old_descr, genbuf);

	char file[HOMELEN];
	sethomefile(file, lookupuser.userid, ".bmfile");
	FILE *fp = fopen(file, "a");
	fprintf(fp, "%s\n", lookupuser.userid);
	fclose(fp);

	/* Modified by Amigo 2002.07.01. Add reference to BM-Guide. */
	//sprintf (genbuf, "\n\t\t\t【 通告 】\n\n"
	//	   "\t任命 %s 为 %s 版%s！\n"
	//	   "\t欢迎 %s 前往 BM_Home 版和本区 Zone 版向大家问好。\n"
	//	   "\t开始工作前，请先通读BM_Home版精华区的版主指南目录。\n",
	//	   lookupuser.userid, bname, bm ? "版主" : "版副", lookupuser.userid);

	//the new version add by Danielfree 06.11.12
	sprintf(
			genbuf,
			"\n"
			" 		[1;31m   ╔═╗╔═╗╔═╗╔═╗										 [m\n"
			" 	 [31m╋──[1m║[33m日[31m║║[33m月[31m║║[33m光[31m║║[33m华[31m║[0;33m──[1;36m〖领会站规精神·熟悉版主操作〗[0;33m─◇◆  [m\n"
			" 	 [31m│    [1m╚═╝╚═╝╚═╝╚═╝										  [m\n"
			" 	 [31m│																	  [m\n"
			" 		 [1;33m︻	[37m任命  %s  为  %s  版版%s。							   [m\n"
			" 		 [1;33m通																  [m\n"
			" 		[1m	欢迎  %s  前往 BM_Home 版和本区 Zone 版向大家问好。			 [m\n"
			" 		 [1;33m告																  [m\n"
			" 		 [1;33m︼	[37m开始工作前，请先通读BM_Home版精华区的版主指南目录。		   [m\n"
			" 																		 [33m│  [m\n"
			" 											 [1;33m╔═╗╔═╗╔═╗╔═╗   [0;33m │  [m\n"
			" 	 [31m◇◆─[1;35m〖维护版面秩序·建设和谐光华〗[0;31m──[1;33m║[31m版[33m║║[31m主[33m║║[31m委[33m║║[31m任[33m║[0;33m──╋	[m\n"
			" 											 [1;33m╚═╝╚═╝╚═╝╚═╝		  [m\n"
			" 																			 [m\n", lookupuser.userid, bname,
			bm_s, lookupuser.userid);
	//add end

	char ps[5][STRLEN];
	move(8, 0);
	prints("请输入任命附言(最多五行，按 Enter 结束)");
	for (int i = 0; i < 5; i++) {
		getdata(i + 9, 0, ": ", ps[i], STRLEN - 5, DOECHO, YEA);
		if (ps[i][0] == '\0')
			break;
	}
	for (int i = 0; i < 5; i++) {
		if (ps[i][0] == '\0')
			break;
		if (i == 0)
			strcat(genbuf, "\n\n");
		strcat(genbuf, "\t");
		strcat(genbuf, ps[i]);
		strcat(genbuf, "\n");
	}

	char buf[STRLEN];
	strcpy(currboard, bname);
	snprintf(buf, sizeof(buf), "任命 %s 为 %s 版版%s", lookupuser.userid,
			board.name, bm_s);
	autoreport(buf, genbuf, YEA, lookupuser.userid, 1);
#ifdef ORDAINBM_POST_BOARDNAME
	strcpy(currboard, ORDAINBM_POST_BOARDNAME);
	autoreport(buf, genbuf, YEA, lookupuser.userid, 1);
#endif
	securityreport(buf, 0, 1);
	move(16, 0);
	outs(buf);
	pressanykey();
	return 0;
}

static bool retire_bm(int bid, const char *uname)
{
	db_res_t *res = db_cmd("DELETE FROM bms b USING users u "
			"WHERE b.user_id = u.id AND b.board_id = %d AND u.name = %s",
			bid, uname);
	db_clear(res);
	return res;
}

int tui_retire_bm(const char *cmd)
{
	int id, right = 0, j = 0, bmnum;
	int find, bm = 1;
	FILE *bmfp;
	char bmfilename[STRLEN], usernames[BMMAXNUM][STRLEN];

	if (!(HAS_PERM(PERM_USER)))
		return 0;

	set_user_status(ST_ADMIN);
	if (!check_systempasswd())
		return 0;

	clear();
	stand_title("版主离职\n");
	clrtoeol();
	if (!gettheuserid(2, "输入欲离职的版主帐号: ", &id))
		return -1;

	char bname[BOARD_NAME_LEN];
	board_t board;
	board_complete(3, "请输入该版主要辞去的版名: ", bname, sizeof(bname),
			AC_LIST_BOARDS_ONLY);
	if (!*bname || !get_board(bname, &board))
		return -1;
	board_to_gbk(&board);

	int oldbm = getbnames(lookupuser.userid, bname, &find);
	if (!oldbm || !find) {
		move(5, 0);
		prints(" %s %s版版主，如有错误，请通知程序站长。", lookupuser.userid,
				(oldbm) ? "不是该" : "没有担任任何");
		pressanykey();
		clear();
		return -1;
	}
	for (int i = find - 1; i < oldbm; i++) {
		if (i != oldbm - 1)
			strcpy(bnames[i], bnames[i + 1]);
	}
	bmnum = 0;
	for (int i = 0; board.bms[i] != '\0'; i++) {
		if (board.bms[i] == ' ') {
			usernames[bmnum][j] = '\0';
			bmnum++;
			j = 0;
		} else {
			usernames[bmnum][j++] = board.bms[i];
		}
	}
	usernames[bmnum++][j] = '\0';
	for (int i = 0; i < bmnum; i++) {
		if (!strcmp(usernames[i], lookupuser.userid)) {
			right = 1;
			if (i)
				bm = 0;
		}
		if (right && i != bmnum - 1) //while(right&&i<bmnum)似乎更好一些;infotech
			strcpy(usernames[i], usernames[i + 1]);
	}
	if (!right) {
		move(5, 0);
		prints("对不起， %s 版版主名单中没有 %s ，如有错误，请通知技术站长。", bname,
				lookupuser.userid);
		pressanykey();
		clear();
		return -1;
	}
	prints("\n你将取消 %s 的 %s 版版%s职务.\n", lookupuser.userid, bname, bm ? "主"
			: "副");
	if (askyn("你确定要取消他的该版版主职务吗?", NA, NA) == NA) {
		prints("\n呵呵，你改变心意了？ %s 继续留任 %s 版版主职务！", lookupuser.userid, bname);
		pressanykey();
		clear();
		return -1;
	}

	retire_bm(board.id, lookupuser.userid);

	char old_descr[STRLEN];
	snprintf(old_descr, sizeof(old_descr), "○ %s", board.descr);

	if (!streq(board.bms, lookupuser.userid)) {
		//sprintf(genbuf, "%-38.38s(BM: %s)", fh.title +8, fh.BM);
		//精华区的显示: 动态分配        显示10个空格 printf("%*c",10,' ');
		{
			int blanklen; //前两个空间大小
			static const char BLANK = ' ';
			blanklen = STRLEN - strlen(old_descr) - strlen(board.bms) - 7;
			blanklen /= 2;
			blanklen = (blanklen > 0) ? blanklen : 1;
			sprintf(genbuf, "%s%*c(BM: %s)", old_descr, blanklen,
					BLANK, board.bms);
		}
	} else {
		sprintf(genbuf, "%-38.38s", old_descr);
	}
	get_grp(board.name);
	edit_grp(board.name, lookgrp, old_descr, genbuf);
	sprintf(genbuf, "取消 %s 的 %s 版版主职务", lookupuser.userid, board.name);
	securityreport(genbuf, 0, 1);
	move(8, 0);
	outs(genbuf);
	sethomefile(bmfilename, lookupuser.userid, ".bmfile");
	if (oldbm - 1) {
		bmfp = fopen(bmfilename, "w+");
		for (int i = 0; i < oldbm - 1; i++)
			fprintf(bmfp, "%s\n", bnames[i]);
		fclose(bmfp);
	} else {
		char secu[STRLEN];

		unlink(bmfilename);
		if (!(lookupuser.userlevel & PERM_OBOARDS) //没有讨论区管理权限
				&& !(lookupuser.userlevel & PERM_SYSOPS) //没有站务权限
		) {
			lookupuser.userlevel &= ~PERM_BOARDS;
			substitut_record(PASSFILE, &lookupuser, sizeof(struct userec),
					id);
			sprintf(secu, "版主卸职, 取消 %s 的版主权限", lookupuser.userid);
			securityreport(secu, 0, 1);
			move(9, 0);
			outs(secu);
		}
	}
	prints("\n\n");
	if (askyn("需要在相关版面发送通告吗?", YEA, NA) == NA) {
		pressanykey();
		return 0;
	}
	prints("\n");
	if (askyn("正常离任请按 Enter 键确认，撤职惩罚按 N 键", YEA, NA) == YEA)
		right = 1;
	else
		right = 0;
	if (right)
		sprintf(bmfilename, "%s 版%s %s 离任通告", bname, bm ? "版主" : "版副",
				lookupuser.userid);
	else
		sprintf(bmfilename, "[通告]撤除 %s 版%s %s", bname, bm ? "版主" : "版副",
				lookupuser.userid);
	strcpy(currboard, bname);
	if (right) {
		sprintf(genbuf, "\n\t\t\t【 通告 】\n\n"
			"\t经站务委员会讨论：\n"
			"\t同意 %s 辞去 %s 版的%s职务。\n"
			"\t在此，对其曾经在 %s 版的辛苦劳作表示感谢。\n\n"
			"\t希望今后也能支持本版的工作.\n", lookupuser.userid, bname, bm ? "版主"
				: "版副", bname);
	} else {
		sprintf(genbuf, "\n\t\t\t【撤职通告】\n\n"
			"\t经站务委员会讨论决定：\n"
			"\t撤除 %s 版%s %s 的%s职务。\n", bname, bm ? "版主" : "版副",
				lookupuser.userid, bm ? "版主" : "版副");
	}

	char buf[5][STRLEN];
	for (int i = 0; i < 5; i++)
		buf[i][0] = '\0';
	move(14, 0);
	prints("请输入%s附言(最多五行，按 Enter 结束)", right ? "版主离任" : "版主撤职");
	for (int i = 0; i < 5; i++) {
		getdata(i + 15, 0, ": ", buf[i], STRLEN - 5, DOECHO, YEA);
		if (buf[i][0] == '\0')
			break;
		//      if(i == 0) strcat(genbuf,right?"\n\n离任附言：\n":"\n\n撤职说明：\n");
		if (i == 0)
			strcat(genbuf, "\n\n");
		strcat(genbuf, "\t");
		strcat(genbuf, buf[i]);
		strcat(genbuf, "\n");
	}
	autoreport(bmfilename, genbuf, YEA, lookupuser.userid, 1);

	prints("\n执行完毕！");
	pressanykey();
	return 0;
}

static bool valid_board_name(const char *name)
{
	for (const char *s = name; *s; ++s) {
		char ch = *s;
		if (!isalnum(ch) && ch != '_' && ch != '.')
			return false;
	}
	return true;
}

static int select_section(void)
{
	int id = 0;
	char buf[3];
	getdata(5, 0, "请输入分区: ", buf, sizeof(buf), DOECHO, YEA);
	if (*buf) {
		db_res_t *res = db_query("SELECT id FROM board_sectors "
				"WHERE lower(name) = lower(%s)", buf);
		if (res && db_res_rows(res) == 1)
			id = db_get_integer(res, 0, 0);
	}
	return id;
}

const char *chgrp(void)
{
	const char *explain[] = {
		"BBS 系统", "复旦大学", "院系风采", "电脑技术", "休闲娱乐", "文学艺术",
		"体育健身", "感性空间", "新闻信息", "学科学术", "音乐影视", "交易专区",
		"隐藏分区", NULL
	};
	const char *groups[] = {
        "system.faq", "campus.faq", "ccu.faq", "comp.faq", "rec.faq",
		"literal.faq", "sport.faq", "talk.faq", "news.faq", "sci.faq",
		"other.faq", "business.faq", "hide.faq", NULL
	};

	clear();
	move(2, 0);
	prints("选择精华区的目录\n\n");

	int i, ch;
	for (i = 0; ; ++i) {
		if (!explain[i] || !groups[i])
			break;
		prints("\033[1;32m%2d\033[m. %-20s%-20s\n", i, explain[i], groups[i]);
	}

	char buf[STRLEN], ans[6];
	snprintf(buf, sizeof(buf), "请输入您的选择(0~%d): ", --i);
	while (1) {
		getdata(i + 6, 0, buf, ans, sizeof(ans), DOECHO, YEA);
		if (!isdigit(ans[0]))
			continue;
		ch = atoi(ans);
		if (ch < 0 || ch > i || ans[0] == '\r' || ans[0] == '\0')
			continue;
		else
			break;
	}
	snprintf(cexplain, sizeof(cexplain), "%s", explain[ch]);

	return groups[ch];
}

int tui_new_board(const char *cmd)
{
	if (!(HAS_PERM(PERM_BLEVELS)))
		return 0;

	set_user_status(ST_ADMIN);
	if (!check_systempasswd()) {
		return 0;
	}

	clear();
	stand_title("开启新讨论区");

	char bname[BOARD_NAME_LEN + 1];
	while (1) {
		getdata(2, 0, "讨论区名称:   ", bname, sizeof(bname), DOECHO, YEA);
		if (*bname) {
			board_t board;
			if (get_board(bname, &board)) {
				prints("\n错误! 此讨论区已经存在!!");
				pressanykey();
				return -1;
			}
		} else {
			return -1;
		}

		if (valid_board_name(bname))
			break;
		prints("\n不合法名称!!");
	}

	GBK_UTF8_BUFFER(descr, BOARD_DESCR_CCHARS);
	getdata(3, 0, "讨论区说明: ", gbk_descr, sizeof(gbk_descr), DOECHO, YEA);
	if (!*gbk_descr)
		return -1;
	convert_g2u(gbk_descr, utf8_descr);

	GBK_UTF8_BUFFER(categ, BOARD_CATEG_CCHARS);
	getdata(4, 0, "讨论区类别: ", gbk_categ, sizeof(gbk_categ), DOECHO, YEA);
	convert_g2u(gbk_categ, utf8_categ);
	
	int sector = select_section();

	char pname[BOARD_NAME_LEN];
	board_complete(6, "输入所属目录: ", pname, sizeof(pname),
			AC_LIST_DIR_ONLY);
	board_t parent;
	get_board(pname, &parent);

	int flag = 0, perm = 0;
	if (askyn("本版是目录吗?", NA, NA)) {
		flag |= (BOARD_DIR_FLAG | BOARD_JUNK_FLAG
				| BOARD_NOREPLY_FLAG | BOARD_POST_FLAG);
		if (askyn("是否限制存取权利?", NA, NA)) {
			char ans[2];
			getdata(7, 0, "限制读? [R]: ", ans, sizeof(ans), DOECHO, YEA);
			move(1, 0);
			clrtobot();
			move(2, 0);
			prints("设定 %s 权利. 讨论区: '%s'\n", "READ", bname);
			perm = setperms(perm, "权限", NUMPERMS, showperminfo);
			clear();
		}
	} else {
		if (askyn("该版的全部文章均不可以回复", NA, NA))
			flag |= BOARD_NOREPLY_FLAG;
		if (askyn("是否是俱乐部版面", NA, NA)) {
			flag |= BOARD_CLUB_FLAG;
			if (askyn("是否读限制俱乐部版面", NA, NA))
				flag |= BOARD_READ_FLAG;
		}
		if (askyn("是否不计算文章数", NA, NA))
			flag |= BOARD_JUNK_FLAG;
		if (askyn("是否为匿名版", NA, NA))
			flag |= BOARD_ANONY_FLAG;
#ifdef ENABLE_PREFIX
		if (askyn ("是否强制使用前缀", NA, NA))
			flag |= BOARD_PREFIX_FLAG;
#endif
		if (askyn("是否限制存取权力", NA, NA)) {
			char ans[2];
			getdata(11, 0, "限制读(R)/写(P)? [R]: ", ans, sizeof(ans),
					DOECHO, YEA);
			if (*ans == 'P' || *ans == 'p')
				flag |= BOARD_POST_FLAG;
			move(1, 0);
			clrtobot();
			move(2, 0);
			prints("设定 %s 权利. 讨论区: '%s'\n",
					(flag & BOARD_POST_FLAG ? "写" : "读"), bname);
			perm = setperms(perm, "权限", NUMPERMS, showperminfo);
			clear();
		}
	}

	db_res_t *res = db_query("INSERT INTO boards "
			"(name, descr, parent, flag, perm, categ, sector) "
			"VALUES (%s, %s, %d, %d, %d, %d, %d, %d) RETURNING id",
			bname, utf8_descr, parent.id, flag, perm, utf8_categ, sector);
	if (!res) {
		prints("\n建立新版出错\n");
		pressanykey();
		clear();
		return -1;
	}
	int bid = db_get_integer(res, 0, 0);
	db_clear(res);

	char *bms = NULL;
	if (!(flag & BOARD_DIR_FLAG)
			&& !askyn("本版诚征版主吗(否则由SYSOPs管理)?", YEA, NA)) {
		bms = "SYSOPs";
		ordain_bm(bid, bms);
	}

	char vdir[HOMELEN];
	snprintf(vdir, sizeof(vdir), "vote/%s", bname);
	char bdir[HOMELEN];
	snprintf(bdir, sizeof(bdir), "boards/%s", bname);
	if (mkdir(bdir, 0755) != 0 || mkdir(vdir, 0755) != 0) {
		prints("\n新建目录出错!\n");
		pressreturn();
		clear();
		return -1;
	}

	if (!(flag & BOARD_DIR_FLAG)) {
		const char *group = chgrp();
		if (group) {
			char buf[STRLEN];
			if (*bms) {
				snprintf(buf, sizeof(buf), "○ %-35.35s(BM: %s)",
						gbk_descr, bms);
			} else {
				snprintf(buf, sizeof(buf), "○ %-35.35s", gbk_descr);
			}
			if (add_grp(group, cexplain, bname, buf) == -1) {
				prints("\n成立精华区失败....\n");
			} else {
				prints("已经置入精华区...\n");
			}
		}
	}

	rebuild_brdshm(); //add by cometcaptor 2006-10-13
	prints("\n新讨论区成立\n");

	char buf[STRLEN];
	snprintf(buf, sizeof(buf), "成立新版：%s", bname);
	securityreport(buf, 0, 1);

	pressreturn();
	clear();
	return 0;
}

static void show_edit_board_menu(board_t *bp, board_t *pp)
{
	prints("1)修改名称:        %s\n", bp->name);
	prints("2)修改说明:        %s\n", bp->descr);
	prints("4)修改所属目录:    %s(%d)\n", pp->name, pp->id);
	if (bp->flag & BOARD_DIR_FLAG) {
		prints("5)修改读写属性:    %s\n",
				(bp->perm == 0) ? "没有限制" : "r(限制阅读)");
	} else {
		prints("5)修改读写属性:    %s\n",
				(bp->flag & BOARD_POST_FLAG) ? "p(限制发文)"
				: (bp->perm == 0) ? "没有限制" : "r(限制阅读)");
	}

	if (!(bp->flag & BOARD_DIR_FLAG)) {
		prints("8)匿名版面:            %s\n",
				(bp->flag & BOARD_ANONY_FLAG) ? "是" : "否");
		prints("9)可以回复:            %s\n",
				(bp->flag & BOARD_NOREPLY_FLAG) ? "否" : "是");
		prints("A)是否计算文章数:      %s\n",
				(bp->flag & BOARD_JUNK_FLAG) ? "否" : "是");
		prints("B)俱乐部属性:          %s\n",
				(bp->flag & BOARD_CLUB_FLAG) ?
				(bp->flag & BOARD_READ_FLAG) ?
				"\033[1;31mc\033[0m(读限制)"
				: "\033[1;33mc\033[0m(写限制)"
				: "非俱乐部");
#ifdef ENABLE_PREFIX
		prints ("C)是否强制使用前缀:    %s\n",
				(bp->flag & BOARD_PREFIX_FLAG) ? "是" : "否");
#endif
	}
}

static bool alter_board_name(board_t *bp)
{
	char bname[BOARD_NAME_LEN + 1];
	getdata(t_lines - 2, 0, "新讨论区名称: ", bname, sizeof(bname),
			DOECHO, YEA);
	if (!*bname || streq(bp->name, bname) || !valid_brdname(bname))
		return 0;

	if (!askyn("确定修改版名?", NA, YEA))
		return 0;

	db_res_t *res = db_cmd("UPDATE boards SET name = %s WHERE id = %d",
			bname, bp->id);
	db_clear(res);
	return res;
}

static bool alter_board_descr(board_t *bp)
{
	GBK_UTF8_BUFFER(descr, BOARD_DESCR_CCHARS);
	getdata(t_lines - 2, 0, "新讨论区说明: ", gbk_descr, sizeof(gbk_descr),
			DOECHO, YEA);
	if (!gbk_descr)
		return 0;

	convert_g2u(gbk_descr, utf8_descr);
	db_res_t *res = db_cmd("UPDATE boards SET descr = %s WHERE id = %d",
			utf8_descr, bp->id);
	db_clear(res);
	return res;
}

static bool alter_board_parent(board_t *bp)
{
	char bname[BOARD_NAME_LEN + 1];
	board_complete(15, "输入所属讨论区名: ", bname, sizeof(bname),
			AC_LIST_DIR_ONLY);
	board_t parent;
	get_board(bname, &parent);

	db_res_t *res = db_cmd("UPDATE boards SET parent = %d WHERE id = %d",
			parent.id, bp->id);
	db_clear(res);
	return res;
}

static bool alter_board_perm(board_t *bp)
{
	char buf[STRLEN], ans[2];
	int flag = bp->flag, perm = bp->perm;
	if (bp->flag & BOARD_DIR_FLAG) {
		snprintf(buf, sizeof(buf), "(N)不限制 (R)限制阅读 [%c]: ",
				(bp->perm) ? 'R' : 'N');
		getdata(15, 0, buf, ans, sizeof(ans), DOECHO, YEA);
		if (ans[0] == 'N' || ans[0] == 'n') {
			flag &= ~BOARD_POST_FLAG;
			perm = 0;
		} else {
			if (ans[0] == 'R' || ans[0] == 'r')
				flag &= ~BOARD_POST_FLAG;
			clear();
			move(2, 0);
			prints("设定 %s '%s' 讨论区的权限\n", "阅读", bp->name);
			perm = setperms(perm, "权限", NUMPERMS, showperminfo);
			clear();
		}
	} else {
		snprintf(buf, sizeof(buf), "(N)不限制 (R)限制阅读 (P)限制张贴 文章 [%c]: ",
				(flag & BOARD_POST_FLAG) ? 'P' : (perm) ? 'R' : 'N');
		getdata(15, 0, buf, ans, sizeof(ans), DOECHO, YEA);
		if (ans[0] == 'N' || ans[0] == 'n') {
			flag &= ~BOARD_POST_FLAG;
			perm = 0;
		} else {
			if (ans[0] == 'R' || ans[0] == 'r')
				flag &= ~BOARD_POST_FLAG;
			else if (ans[0] == 'P' || ans[0] == 'p')
				flag |= BOARD_POST_FLAG;
			clear();
			move(2, 0);
			prints("设定 %s '%s' 讨论区的权限\n",
					(flag & BOARD_POST_FLAG) ? "张贴" : "阅读", bp->name);
			perm = setperms(perm, "权限", NUMPERMS, showperminfo);
			clear();
		}
	}

	db_res_t *res = db_cmd("UPDATE boards SET flag = %d, perm = %d "
			"WHERE id = %d", flag, perm, bp->id);
	db_clear(res);
	return res;
}

static bool alter_board_flag(board_t *bp, const char *prompt, int flag)
{
	int f = bp->flag;
	if (askyn(prompt, (bp->flag & flag) ? YEA : NA, YEA)) {
		f |= flag;
	} else {
		f &= ~flag;
	}

	if (flag == BOARD_CLUB_FLAG && (f & BOARD_CLUB_FLAG)) {
		if (askyn("是否读限制俱乐部?",
					(bp->flag & BOARD_READ_FLAG) ? YEA : NA, NA)) {
			f |= BOARD_READ_FLAG;
		} else {
			f &= ~BOARD_READ_FLAG;
		}
	}

	db_res_t *res = db_cmd("UPDATE boards SET flag = %d WHERE id = %d",
			f, bp->id);
	db_clear(res);
	return res;
}

int tui_edit_board(const char *cmd)
{
	if (!(HAS_PERM(PERM_BLEVELS)))
		return 0;

	set_user_status(ST_ADMIN);
	if (!check_systempasswd())
		return 0;

	clear();
	stand_title("修改讨论区设置");

	char bname[BOARD_NAME_LEN + 1];
	board_complete(2, "输入讨论区名称: ", bname, sizeof(bname),
			AC_LIST_BOARDS_AND_DIR);
	board_t board;
	if (!*bname || !get_board(bname, &board))
		return -1;
	board_to_gbk(&board);

	board_t parent = { .id = 0, .name = { '\0' } };
	if (board.parent) {
		get_board_by_bid(board.parent, &parent);
		board_to_gbk(&parent);
	}

	clear();
	stand_title("修改讨论区设置");
	move(2, 0);

	show_edit_board_menu(&board, &parent);

	char ans[2];
	getdata(14, 0, "更改哪项设置[0]", ans, sizeof(ans), DOECHO, YEA);
	if (!ans[0])
		return 0;

	int res = 0;
	move(15, 0);
	switch (ans[0]) {
		case '1':
			res = alter_board_name(&board);
			break;
		case '2':
			res = alter_board_descr(&board);
			break;
		case '4':
			res = alter_board_parent(&board);
			break;
		case '5':
			res = alter_board_perm(&board);
			break;
		default:
			break;
	}

	if (!(board.flag & BOARD_DIR_FLAG)) {
		switch (ans[0]) {
			case '7':
				res = askyn("移动精华区", NA, YEA);
				break;
			case '8':
				res = alter_board_flag(&board, "是否匿名?", BOARD_ANONY_FLAG);
				break;
			case '9':
				res = alter_board_flag(&board, "禁止回复?", BOARD_NOREPLY_FLAG);
				break;
			case 'a':
			case 'A':
				res = alter_board_flag(&board, "不计文章数?", BOARD_JUNK_FLAG);
				break;
			case 'b':
			case 'B':
				res = alter_board_flag(&board, "是否俱乐部?", BOARD_CLUB_FLAG);
				break;
#ifdef ENABLE_PREFIX
			case 'c':
			case 'C':
				res = alter_board_flag(&board, "强制前缀?", BOARD_PREFIX_FLAG);
				break;
#endif
		}
	}

	if (res) {
		board_t nb;
		get_board_by_bid(board.id, &nb);
		board_to_gbk(&board);

		if (ans[0] == '1') {
			char secu[STRLEN];
			sprintf(secu, "修改讨论区：%s(%s)", board.name, nb.name);
			securityreport(secu, 0, 1);

			char old[HOMELEN], tar[HOMELEN];
			setbpath(old, board.name);
			setbpath(tar, nb.name);
			rename(old, tar);
			sprintf(old, "vote/%s", board.name);
			sprintf(tar, "vote/%s", nb.name);
			rename(old, tar);
		}

		char vbuf[STRLEN];
		if (*nb.bms) {
			snprintf(vbuf, sizeof(vbuf), "○ %-35.35s(BM: %s)",
					nb.descr, nb.bms);
		} else {
			snprintf(vbuf, sizeof(vbuf), "○ %-35.35s", nb.descr);
		}

		char old_descr[STRLEN];
		snprintf(old_descr, sizeof(old_descr), "○ %s", board.descr);

		if (ans[1] == '2') {
			get_grp(board.name);
			edit_grp(board.name, lookgrp, old_descr, vbuf);
		}

		if (ans[1] == '1' || ans[1] == '7') {
			const char *group = chgrp();
			get_grp(board.name);
			char tmp_grp[STRLEN];
			strcpy(tmp_grp, lookgrp);
			if (strcmp(tmp_grp, group)) {
				char tmpbuf[160];
				sprintf(tmpbuf, "%s:", board.name);
				del_from_file("0Announce/.Search", tmpbuf);
				if (group != NULL) {
					if (add_grp(group, cexplain, nb.name, vbuf) == -1)
						prints("\n成立精华区失败....\n");
					else
						prints("已经置入精华区...\n");

					char newpath[HOMELEN], oldpath[HOMELEN];
					sprintf(newpath, "0Announce/groups/%s/%s",
							group, nb.name);
					sprintf(oldpath, "0Announce/groups/%s/%s",
							tmp_grp, board.name);
					if (strcmp(oldpath, newpath) != 0 && dashd(oldpath)) {
						deltree(newpath);
						rename(oldpath, newpath);
						del_grp(tmp_grp, board.name, old_descr);
					}
				}
			}
		}
		char buf[STRLEN];
		snprintf(buf, sizeof(buf), "更改讨论区 %s 的资料 --> %s", board.name, nb.name);
		report(buf, currentuser.userid);
	}

	clear();
	return 0;

}

// 批注册单时显示的标题
void regtitle() {
	prints("[1;33;44m批注册单 NEW VERSION wahahaha                                                   [m\n");
	prints(" 离开[[1;32m←[m,[1;32me[m] 选择[[1;32m↑[m,[1;32m↓[m] 阅读[[1;32m→[m,[1;32mRtn[m] 批准[[1;32my[m] 删除[[1;32md[m]\n");

	prints("[1;37;44m  编号 用户ID       姓  名       系别             住址             注册时间     [m\n");
}

//      在批注册单时显示的注册ID列表
char *regdoent(int num, reginfo_t* ent) {
	static char buf[128];
	char rname[17];
	char dept[17];
	char addr[17];
	//struct tm* tm;
	//tm=gmtime(&ent->regdate);
	strlcpy(rname, ent->realname, 12);
	strlcpy(dept, ent->dept, 16);
	strlcpy(addr, ent->addr, 16);
	ellipsis(rname, 12);
	ellipsis(dept, 16);
	ellipsis(addr, 16);
	sprintf(buf, "  %4d %-12s %-12s %-16s %-16s %s", num, ent->userid,
			rname, dept, addr, getdatestring(ent->regdate, DATE_SHORT));
	return buf;
}

//      返回userid 与ent->userid是否相等
static int filecheck(void *ent, void *userid)
{
	return !strcmp(((reginfo_t *)ent)->userid, (char *)userid);
}

// 删除注册单文件里的一个记录
int delete_register(int index, reginfo_t* ent, char *direct) {
	delete_record(direct, sizeof(reginfo_t), index, filecheck, ent->userid);
	return DIRCHANGED;
}

//      通过注册单
int pass_register(int index, reginfo_t* ent, char *direct) {
	int unum;
	struct userec uinfo;
	char buf[80];
	FILE *fout;

	unum = getuser(ent->userid);
	if (!unum) {
		clear();
		prints("系统错误! 查无此账号!\n"); //      在回档或者某些情况下,找不到在注册单文件
		pressanykey(); // unregister中的此记录,故删除
		delete_record(direct, sizeof(reginfo_t), index, filecheck,
				ent->userid);
		return DIRCHANGED;
	}

	delete_record(direct, sizeof(reginfo_t), index, filecheck, ent->userid);

	memcpy(&uinfo, &lookupuser, sizeof (uinfo));
#ifdef ALLOWGAME
	uinfo.money = 1000;
#endif
	substitut_record(PASSFILE, &uinfo, sizeof (uinfo), unum);
	sethomefile(buf, uinfo.userid, "register");
	if ((fout = fopen(buf, "a")) != NULL) {
		fprintf(fout, "注册时间     : %s\n", getdatestring(ent->regdate, DATE_EN));
		fprintf(fout, "申请帐号     : %s\n", ent->userid);
		fprintf(fout, "真实姓名     : %s\n", ent->realname);
		fprintf(fout, "学校系级     : %s\n", ent->dept);
		fprintf(fout, "目前住址     : %s\n", ent->addr);
		fprintf(fout, "联络电话     : %s\n", ent->phone);
#ifndef FDQUAN
		fprintf(fout, "电子邮件     : %s\n", ent->email);
#endif
		fprintf(fout, "校 友 会     : %s\n", ent->assoc);
		fprintf(fout, "成功日期     : %s\n", getdatestring(time(NULL), DATE_EN));
		fprintf(fout, "批准人       : %s\n", currentuser.userid);
		fclose(fout);
	}
	mail_file("etc/s_fill", uinfo.userid, "恭禧您，您已经完成注册。");
	sethomefile(buf, uinfo.userid, "mailcheck");
	unlink(buf);
	sprintf(genbuf, "让 %s 通过身分确认.", uinfo.userid);
	securityreport(genbuf, 0, 0);

	return DIRCHANGED;
}

//      处理注册单
int do_register(int index, reginfo_t* ent, char *direct) {
	int unum;
	struct userec uinfo;
	//char ps[80];
	register int ch;
	static char *reason[] = { "请确实填写真实姓名.", "请详填学校科系与年级.", "请填写完整的住址资料.",
			"请详填联络电话.", "请确实填写注册申请表.", "请用中文填写申请单.", "其他" };
	unsigned char rejectindex = 4;

	if (!ent)
		return DONOTHING;

	unum = getuser(ent->userid);
	if (!unum) {
		prints("系统错误! 查无此账号!\n"); //删除不存在的记录,如果有的话
		delete_record(direct, sizeof(reginfo_t), index, filecheck,
				ent->userid);
		return DIRCHANGED;
	}

	memcpy(&uinfo, &lookupuser, sizeof (uinfo));
	clear();
	move(0, 0);
	prints("[1;33;44m 详细资料                                                                      [m\n");
	prints("[1;37;42m [.]接受 [+]拒绝 [d]删除 [0-6]不符合原因                                       [m");

	//strcpy(ps, "(无)");
	for (;;) {
		disply_userinfo(&uinfo);
		move(14, 0);
		printdash(NULL);
		prints("   注册时间   : %s\n", getdatestring(ent->regdate, DATE_EN));
		prints("   申请帐号   : %s\n", ent->userid);
		prints("   真实姓名   : %s\n", ent->realname);
		prints("   学校系级   : %s\n", ent->dept);
		prints("   目前住址   : %s\n", ent->addr);
		prints("   联络电话   : %s\n", ent->phone);
#ifndef FDQUAN
		prints("   电子邮件   : %s\n", ent->email);
#endif
		prints("   校 友 会   : %s\n", ent->assoc);
		ch = egetch();
		switch (ch) {
			case '.':
				pass_register(index, ent, direct);
				return READ_AGAIN;
			case '+':
				uinfo.userlevel &= ~PERM_SPECIAL4;
				substitut_record(PASSFILE, &uinfo, sizeof (uinfo), unum);
				//mail_file("etc/f_fill", uinfo.userid, "请重新填写您的注册资料");
				mail_file("etc/f_fill", uinfo.userid, reason[rejectindex]);
			case 'd':
				uinfo.userlevel &= ~PERM_SPECIAL4;
				substitut_record(PASSFILE, &uinfo, sizeof (uinfo), unum);
				delete_register(index, ent, direct);
				return READ_AGAIN;
			case KEY_DOWN:
			case '\r':
				return READ_NEXT;
			case KEY_LEFT:
				return DIRCHANGED;
			default:
				if (ch >= '0' && ch <= '6') {
					rejectindex = ch - '0';
					//strcpy(uinfo.address, reason[ch-'0']);
				}
				break;
		}
	}
	return 0;
}

struct one_key reg_comms[] = {
		{'r', do_register},
		{'y', pass_register},
		{'d', delete_register},
		{'\0', NULL}
};

void show_register() {
	FILE *fn;
	int x; //, y, wid, len;
	char uident[STRLEN];
	if (!(HAS_PERM(PERM_USER)))
		return;

	set_user_status(ST_ADMIN);
	if (!check_systempasswd()) {
		return;
	}
	clear();
	stand_title("查询使用者注册资料");
	move(1, 0);
	usercomplete("请输入要查询的代号: ", uident);
	if (uident[0] != '\0') {
		if (!getuser(uident)) {
			move(2, 0);
			prints("错误的使用者代号...");
		} else {
			sprintf(genbuf, "home/%c/%s/register",
					toupper(lookupuser.userid[0]), lookupuser.userid);
			if ((fn = fopen(genbuf, "r")) != NULL) {
				prints("\n注册资料如下:\n\n");
				for (x = 1; x <= 15; x++) {
					if (fgets(genbuf, STRLEN, fn))
						prints("%s", genbuf);
					else
						break;
				}
			} else {
				prints("\n\n找不到他/她的注册资料!!\n");
			}
		}
	}
	pressanykey();
}
//  进入 注册单察看栏,看使用者的注册资料或进注册单管理程序
int m_register() {
	FILE *fn;
	char ans[3]; //, *fname;
	int x; //, y, wid, len;
	char uident[STRLEN];

	if (!(HAS_PERM(PERM_USER)))
		return 0;

	set_user_status(ST_ADMIN);
	if (!check_systempasswd()) {
		return 0;
	}
	clear();

	stand_title("设定使用者注册资料");
	for (;;) {
		getdata(1, 0, "(0)离开  (1)审查新注册 (2)查询使用者注册资料 ? : ", ans, 2, DOECHO,
				YEA);
		if (ans[0] == '1' || ans[0] == '2') { // || ans[0]=='3') 现在只有0,1,2
			break;
		} else {
			return 0;
		}
	}
	switch (ans[0]) {
		case '2':
			move(1, 0);
			usercomplete("请输入要查询的代号: ", uident);
			if (uident[0] != '\0') {
				if (!getuser(uident)) {
					move(2, 0);
					prints("错误的使用者代号...");
				} else {
					sprintf(genbuf, "home/%c/%s/register",
							toupper(lookupuser.userid[0]),
							lookupuser.userid);
					if ((fn = fopen(genbuf, "r")) != NULL) {
						prints("\n注册资料如下:\n\n");
						for (x = 1; x <= 15; x++) {
							if (fgets(genbuf, STRLEN, fn))
								prints("%s", genbuf);
							else
								break;
						}
					} else {
						prints("\n\n找不到他/她的注册资料!!\n");
					}
				}
			}
			pressanykey();
			break;
		case '1':
			i_read(ST_ADMIN, "unregistered", regtitle, regdoent,
					&reg_comms[0], sizeof(reginfo_t));
			break;
	}
	clear();
	return 0;
}

//      删除一个帐号
int d_user(char *cid) {
	int id, num, i;
	char secu[STRLEN];
	char genbuf_rm[STRLEN]; //added by roly 02.03.24
	char passbuf[PASSLEN];

	if (!(HAS_PERM(PERM_USER)))
		return 0;

	set_user_status(ST_ADMIN);
	if (!check_systempasswd()) {
		return 0;
	}
	clear();
	stand_title("删除使用者帐号");
	// Added by Ashinmarch in 2008.10.20 
	// 砍掉账号时增加密码验证
	getdata(1, 0, "[1;37m请输入密码: [m", passbuf, PASSLEN, NOECHO, YEA);
	passbuf[8] = '\0';
	if (!passwd_check(currentuser.userid, passbuf)) {
		prints("[1;31m密码输入错误...[m\n");
		return 0;
	}
	// Add end.
	if (!gettheuserid(1, "请输入欲删除的使用者代号: ", &id))
		return 0;
	if (!strcmp(lookupuser.userid, "SYSOP")) {
		prints("\n对不起，你不可以删除 SYSOP 帐号!!\n");
		pressreturn();
		clear();
		return 0;
	}
	if (!strcmp(lookupuser.userid, currentuser.userid)) {
		prints("\n对不起，你不可以删除自己的这个帐号!!\n");
		pressreturn();
		clear();
		return 0;
	}
	prints("\n\n以下是 [%s] 的部分资料:\n", lookupuser.userid);
	prints("    User ID:  [%s]\n", lookupuser.userid);
	prints("    昵   称:  [%s]\n", lookupuser.username);
	strcpy(secu, "ltmprbBOCAMURS#@XLEast0123456789\0");
	for (num = 0; num < strlen(secu) - 1; num++) {
		if (!(lookupuser.userlevel & (1 << num)))
			secu[num] = '-';
	}
	prints("    权   限: %s\n\n", secu);

	num = getbnames(lookupuser.userid, secu, &num);
	if (num) {
		prints("[%s] 目前尚担任了 %d 个版的版主: ", lookupuser.userid, num);
		for (i = 0; i < num; i++)
			prints("%s ", bnames[i]);
		prints("\n请先使用版主卸职功能取消其版主职务再做该操作.");
		pressanykey();
		clear();
		return 0;
	}

	sprintf(genbuf, "你确认要删除 [%s] 这个 ID 吗", lookupuser.userid);
	if (askyn(genbuf, NA, NA) == NA) {
		prints("\n取消删除使用者...\n");
		pressreturn();
		clear();
		return 0;
	}
	sprintf(secu, "删除使用者：%s", lookupuser.userid);
	securityreport(secu, 0, 0);
	sprintf(genbuf, "mail/%c/%s", toupper(lookupuser.userid[0]),
			lookupuser.userid);
	//f_rm(genbuf);
	/* added by roly 02.03.24 */
	sprintf(genbuf_rm, "/bin/rm -fr %s", genbuf); //added by roly 02.03.24
	system(genbuf_rm);
	/* add end */
	sprintf(genbuf, "home/%c/%s", toupper(lookupuser.userid[0]),
			lookupuser.userid);
	//f_rm(genbuf);
	/* added by roly 02.03.24 */
	sprintf(genbuf_rm, "/bin/rm -fr %s", genbuf); //added by roly 02.03.24
	system(genbuf_rm);
	/* add end */
	lookupuser.userlevel = 0;
#ifdef ALLOWGAME
	lookupuser.money = 0;
	lookupuser.nummedals = 0;
	lookupuser.bet = 0;
#endif
	strcpy(lookupuser.username, "");
	prints("\n%s 已经被灭绝了...\n", lookupuser.userid);
	lookupuser.userid[0] = '\0';
	substitut_record(PASSFILE, &lookupuser, sizeof(lookupuser), id);
	setuserid(id, lookupuser.userid);
	pressreturn();
	clear();
	return 1;
}

//      更改使用者的权限
int x_level() {
	int id;
	char reportbuf[60];
	unsigned int newlevel;

	if (!HAS_PERM(PERM_SYSOPS))
		return 0;

	set_user_status(ST_ADMIN);
	if (!check_systempasswd()) {
		return 0;
	}
	clear();
	move(0, 0);
	prints("更改使用者权限\n");
	clrtoeol();
	move(1, 0);
	usercomplete("输入欲更改的使用者帐号: ", genbuf);
	if (genbuf[0] == '\0') {
		clear();
		return 0;
	}
	if (!(id = getuser(genbuf))) {
		move(3, 0);
		prints("Invalid User Id");
		clrtoeol();
		pressreturn();
		clear();
		return 0;
	}
	move(1, 0);
	clrtobot();
	move(2, 0);
	prints("设定使用者 '%s' 的权限 \n", genbuf);
	newlevel
			= setperms(lookupuser.userlevel, "权限", NUMPERMS, showperminfo);
	move(2, 0);
	if (newlevel == lookupuser.userlevel)
		prints("使用者 '%s' 权限没有变更\n", lookupuser.userid);
	else {
		sprintf(reportbuf, "change level: %s %.8x -> %.8x",
				lookupuser.userid, lookupuser.userlevel, newlevel);
		report(reportbuf, currentuser.userid);
		lookupuser.userlevel = newlevel;
		{
			char secu[STRLEN];
			sprintf(secu, "修改 %s 的权限", lookupuser.userid);
			securityreport(secu, 0, 0);
		}

		substitut_record(PASSFILE, &lookupuser, sizeof(struct userec), id);
		if (!(lookupuser.userlevel & PERM_REGISTER)) {
			char src[STRLEN], dst[STRLEN];
			sethomefile(dst, lookupuser.userid, "register.old");
			if (dashf(dst))
				unlink(dst);
			sethomefile(src, lookupuser.userid, "register");
			if (dashf(src))
				rename(src, dst);
		}
		prints("使用者 '%s' 权限已经更改完毕.\n", lookupuser.userid);
	}
	pressreturn();
	clear();
	return 0;
}

void a_edits() {
	int aborted;
	char ans[7], buf[STRLEN], buf2[STRLEN];
	int ch, num, confirm;
	static char *e_file[] = { "../Welcome", "../Welcome2", "issue",
			"logout", "../vote/notes", "hotspot", "menu.ini",
			"../.badname", "../.bad_email", "../.bad_host", "autopost",
			"junkboards", "sysops", "whatdate", "../NOLOGIN",
			"../NOREGISTER", "special.ini", "hosts", "restrictip",
			"freeip", "s_fill", "f_fill", "register", "firstlogin",
			"chatstation", "notbackupboards", "bbsnet.ini", "bbsnetip",
			"bbsnet2.ini", "bbsnetip2", NULL };
	static char *explain_file[] = { "特殊进站公布栏", "进站画面", "进站欢迎档", "离站画面",
			"公用备忘录", "系统热点", "menu.ini", "不可注册的 ID", "不可确认之E-Mail",
			"不可上站之位址", "每日自动送信档", "不算POST数的版", "管理者名单", "纪念日清单",
			"暂停登陆(NOLOGIN)", "暂停注册(NOREGISTER)", "个人ip来源设定档", "穿梭ip来源设定档",
			"只能登陆5id的ip设定档", "不受5 id限制的ip设定档", "注册成功信件", "注册失败信件",
			"新用户注册范例", "用户第一次登陆公告", "国际会议厅清单", "区段删除不需备份之清单",
			"BBSNET 转站清单", "穿梭限制ip", "BBSNET2 转站清单", "穿梭2限制IP", NULL };
	set_user_status(ST_ADMIN);
	if (!check_systempasswd()) {
		return;
	}
	clear();
	move(1, 0);
	prints("编修系统档案\n\n");
	for (num = 0; (HAS_PERM(PERM_ESYSFILE)) ? e_file[num] != NULL
			&& explain_file[num] != NULL : strcmp(explain_file[num], "menu.ini"); num++) {
		prints("[\033[1;32m%2d\033[m] %s", num + 1, explain_file[num]);
		if (num < 17)
			move(4 + num, 0);
		else
			move(num - 14, 50);
	}
	prints("[\033[1;32m%2d\033[m] 都不想改\n", num + 1);

	getdata(23, 0, "你要编修哪一项系统档案: ", ans, 3, DOECHO, YEA);
	ch = atoi(ans);
	if (!isdigit(ans[0]) || ch <= 0 || ch > num || ans[0] == '\n'
			|| ans[0] == '\0')
		return;
	ch -= 1;
	sprintf(buf2, "etc/%s", e_file[ch]);
	move(3, 0);
	clrtobot();
	sprintf(buf, "(E)编辑 (D)删除 %s? [E]: ", explain_file[ch]);
	getdata(3, 0, buf, ans, 2, DOECHO, YEA);
	if (ans[0] == 'D' || ans[0] == 'd') {
		sprintf(buf, "你确定要删除 %s 这个系统档", explain_file[ch]);
		confirm = askyn(buf, NA, NA);
		if (confirm != 1) {
			move(5, 0);
			prints("取消删除行动\n");
			pressreturn();
			clear();
			return;
		}
		{
			char secu[STRLEN];
			sprintf(secu, "删除系统档案：%s", explain_file[ch]);
			securityreport(secu, 0, 0);
		}
		unlink(buf2);
		move(5, 0);
		prints("%s 已删除\n", explain_file[ch]);
		pressreturn();
		clear();
		return;
	}
	set_user_status(ST_EDITSFILE);
	aborted = vedit(buf2, NA, YEA); /* 不添加文件头, 允许修改头部信息 */
	clear();
	if (aborted != -1) {
		prints("%s 更新过", explain_file[ch]);
		{
			char secu[STRLEN];
			sprintf(secu, "修改系统档案：%s", explain_file[ch]);
			securityreport(secu, 0, 0);
		}

		if (!strcmp(e_file[ch], "../Welcome")) {
			unlink("Welcome.rec");
			prints("\nWelcome 记录档更新");
		} else if (!strcmp(e_file[ch], "whatdate")) {
			brdshm->fresh_date = time(0);
			prints("\n纪念日清单 更新");
		}
	}
	pressreturn();
}

// 全站广播...
int wall() {
	char passbuf[PASSLEN];

	if (!HAS_PERM(PERM_SYSOPS))
		return 0;
	// Added by Ashinmarch on 2008.10.20
	// 全站广播前增加密码验证
	clear();
	stand_title("全站广播!");
	getdata(1, 0, "[1;37m请输入密码: [m", passbuf, PASSLEN, NOECHO, YEA);
	passbuf[8] = '\0';
	if (!passwd_check(currentuser.userid, passbuf)) {
		prints("[1;31m密码输入错误...[m\n");
		return 0;
	}
	// Add end.

	set_user_status(ST_MSG);
	move(2, 0);
	clrtobot();
	if (!get_msg("所有使用者", buf2, 1)) {
		return 0;
	}
	if (apply_ulist(dowall) == -1) {
		move(2, 0);
		prints("线上空无一人\n");
		pressanykey();
	}
	prints("\n已经广播完毕...\n");
	pressanykey();
	return 1;
}

// 设定系统密码
int setsystempasswd() {
	FILE *pass;
	char passbuf[20], prepass[20];
	set_user_status(ST_ADMIN);
	if (!check_systempasswd())
		return 0;
	if (strcmp(currentuser.userid, "SYSOP")) {
		clear();
		move(10, 20);
		prints("对不起，系统密码只能由 SYSOP 修改！");
		pressanykey();
		return 0;
	}
	getdata(2, 0, "请输入新的系统密码(直接回车则取消系统密码): ", passbuf, 19, NOECHO, YEA);
	if (passbuf[0] == '\0') {
		if (askyn("你确定要取消系统密码吗?", NA, NA) == YEA) {
			unlink("etc/.syspasswd");
			securityreport("取消系统密码", 0, 0);
		}
		return 0;
	}
	getdata(3, 0, "确认新的系统密码: ", prepass, 19, NOECHO, YEA);
	if (strcmp(passbuf, prepass)) {
		move(4, 0);
		prints("两次密码不相同, 取消此次设定.");
		pressanykey();
		return 0;
	}
	if ((pass = fopen("etc/.syspasswd", "w")) == NULL) {
		move(4, 0);
		prints("系统密码无法设定....");
		pressanykey();
		return 0;
	}
	fprintf(pass, "%s\n", genpasswd(passbuf));
	fclose(pass);
	move(4, 0);
	prints("系统密码设定完成....");
	pressanykey();
	return 0;
}

#define DENY_LEVEL_LIST ".DenyLevel"
extern int denylist_key_deal(const char *file, int ch, const char *line);

/**
 * 全站处罚列表标题.
 */
static void denylist_title_show(void)
{
	move(0, 0);
	prints("\033[1;44;36m 处罚到期的ID列表\033[K\033[m\n"
			" 离开[\033[1;32m←\033[m] 选择[\033[1;32m↑\033[m,\033[1;32m↓\033[m] 添加[\033[1;32ma\033[m]  修改[\033[1;32mc\033[m] 恢复[\033[1;32md\033[m] 到期[\033[1;32mx\033[m] 查找[\033[1;32m/\033[m]\n"
			"\033[1;44m 用户代号     处罚说明(A-Z;'.[])                 权限 结束日期   站务          \033[m\n");
}

/**
 * 全站处罚列表入口函数.
 * @return 0/1.
 */
int x_new_denylevel(void)
{
	if (!HAS_PERM(PERM_OBOARDS) && !HAS_PERM(PERM_SPECIAL0))
		return DONOTHING;
	set_user_status(ST_ADMIN);
	list_text(DENY_LEVEL_LIST, denylist_title_show, denylist_key_deal, NULL);
	return FULLUPDATE;
}
