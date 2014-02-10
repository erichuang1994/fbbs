/**
 * @fileOverview board search
 * @author pzh
 */

(function ($, f) {
    'use strict';

    $.widget("ui.boardSearch", {
        version: '1.0.0',
        options: {
            tpl: 'ui.boardSearch.tpl',
            listTpl: 'ui.sideBarList.tpl',
            searchDelay: 700,
            keydownKeyCode: [
                8, 46, // backspace, delete
                48, 49, 50, 51, 52, 53, 54, 55, 56, 57, // 0-9
                65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
                81, 82, 83, 84, 85, 86, 87, 88, 89, 90  // a-z
            ],
            onsearch: null,
            oncreate: null,
            onclickTarget: null
        },
        _debounce: 0,
        _timeout: null,
        _needPlaceholder: !('placeholder' in document.createElement('input')),
        _hasInputEvent: 'oninput' in document.createElement('input'),
        _create: function () {
            this.element.html(f.tpl.format(this.options.tpl));
            this._getElements();
            this._$input.toggleClass('with-placeholder', this._needPlaceholder);
            this._bindEvents();
        },
        _getElements: function () {
            this._$input = this.element.find('.ui-board-search-input');
            this._$container = this.element.find('.ui-board-search');
            this._$list = this.element.find('.ui-board-search-list-container');
        },
        _checkPlaceholder: function (input) {
            this._$input.toggleClass('with-placeholder', this._needPlaceholder && !input);
        },
        _bindEvents: function () {
            this._on({
                'input .ui-board-search-input': $.proxy(this, '_oninput'),
                'keydown .ui-board-search-input': $.proxy(this, '_onkeydown'),
                'keyup .ui-board-search-input': $.proxy(this, '_onkeyup'),
                'click .ui-board-search-list .ui-side-bar-list-reset': $.proxy(this, '_reset'),
                'click .ui-board-search-list .ui-side-bar-list-target': $.proxy(this, '_onclickTarget')
            });
        },
        _onkeydown: function (event) {
            switch (event.which) {
                case 13:
                    // Press enter
                    event.preventDefault();
                    event.stopPropagation();
                    var a = this._$list.find('.ui-side-bar-list-item.focus a');
                    if (a.length) {
                        this._onclickTarget(event, a);
                    }
                    break;
                case 38:
                    // Press up key
                    this._prev();
                    event.preventDefault();
                    event.stopPropagation();
                    break;
                case 40:
                    // Press down key
                    this._next();
                    event.preventDefault();
                    event.stopPropagation();
                    break;
            }
        },
        _onkeyup: function (event) {
            if (!f.contains(this.options.keydownKeyCode, event.which)) {
                event.preventDefault();
                event.stopPropagation();
                return;
            }
            // For IE 8: bind keyup event
            // For IE 9: fix backspace and delete key can not fire oninput event
            if ((!this._hasInputEvent)
                || (this._needPlaceholder && this._hasInputEvent && f.contains([8, 46], event.which))) {
                this._oninput(event);
            }
        },
        _oninput: function (event) {
            this._checkPlaceholder($(event.target).val());
            var now = (new Date()).getTime();
            if (now - this._debounce < this.options.searchDelay) {
                this._timeout && clearTimeout(this._timeout);
            }
            this._timeout = setTimeout($.proxy(this, '_onsearch', {
                    value: $(event.target).val()
                }), this.options.searchDelay);
            this._debounce = now;
        },
        _onsearch: function (data) {
            var val = f.trim(data.value);
            this._$input.val(val);
            if (val) {
                this._trigger('onsearch', null, {
                    value: val
                });
            }
            else {
                this._reset();
            }
        },
        _focus: function (index) {
            this._$list
                .find('.ui-side-bar-list-item')
                .removeClass('focus')
                .eq(index)
                .addClass('focus');
        },
        _next: function () {
            var items = this._$list.find('.ui-side-bar-list-item');
            var focused = items.filter('.focus');
            var index = items.index(focused) + 1;
            if (index < items.length) {
                focused.removeClass('focus');
                items.eq(index).addClass('focus');
            }
        },
        _prev: function () {
            var items = this._$list.find('.ui-side-bar-list-item');
            var focused = items.filter('.focus');
            var index = items.index(focused) - 1;
            if (index >= 0) {
                focused.removeClass('focus');
                items.eq(index).addClass('focus');
            }
        },
        _reset: function () {
            this.removeList();
            this._$input.val('');
            this._checkPlaceholder();
        },
        _onclickTarget: function (event, target) {
            var $target = target || $(event.currentTarget);
            this._trigger('onclickTarget', null, {
                url: $target.attr('href'),
                target: !!target
            });
            this._reset();
        },
        loading: function (toggle) {
            this._$container.toggleClass('loading', toggle !== false);
        },
        renderList: function (list) {
            this._$list.html(f.tpl.format(this.options.listTpl, {
                className: 'ui-board-search-list',
                label: '搜索结果',
                rightLabel: '清空',
                emptyTip: '无结果',
                list: list
            }));
            this._focus(0);
        },
        removeList: function () {
            this._$list.empty();
        }
    });
}(jQuery, f));