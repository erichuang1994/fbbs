/**
 * @fileOverview All configurations of system
 * @author pzh
 */

/**
 * @namespace f.config Including pageInfo, userInfo, systemConfig, etc..
 */
f.namespace('f.config');

// 全局设置
f.config = {
    systemConfig: {
        version: "201312130001",
        webRoot: "/web",
        baseUri: "/web/html",
        defaultPage: "#/home.html",
        ajaxUri: "/web/bbs",
        cookiePath: "/",
        defaultTitle: "日月光华 | 复旦大学BBS",
        defaultShortTitle: "日月光华"
    },
    urlConfig: {
        login: "#/login.html",
        logout: "#/logout.html",
        newMail: "#/new_mail.html",
        mail: "#/mail.html",
        userInfo: "#/user_info.html"
    },
    userInfo: {},
    menuList: {
        title: {
            label: "日月光华",
            url: "#/home.html"
        },
        menus: [
            {
                id: "sec",
                label: "分类讨论",
                url: "#/sec.html"
            },
            {
                id: "0an",
                label: "本站精华",
                url: "#/0an.html"
            },
            {
                id: "top10",
                label: "本日十大",
                url: "#/top10.html"
            }
        ],
        userMenus: [
            {
                id: "user-info",
                label: "个人资料",
                url: "#/user_info.html"
            },
            {
                id: "collections-edit",
                label: "收藏管理",
                url: "#/collections_edit.html"
            },
            {
                id: "friends-edit",
                label: "好友管理",
                url: "#/friends_edit.html"
            }
        ]
    },
    pageInfo: {}
}

/**
 * @namespace f.zIndex 全局z-index管理
 */
f.namespace('f.zIndex');

f.zIndex.scroll = 9999; // 随屏滚动的z-index范围为8889-9999;
f.zIndex.ui = 10000; // ui的z-index的z-index范围
f.zIndex.tooltip = 11000;
f.zIndex.mask = 12000;

/**
 * Global mediator object used to subscribe/publish custom events
 *
 * @public
 */
f.mediator = Mediator();
