var ws_url = "ws://" + location.host + "/hall";
var ws_hdl = null;
let fileReader = new FileReader();
let fileChunk = null;

function get_user_info() {
    $.ajax({
        url: "/info",
        type: "get",
        success: function (res) {
            var info_html;
            if (res.photo == "waiting" || res.photo == null) {
                info_html = "<img src='./img/waiting.jpg'  id='head'>"
                    + "<p>" + "用户名：" + res.username + " 积分：" + res.score
                    + "</br>" + "比赛总次：" + res.total_count + " 获胜总次："
                    + res.win_count + "</p>";
            } else {
                info_html = "<img src='./img/" + res.photo + "' id='head'>"
                    + "<p>" + "用户名：" + res.username + " 积分：" + res.score
                    + "</br>" + "比赛总次：" + res.total_count + " 获胜总次："
                    + res.win_count + "</p>";
            }

            var screen_div = document.getElementById("screen");
            screen_div.innerHTML = info_html;

            ws_hdl = new WebSocket(ws_url);
            ws_hdl.onopen = ws_onopen;
            ws_hdl.onclose = ws_onclose;
            ws_hdl.onerror = ws_onerror;
            ws_hdl.onmessage = ws_onmessage;
        },
        error: function (xhr) {
            alert(JSON.stringify(xhr));
            location.replace("/login.html");
        }
    })
}
get_user_info();

window.onbeforeunload = function () {
    ws_hdl.close();
}

// 切换用户搜索
var add = document.getElementById("add");
add.onclick = function () {
    var friends = document.getElementById("friends");
    friends.innerHTML = "";
    var friends_html = "<div id='search'><button onclick='ret()' id='button1'></button>" +
        "<input type='text' id='search_text'>" +
        "<button onclick='search()' id='button2'></button></div>" +
        "<ul class='list' id='friends-list'></ul>";
    friends.innerHTML = friends_html;
}

// 查询用户返回好友列表
function ret() {
    location.replace("/hall.html");
}

// 搜索点击
function search() {
    var search_text = document.getElementById("search_text").value;
    if (search_text == "") {
        alert("请输入用户名！");
        // 聚焦光标
        jQuery("#search_text").focus();
        return false;
    }

    var info = {
        optype: "user_search",
        username: search_text
    };

    ws_hdl.send(JSON.stringify(info));
}

function pass(username) {
    // 发送邀请的玩家用户名
    var info = {
        optype: "pass_friend",
        username: username
    };

    ws_hdl.send(JSON.stringify(info));
}

function refuse(username) {
    // 发送邀请的玩家用户名
    var info = {
        optype: "refuse_friend",
        username: username
    };

    ws_hdl.send(JSON.stringify(info));
}

var button_flag = "stop";
// 匹配按钮的点击事件处理：
var be = document.getElementById("match-button");
be.onclick = function () {
    if (button_flag == "stop") {
        // 没有进行匹配的状态下点击按钮，发送对战匹配请求
        var req_json = {
            optype: "match_start"
        }
        ws_hdl.send(JSON.stringify(req_json));
    } else {
        // 正在匹配中的状态下点击按钮，发送停止对战匹配请求
        var req_json = {
            optype: "match_stop"
        }
        ws_hdl.send(JSON.stringify(req_json));
    }
}

// 邀请按钮点击事件
function invite_friend(username) {
    // 发送邀请的玩家用户名
    var info = {
        optype: "invite_select",
        username: username
    };

    ws_hdl.send(JSON.stringify(info));
}

// 添加按钮点击事件
function apply_friend(username) {
    // 发送邀请的玩家用户名
    var info = {
        optype: "apply_friend",
        username: username
    };

    ws_hdl.send(JSON.stringify(info));
}

// 生成uuid
function generateUUID() {
    var result = '';
    var characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
    var charactersLength = characters.length;
    for (var i = 0; i < 36; i++) {
        result += characters.charAt(Math.floor(Math.random() * charactersLength));
    }

    // 根据UUID规范设置必要的字符  
    result = result.replace(/(.{8})(.{4})(.{4})(.{4})(.{12})/, "$1-$2-$3-$4-$5");

    return result;
}

// 上传文件按钮点击事件处理
function uploadFile() {
    // 获取input标签里的文件
    const fileInput = document.getElementById('fileInput');
    const file = fileInput.files[0];
    // 获取文件名
    const fileName = file.name;
    // 获取文件后缀名
    const lastIndex = fileName.lastIndexOf('.');
    let fileExtension;
    if (lastIndex !== -1) {
        fileExtension = fileName.slice(lastIndex);
    } else {
        fileExtension = "";
    }

    // 发送文件文件各项信息给后端保存
    ws_hdl.send(JSON.stringify({
        optype: "file",
        filename: generateUUID(),
        fileExtension: fileExtension,
        size: file.size
    }));

    // 发送文件数据
    fileReader.onload = function (event) {
        if (event.target.readyState === FileReader.DONE) { // 文件读取完成  
            fileChunk = event.target.result;
            ws_hdl.send(fileChunk); // 发送文件内容  
        }
    };

    // 分片读取文件（这里简化为一次读取整个文件）  
    fileReader.readAsArrayBuffer(file);
}

function ws_onopen() {
    console.log("websocket onopen");
}
function ws_onclose() {
    console.log("websocket onopen");
}
function ws_onerror() {
    console.log("websocket onopen");
}
function ws_onmessage(evt) {
    var rsp_json = JSON.parse(evt.data);

    // 获取模糊查询用户列表
    if (rsp_json.length != null && rsp_json[0].optype == "friends_search") {
        var friends_html = "";
        for (var i = 1; i < rsp_json.length; ++i) {
            var photo = rsp_json[i].photo;
            var username = rsp_json[i].username;
            friends_html += "<li><img src='./img/" + photo + "'>" +
                "<div id='invite'>" + username + "</div>" +
                "<button onclick=\"apply_friend('" + username + "')\">添加</button></li>";
        }
        console.log(friends_html);
        var element = document.getElementById("friends-list");
        element.insertAdjacentHTML('beforeend', friends_html);
        return;
    }

    // 获取好友列表
    if (rsp_json.length != null && rsp_json[0].optype == "friends") {
        var friends_html = "";
        for (var i = 1; i < rsp_json.length; ++i) {
            var photo = rsp_json[i].photo;
            var username = rsp_json[i].username;
            friends_html += "<li><img src='./img/" + photo + "'>" +
                "<div id='invite'>" + username + "</div>" +
                "<button onclick=\"invite_friend('" + username + "')\">邀请</button></li>";
        }
        console.log(friends_html);
        var element = document.getElementById("friends-list");
        element.insertAdjacentHTML('beforeend', friends_html);
        return;
    }

    // 获取好友申请记录
    if (rsp_json.length != null && rsp_json[0].optype == "apply_select") {
        var friends_html = "";
        for (var i = 1; i < rsp_json.length; ++i) {
            var photo = rsp_json[i].photo;
            var username = rsp_json[i].username;
            friends_html += "<li><img src='./img/" + photo + "'>" +
                "<div id='invite'>" + username + "</div>" +
                "<button onclick=\"pass('" + username + "')\">同意</button></li>" +
                "<button onclick=\"refuse('" + username + "')\">拒绝</button></li>";
        }
        console.log(friends_html);
        var element = document.getElementById("friends-list");
        element.insertAdjacentHTML('beforeend', friends_html);

        // 发送查询好友列表请求
        var friends_select = {
            optype: "friends"
        };
        ws_hdl.send(JSON.stringify(friends_select));


        return;
    }

    if (rsp_json.result == false && rsp_json["optype"] != "invite_ol") {
        alert(evt.data);
        location.replace("/login.html");
        return;
    }

    if (rsp_json["optype"] == "hall_ready") {
        var friends_apply = {
            optype: "apply_select"
        };
        ws_hdl.send(JSON.stringify(friends_apply));

        console.log("游戏大厅连接建立成功！");
    } 
    else if (rsp_json["optype"] == "match_success") {
        location.replace("/room.html");
    } 
    else if (rsp_json["optype"] == "match_start") {
        console.log("玩家已经加入匹配队列");
        button_flag = "start";
        if (rsp_json.invite == true) {
            be.innerHTML = "等待对方同意邀请";
        } else {
            be.innerHTML = "匹配中 点击按钮停止匹配!";
        }
        return;
    } 
    else if (rsp_json["optype"] == "match_stop") {
        console.log("玩家已经移除匹配队列");
        button_flag = "stop";
        be.innerHTML = "开始匹配";
        return;
    } 
    else if (rsp_json["optype"] == "invite_ol") {
        if (rsp_json.result == false) {
            alert("当前用户不在线");
            return;
        }
        // 如果查询的用户在线，则可以选择是否邀请用户对战
        if (confirm("当前玩家在线，是否邀请玩家对战")) {
            var invite_start = {
                optype: "invite_start",
                uid: rsp_json["uid"],
                hid: rsp_json["hid"]
            };
            ws_hdl.send(JSON.stringify(invite_start));
        }
    } 
    else if (rsp_json["optype"] == "invite_start") {
        var str = "是否同意" + rsp_json["username"] + "的约战邀请";
        console.log(rsp_json["id"]);
        if (confirm(str)) {
            // 同意邀请则发送请求
            var invite_info = {
                optype: "invite_success",
                id: rsp_json["id"]
            };
            ws_hdl.send(JSON.stringify(invite_info));
        } else {
            // 拒绝邀请也要做相应的响应
            var invite_info = {
                optype: "invite_fail",
                hid: rsp_json["id"]
            };
            ws_hdl.send(JSON.stringify(invite_info));
        }
    } 
    else if (rsp_json["optype"] == "invite_fail") {
        be.innerHTML = "开始匹配";
    } 
    else if (rsp_json["optype"] == "file_success") {
        location.replace("/hall.html");
    } 
    else if (rsp_json["optype"] == "apply_success") {
        alert("申请成功，等待对方同意");
        location.replace("/hall.html");
    } 
    else if (rsp_json["optype"] == "apply_error") {
        alert(rsp_json["reason"]);
    } 
    else if (rsp_json["optype"] == "pass_success") {
        location.replace("/hall.html");
    } 
    else if (rsp_json["optype"] == "pass_error") {
        alert(rsp_json["reason"]);
    } 
    else if (rsp_json["optype"] == "refuse_success") {
        location.replace("/hall.html");
    } 
    else if (rsp_json["optype"] == "refuse_error") {
        alert(rsp_json["reason"]);
    } 
    else {
        alert(evt.data);
        location.replace("/login.html");
        return;
    }
}
