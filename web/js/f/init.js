/**
 * @fileOverview Initializing system, and this is the entry of system
 * @author pzh
 */

(function ($, f) {
    'use strict';

    $.extend(f, (function () {
        /**
         * Initializing widget of system and regiser global event callback handler
         */

        function initBBS() {
            $('.menu-nav').topbar({
                onchange: function (event, data) {
                    f.mediator.trigger('app:topbar:onchange', data);
                }
            });

            /*
            $('.side-nav').sideNav({
                zIndex: f.zIndex.ui--
            });

            $(document.body).loading();
            */

            //history back click event handler
            $('.body').on('click', '.history-back', function () {
                History.back();
            });

            // hide layer click event handler
            $('.body').on('click', '.cancel-layer', function () {
                $(document).trigger('click');

                return false;
            });

        }

        /**
         * Load config from server
         */

        function loadConfig() {
            $.ajax({
                type: 'GET',
                url: f.config.bbsConfig,
                async: false
            }).done(function (data, textStatus, jqXHR) {
                f.extend(f.config, JSON.parse(data));
            }).fail(function () {});
        }

        /**
         * Load template from server, then parse and cache the template on the client-side
         */

        function loadTemplate() {
            $.ajax({
                type: 'GET',
                url: f.config.systemConfig.webRoot + '/tpl/tpl.html?__v=' + f.config.systemConfig.version,
                async: false
            }).done(function (data, textStatus, jqXHR) {
                parseTemplate(String(data));
            }).fail(function () {});
        }

        /**
         * Parse and cache the client-side template
         *
         * @param  {string} tplText Template text value
         */

        function parseTemplate(tplText) {
            var tplSettings = [];
            var pattern = /<(script|tpl)[^>]*id=['"]([\w-\.]*)['"][^>]*>([\s\S]*?)<\/\1>/g;
            var result;

            while ((result = pattern.exec(tplText)) != null) {
                tplSettings.push({
                    id: result[2],
                    tpl: result[3]
                });
            }

            var i = tplSettings.length;

            while (i--) {
                var tpl = tplSettings[i];

                f.tpl.register(tpl.id, tpl.tpl);
            }
        }

        /**
         * The entry of system, initializing template, system and register hash change event etc.
         */

        function init() {
            loadConfig();
            loadTemplate();
            initBBS();
            $('#body').opoa({
                baseUri: f.config.systemConfig.baseUri,
                defaultPage: f.config.systemConfig.defaultPage,
                beforeClose: function () {
                    //$(document.body).loading('show');
                },
                onclose: function () {
                    if (f.bbsPage) {
                        f.bbsPage.close();
                    }
                },
                onfail: function () {
                    //$(document.body).loading('hide');
                },
                afterClose: function (event, data) {
                    var url = data.url;
                    var pageInfo = f.config.pageInfo;
                    var sideNav = $(':data(ui-sideNav)');
                    var menuNav = $(':data(ui-menuNav)');

                    /*
                    $(document.body).loading('hide');

                    if (pageInfo.isShowSideNav) {
                        sideNav.sideNav({
                            isShow: 1
                        });

                        if (pageInfo.isRenderSideNav) {
                            sideNav.sideNav('render');
                        }
                    } else {
                        sideNav.sideNav({
                            isShow: 0
                        });
                    }
                    */

                    if (pageInfo.menuNavId) {
                        menuNav.topbar('setActivate', pageInfo.menuNavId);
                    }

                    // ie6,7提示
                    if ($.browser.msie && ~~$.browser.version < 8) {
                        /*
                        $('#global-tip')
                            .show()
                            .find('.text')
                            .html(h.format('提示：本系统不支持IE6、IE7浏览器！为了避免网页显示不兼容，建议您升级浏览器。<a target="_target" href="!{firefox}">Firefox浏览器下载</a>&#12288;|&#12288;<a target="_target" href="!{chrome}">Chrome浏览器下载</a>', {
                            chrome: 'http://www.google.cn/intl/zh-CN/chrome/browser/',
                            firefox: 'http://firefox.com.cn/download/'
                        }));
                        */
                    }
                }
            });
        }

        return {
            init: init,
            bbsPage: null
        };

    })());
}(jQuery, f));