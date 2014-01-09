<tpl id="ui.login.tpl">
<div class="ui-login">
    <form class="ui-login-form">
        <div class="ui-login-content">
            <label class="ui-login-label" for="id">账号</label>
            <a class="ui-login-label-tip" href="!#{this.registUrl}">注册</a>
            <input class="ui-login-input ui-login-input-id" type="text" id="id" name="id" placeholder="请输入ID" value="" autofocus required />
        </div>
        <div class="ui-login-content">
            <label class="ui-login-label" for="pw">密码</label>
            <a class="ui-login-label-tip" href="!#{this.forgetUrl}">忘记密码</a>
            <input class="ui-login-input ui-login-input-pw" type="password" id="pw" name="pw" placeholder="请输入密码" value="" required />
            <div class="ui-login-pw-method"></div>
        </div>
        <div class="ui-login-content">
            <input class="ui-login-input ui-login-input-rm" type="checkbox" id="rm" name="rm" />
            <label class="ui-login-label ui-login-label-rm" for="rm">3个月内自动登录</label>
        </div>
        <div class="ui-login-content">
            <p class="ui-login-tip"></p>
        </div>
        <div class="ui-login-content">
            <button class="ui-login-button ui-login-submit">!#{this.loginText}</button>
            <span class="ui-login-loading"></span>
        </div>
    </form>
</div>
</tpl>