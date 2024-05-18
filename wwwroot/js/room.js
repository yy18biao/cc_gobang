let chessBoard = [];
let BOARD_ROW_AND_COL = 15;
let chess = document.getElementById('chess');
let context = chess.getContext('2d');//获取chess控件的2d画布

var ws_url = "ws://" + location.host + "/room";
var ws_hdl = new WebSocket(ws_url);

var room_info = null;//用于保存房间信息 
var is_me;

// 初始化房间
function initGame() {
    initBoard();
    context.strokeStyle = "#BFBFBF";
    // 背景图片
    let logo = new Image();
    logo.src = "https://img0.baidu.com/it/u=955686356,3862136952&fm=253&fmt=auto&app=138&f=JPEG?w=889&h=500";
    logo.onload = function () {
        // 绘制图片
        context.drawImage(logo, 0, 0, 450, 450);
        // 绘制棋盘
        drawChessBoard();
    }
}

// 初始化棋盘
function initBoard() {
    for (let i = 0; i < BOARD_ROW_AND_COL; i++) {
        chessBoard[i] = [];
        for (let j = 0; j < BOARD_ROW_AND_COL; j++) {
            chessBoard[i][j] = 0;
        }
    }
}

// 绘制棋盘网格线
function drawChessBoard() {
    for (let i = 0; i < BOARD_ROW_AND_COL; i++) {
        context.moveTo(15 + i * 30, 15);
        context.lineTo(15 + i * 30, 430); //横向的线条
        context.stroke();
        context.moveTo(15, 15 + i * 30);
        context.lineTo(435, 15 + i * 30); //纵向的线条
        context.stroke();
    }
}

//绘制棋子
function oneStep(i, j, isWhite) {
    if (i < 0 || j < 0) return;
    context.beginPath();
    context.arc(15 + i * 30, 15 + j * 30, 13, 0, 2 * Math.PI);
    context.closePath();
    var gradient = context.createRadialGradient(15 + i * 30 + 2, 15 + j * 30 - 2, 13, 15 + i * 30 + 2, 15 + j * 30 - 2, 0);
    // 区分黑白子
    if (!isWhite) {
        gradient.addColorStop(0, "#0A0A0A");
        gradient.addColorStop(1, "#636766");
    } else {
        gradient.addColorStop(0, "#D1D1D1");
        gradient.addColorStop(1, "#F9F9F9");
    }
    context.fillStyle = gradient;
    context.fill();
}

//棋盘区域的点击事件
chess.onclick = function (e) {
    // 获取下棋位置，判断当前下棋操作是否正常
    // 当前是否轮到自己走棋了
    // 当前位置是否已经被占用
    // 向服务器发送走棋请求
    if (!is_me) {
        alert("等待对方走棋....");
        return;
    }
    let x = e.offsetX;
    let y = e.offsetY;
    // 注意, 横坐标是列, 纵坐标是行
    // 这里是为了让点击操作能够对应到网格线上
    let col = Math.floor(x / 30);
    let row = Math.floor(y / 30);
    if (chessBoard[row][col] != 0) {
        alert("当前位置已有棋子！");
        return;
    }
    send_chess(row, col);
}

// 发送下棋请求
function send_chess(r, c) {
    var chess_info = {
        optype: "put_chess",
        room_id: room_info.room_id,
        uid: room_info.uid,
        row: r,
        col: c
    };
    ws_hdl.send(JSON.stringify(chess_info));
    console.log("click:" + JSON.stringify(chess_info));
}

window.onbeforeunload = function () {
    ws_hdl.close();
}
ws_hdl.onopen = function () {
    console.log("房间长连接建立成功");
}
ws_hdl.onclose = function () {
    console.log("房间长连接断开");
}
ws_hdl.onerror = function () {
    console.log("房间长连接出错");
}

// 编辑提示框
function set_screen(me) {
    var screen_div = document.getElementById("screen");
    if (me) {
        screen_div.innerHTML = "轮到己方走棋...";
    } else {
        screen_div.innerHTML = "轮到对方走棋...";
    }
}

ws_hdl.onmessage = function (evt) {
    // 在收到room_ready之后进行房间的初始化
    // 将房间信息保存起来
    var info = JSON.parse(evt.data);
    console.log(JSON.stringify(info));
    if (info.optype == "room_ready") {
        room_info = info;
        is_me = info.uid == info.white_id ? true : false;
        set_screen(is_me);
        var me_div = document.getElementById("me");
        me_div.innerHTML = "<img id='data_head' src='./img/" + info.mphoto + "'>"
                              + info.username + "<br>" + "积分：" + info.score
                              + " 胜场：" + info.win_count + " 胜率：" + info.win_rate;
        var rival_div = document.getElementById("rival");
        rival_div.innerHTML = info.hname + "<br>" + "积分：" + info.hscore 
                              + " 胜场：" + info.hwin_count + " 胜率：" + info.hwin_rate 
                              + "<img id='data_head' src='./img/" + info.hphoto + "'>";
        initGame();
    } else if (info.optype == "put_chess") {
        console.log("put_chess" + evt.data);
        // 走棋操作
        // 收到走棋消息，进行棋子绘制
        if (info.result == false) {
            alert(info.reason);
            return;
        }
        // 将标志设置为对方
        is_me = info.uid == room_info.uid ? false : true;
        set_screen(is_me); // 编辑提示框
        //绘制棋子的颜色，应该根据当前下棋角色的颜色确定
        isWhite = info.uid == room_info.white_id ? true : false;
        //绘制棋子
        if (info.row != -1 && info.col != -1) {
            oneStep(info.col, info.row, isWhite);
            //设置棋盘信息
            chessBoard[info.row][info.col] = 1;
        }
        //是否有胜利者
        if (info.winner == 0) {
            return;
        }
        var screen_div = document.getElementById("screen");
        if (room_info.uid == info.winner) {
            screen_div.innerHTML = info.reason;
        } else {
            screen_div.innerHTML = "你输了";
        }

        var chess_area_div = document.getElementById("chess_area");
        var button_div = document.getElementById("return-hall");
        button_div.innerHTML = "返回大厅";
        button_div.onclick = function () {
            ws_hdl.close();
            location.replace("/hall.html");
        }
        chess_area_div.appendChild(button_div);
    } else if (info.optype == "chat") {
        //收到一条消息，判断result，如果为true则渲染一条消息到显示框中
        if (info.result == false) {
            alert(info.reason);
            return;
        }

        var msg_div = document.createElement("p");
        // 根据当前用户id判断是否文本框的位置以及头像
        if (info.uid == room_info.uid) {
            msg_div.innerHTML = info.message + "<img src='./img/" + info.photo + "' id='head'>";
            msg_div.setAttribute("id", "self_msg");
        } else {
            msg_div.innerHTML = "<img src='./img/" + info.photo + "' id='head'>" + info.message;
            msg_div.setAttribute("id", "peer_msg");
        }

        var br_div = document.createElement("br");
        var msg_show_div = document.getElementById("chat_show");
        msg_show_div.appendChild(msg_div);
        msg_show_div.appendChild(br_div);
        document.getElementById("chat_input").value = "";
    }
}

// 聊天动作捕捉
var cb_div = document.getElementById("chat_button");
cb_div.onclick = function () {

    var send_msg = {
        optype: "chat",
        room_id: room_info.room_id,
        uid: room_info.uid,
        message: document.getElementById("chat_input").value
    };

    ws_hdl.send(JSON.stringify(send_msg));
}