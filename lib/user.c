#include "bbs.h"
#include "fbbs/mdbi.h"
#include "fbbs/session.h"
#include "fbbs/string.h"

#define USER_ID_HASH_KEY  "user_id"

struct userec currentuser;

static void set_user_id_cache(const char *uname, user_id_t uid)
{
	char name[IDLEN + 1];
	strlcpy(name, uname, sizeof(name));
	strtolower(name, name);

	mdb_res_t *r = mdb_cmd("HSET", USER_ID_HASH_KEY" %s %"PRIdUID, name, uid);
	mdb_clear(r);
}

static user_id_t get_user_id_cache(const char *uname)
{
	char name[IDLEN + 1];
	strlcpy(name, uname, sizeof(name));
	strtolower(name, name);

	return (user_id_t)mdb_integer(-1, "HGET", USER_ID_HASH_KEY" %s", name);
}

void remove_user_id_cache(const char *uname)
{
	char name[IDLEN + 1];
	strlcpy(name, uname, sizeof(name));
	strtolower(name, name);

	mdb_res_t *r = mdb_cmd("HDEL", USER_ID_HASH_KEY" %s", name);
	mdb_clear(r);
}

/**
 * Get user id by name.
 * @param name The user name.
 * @return user id on success, 0 if not exist, -1 on error.
 */
user_id_t get_user_id(const char *name)
{
	user_id_t uid = get_user_id_cache(name);
	if (uid != -1)
		return uid;

	db_res_t *res = db_query("SELECT id FROM alive_users"
			" WHERE lower(name) = lower(%s)", name);
	if (!res)
		return -1;

	uid = 0;
	if (db_res_rows(res) > 0)
		uid = db_get_user_id(res, 0, 0);
	db_clear(res);

	if (uid > 0)
		set_user_id_cache(name, uid);
	return uid;
}

#define USER_COUNT_CACHE_KEY  "c:users"

enum {
	USER_COUNT_REFRESH_INTERVAL = 3600,
};

int get_user_count(void)
{
	int cached = mdb_integer(-1, "GET", USER_COUNT_CACHE_KEY);
	if (cached >= 0)
		return cached;

	int count = 0;
	db_res_t *res = db_query("SELECT count(*) FROM alive_users");
	if (res && db_res_rows(res) > 0)
		count = db_get_bigint(res, 0, 0);
	db_clear(res);

	mdb_res_t *r = mdb_cmd("SET", USER_COUNT_CACHE_KEY" %d", count);
	mdb_clear(r);
	r = mdb_cmd("EXPIRE", USER_COUNT_CACHE_KEY" %d",
			USER_COUNT_REFRESH_INTERVAL);
	mdb_clear(r);
	
	return count;
}

static int _user_data_add(const char *name, int uid, int field, int delta)
{
	const char *fields[] = { "logins", "posts", "stay" };
	if (field < 0 || field >= ARRAY_SIZE(fields))
		return -1;

	char query[128];
	int bytes = snprintf(query, sizeof(query),
		"UPDATE users SET %s = %s + %d WHERE %s", fields[field],
		fields[field], delta, name ? "lower(name) = lower(%s)" : "id = %d");
	if (bytes >= sizeof(query))
		return -1;

	db_res_t *res;
	if (name)
		res = db_query(query, name);
	else
		res = db_query(query, uid);
	if (!res)
		return -1;
	return 0;
}

int user_data_add_by_name(const char *name, int field, int delta)
{
	return _user_data_add(name, 0, field, delta);
}

int user_data_add(int uid, int field, int delta)
{
	return _user_data_add(NULL, uid, field, delta);
}

int calc_user_stay(bool is_login, bool is_dup, time_t login, time_t logout)
{
	time_t now = time(NULL);
	time_t last = logout > login ? logout : login;
	int stay = now - last;
	if (stay < 0 || (is_login && !is_dup))
		stay = 0;
	return stay;	
}

int set_my_last_post_time(fb_time_t t)
{
	mdb_res_t *res = mdb_cmd("HSET",
			"last_post_time %"PRIdUID" %"PRIdFBT, session.uid, t);
	mdb_clear(res);
	return !res;
}

fb_time_t get_my_last_post_time(void)
{
	return (fb_time_t) mdb_integer(0, "HGET",
			"last_post_time %"PRIdUID, session.uid);
}

int set_doc_mode(int mode)
{
	mdb_res_t *res = mdb_cmd("HSET",
			"doc_mode %"PRIdUID" %d", session.uid, mode);
	mdb_clear(res);
	return !res;
}

int get_doc_mode(void)
{
	return (int) mdb_integer(0, "HGET", "doc_mode %"PRIdUID, session.uid);
}
