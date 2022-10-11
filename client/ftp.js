/* FTP Client */

import log4js from "log4js";
import net from "net";
import { EventEmitter } from "events";

const logger = log4js.getLogger("ftp-service");
logger.level = "debug";
/*
[LOGIN]
 USER: username
 PASS: password
[RENAME]
 RNFR
 RNTO
[CD]
[MKD]
[RMD]
[PWD]
[QUIT/ABOR]
[SYST] - [TYPE]
[PASV/PORT mode set]
*/

/* binary, cd, rmdir, mkdir, ls/dir, get, send, user, pass*/

/* gen a react to outside */
function genReaction(message, wait, kill = false) {
  return {
    message: message.trim(),
    wait: wait,
    kill: kill,
  };
}

function genReactionFromReply(reply, wait, kill = false) {
  return {
    message: reply.genMessage().trim(),
    reply: reply,
    wait: wait,
    kill: kill,
  };
}

const reply_proto = {
  genMessage: function (end_break = false) {
    if (!this.message.endsWith("\r\n")) this.message = this.message + "\r\n";
    let msg = "";
    const sentences = this.message.split("\r\n");
    const len = sentences.length - 1;
    for (let i = 0; i < len - 1; i++) {
      msg = msg + this.code + "-" + sentences[i] + "\r\n";
    }
    msg =
      msg + this.code + " " + sentences[len - 1] + (end_break ? "\r\n" : "");
    return msg;
  },
};

function FTPReply(code, msg) {
  const r = Object.create(reply_proto);
  r.code = code;
  r.message = msg;
  r.kind = r.code >= 100 && r.code <= 199 ? "mark" : "response";
  // console.log(r.code / 100);
  if (r.code == 421) r.quit = true;
  else r.quit = false;
  // parse reply and check respone/mark
  return r;
}

const request_proto = {
  sendRequest: function (ftp_socket) {
    return ftp_socket.write(this.command + " " + this.params + "\r\n");
  },
};

function FTPRequest(command, param) {
  const r = Object.create(request_proto);
  r.command = command;
  r.param = param;
  return r;
}

const ftpservice = {
  runFTP: function () {
    logger.info(
      "running on " +
        this.ftp_socket.localAddress +
        ":" +
        this.ftp_socket.localPort
    );
    // TODO: another thread for
  },
  /* return value show if the data is freshed to kernel buffer */
  runAct: function (action) {
    if ("handle" + action.action in this) {
      logger.debug("Action get " + action.action);
      return this["handle" + action.action](action);
    } else {
      logger.warn("Unknown action " + action.action);
      return { status: "unknown", msg: "unknown action." };
    }
  },
  /* checkLine will try to remove those substring ends in \r\n but not match reply format */
  /* checkLine will remove the \r\n from the endline [for print] */
  checkLine: function (raw) {
    const l = raw.indexOf("\r\n") + 2; /* [0, len) */
    if (l <= 5) return { msg: null, len: l }; /* ill format 123 \r\n */
    const re = /[1-5][0-9][0-9]/i;
    if (raw.search(re) != 0) return { msg: null, len: l };
    if (raw[3] != " " && raw[3] != "-") return { msg: null, len: l };

    return {
      msg: raw.substring(4, l),
      endline: raw[3] == " " ? true : false,
      code: parseInt(raw.substring(0, 3)),
      len: l /* length of the line */,
    };
  },
  /* check Reply from this.buffer */
  checkReply: function () {
    let message = "";
    let length = 0;
    let reply_code = -1;

    while (true) {
      const { msg, endline, code, len } = this.checkLine(
        this.buffer.slice(length)
      );

      if (msg == null) return null; /* reply not complete */
      if (msg == "ill-format") {
        length += len;
        /* checkLine find some ill-format substring */
        /* we will throw it when a complete reply received */
        continue;
      }

      /* msg is not null */
      message = message.concat(msg);
      length += len;

      if (reply_code == -1) {
        reply_code = code;
      } else if (reply_code != -1 && code != reply_code) {
        logger.warn("recieve a ill-format reply" + buffer);
      }

      if (endline) break;
    }

    this.buffer = this.buffer.slice(length);

    /* return reply and new buffer */
    return FTPReply(reply_code, message);
  },
  waitReply: function (data) {
    logger.debug("[waitReply] some data come");
    this.buffer = this.buffer.concat(data);
    // logger.debug(JSON.stringify(this.buffer));

    const reply = this.checkReply();
    if (reply == null) return;

    /* when receive a reply, call it callback */
    const r = this.callbacks.at(0);
    if (!r) logger.warn("[waitReply] callback lost");
    if (reply.kind == "response") this.callbacks.shift();
    r?.(reply);
  },
  waitData: function (data) {
    logger.debug("[waitData] some data come");
    this.dbuffer = this.dbuffer.concat(data);
    /* big file save. use rv of callback */
  },
  handleRmdir: function (action) {
    logger.info("rmdir [" + action.dir + "]");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("RMD " + action.dir + "\r\n");
  },
  handleMkdir: function (action) {
    logger.info("mkdir [" + action.dir + "]");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("MKD " + action.dir + "\r\n");
  },
  handleCwd: function (action) {
    logger.info("cwd [" + action.dir + "]");
    this.callbacks.push((reply) => {
      logger.debug("reply" + reply.message);
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("CWD " + action.dir + "\r\n");
  },
  handlePwd: function (action) {
    logger.info("pwd");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("PWD\r\n");
  },
  handleUser: function (action) {
    logger.info("user [" + action.username + "] login ");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("USER " + action.username + "\r\n");
  },
  handlePass: function (action) {
    logger.info("password is [" + action.password + "]");
    this.callbacks.push((reply) => {
      this.react?.(genReactionFromReply(reply, false));
    });
    this.writeRaw("PASS " + action.password + "\r\n");
  },
  handleRename: function (action, step = "rnfr") {
    if (step == "rnfr") {
      this.callbacks.push((reply) => {
        if (reply.code == 350) {
          this.react?.(genReactionFromReply(reply, true));
          this.handleRename(action, "rnto");
        } else {
          this.react?.(genReactionFromReply(reply, false));
        }
      });
      this.writeRaw("RNFR " + action.from + "\r\n");
    } else if (step == "rnto") {
      logger.debug("[handleRename] rnto");
      this.callbacks.push((reply) => {
        if (parseInt(reply.code / 100) == 2) {
          // TODO:
          this.react?.(genReactionFromReply(reply, false));
        } else {
          this.react?.(genReactionFromReply(reply, false));
        }
      });
      this.writeRaw("RNTO " + action.to + "\r\n");
      // transfer data in data_socket
    }
  },
  handleList: function (action, step = "mode") {
    if (step == "mode") {
      this.prepareMode((reply) => {
        console.log("callback", reply);
        this.handleList(action, "list");
      });
    } else if (step == "list") {
      logger.debug("[handleList] list");
      this.callbacks.push((reply) => {
        if (reply.code == 425 || reply.code == 426) {
          logger.debug("receive a 425 or 426 code");
          this.react?.(genReactionFromRepl(reply, false));
        }
        if (reply.code == 150) {
          this.react?.(genReactionFromReply(reply, true));
        }
        if (reply.code == 226) {
          this.react?.(genReactionFromReply(reply, true));
          this.handleList(action, "over");
        }
      });
      this.writeRaw("LIST\r\n");
      // transfer data in data_socket
    } else if (step == "over") {
      // over list
      const msg = this.dbuffer;
      this.dbuffer = "";
      this.react?.(genReaction(msg, false));
      logger.log("[handleList] over");
    }
  },
  writeRaw: function (raw) {
    logger.debug("write " + raw.trim() + " to ftp_socket");
    const ret = this.ftp_socket.write(raw);
    return ret;
  },
  handleTest: function (action) {
    return {
      status: this.writeRaw(action.msg),
      msg: "test message.",
    };
  },
  handlePassive: function (action) {
    if (this.mode == "pasv") this.mode = "port";
    else this.mode = "pasv";
    this.react?.(genReaction("Current mode is " + this.mode.toUpperCase(), false));
  },
  handleError: function () {
    logger.warn("some error happened");
  },
  /* get file */
  prepareMode: function (callback = null) {
    /* checkmode */
    if (this.mode == "pasv") {
      this.callbacks.push((reply) => {
        logger.debug("[mode prepare]" + reply.genMessage());
        console.log(reply.message);
        const regex = /((([0-9]{1,3}),){5})([0-9]{1,3})/g;
        const info = reply.message.match(regex)?.[0];
        if (info != null) {
          const ipre = /((([0-9]{1,3}),){3})([0-9]{1,3})/g;
          this.d_pasvhost = info.match(ipre)?.[0].replaceAll(",", ".");
          const num = info.split(",");
          this.d_pasvport = parseInt(num.at(-2)) * 256 + parseInt(num.at(-1));

          logger.info(
            "[prepareMode] PASV: " + this.d_pasvhost + ":" + this.d_pasvport
          );

          this.dsocket = net.createConnection(
            this.d_pasvport,
            this.d_pasvip,
            () => {
              logger.debug("dsocket connection created.");
              callback?.(reply);
            }
          );
          this.dsocket.on("data", (data) => this.waitData.bind(this)(data));
        }
        // get dserver_host/port from reply.
        logger.debug("[prepareMode] try to call the callback");
      });
      this.writeRaw("PASV\r\n");
    } else if (this.mode == "port") {
      // prepare listen socket for port mode
      this.dserver_socket = net.createServer((sock) => {
        // TODO: check permission
        logger.debug("sock got");
        this.dsocket = sock;
        this.dsocket.on("data", (data) => this.waitData.bind(this)(data));
        // this.dserver_socket.close();
      });
      this.dserver_socket.maxConnections = 1;
      this.dserver_socket = this.dserver_socket.listen();
      this.dserver_socket.on("listening", () => {
        const port = this.dserver_socket.address().port;
        let msg =
          "=" +
          this.ftp_socket.localAddress +
          "." +
          parseInt(port / 256) +
          "." +
          (port % 256);
        msg = msg.replaceAll(".", ",");
        logger.debug("prepare to send " + msg);
        this.callbacks.push((reply) => {
          logger.debug("port server up.");
          callback?.(reply);
        });
        this.writeRaw("PORT " + msg + "\r\n");
      });
      // logger.debug("listen to " + this.dserver_socket.port);

      // get dserver_host/port from reply.
      // logger.debug("[prepareMode] try to call the callback");
      // this.callbacks.push((reply) => callback?.(reply));
      // this.writeRaw("PORT\r\n");
    } else return false;
    return false;
  },
  handleGet: function (action, step = "mode") {
    if (step == "mode") {
      prepareMode((reply) => {
        this.handleGet(action, "retr");
      });
    } else if (step == "retr") {
      // transfer data in data_socket
    }
    return;
  },
  handleClose: function () {
    logger.info("The FTP service is fully closed");
  },
  usePort: function () {
    logger.info("port mode");
  },
  usePasv: function (step = "init", callback = null) {
    logger.info("port mode[" + step + "] start");

    writeRaw("PASV\r\n");
  },
  killService: function () {
    logger.info("[killService] someone try to kill service");
    this.ftp_socket?.destroy();
  },
  greet: function (reply) {},
  startService: function () {
    if (this.server_ip != null) {
      logger.debug("ip check ok");
      this.ftp_socket = net.createConnection(
        this.server_port,
        this.server_ip,
        this.runFTP.bind(this)
      );
      this.callbacks.push((reply) => {
        this.react(genReactionFromReply(reply, false));
      }); /* greeting message */
      this.ftp_socket.on("error", this.handleError.bind(this));
      this.ftp_socket.on("data", (data) => this.waitReply.bind(this)(data));
      this.ftp_socket.on("close", this.handleClose.bind(this));
      return true;
    }
    return false;
  },
  setAddress: function (host, port) {
    this.server_ip = host;
    this.server_port = port;
  },
  setReact: function (react) {
    this.react = react;
  },
};

function ftpclient() {
  this.server_ip = "0.0.0.0";
  this.server_port = 5001;
  this.ftp_socket = null; /* ftp socket for commands */
  this.dsocket = null; /* data socket for data transfering */
  this.dserver_socket = null; /* data server socket for PORT mode */
  this.dserver_host = null;
  this.dserver_port = null;
  this.emitter = new EventEmitter();
  this.buffer = ""; /* buffer for reading */
  this.dbuffer = ""; /* dbuffer */
  this.mode = "port"; /* pasv or port */
  this.react = null; /* the react of client outside */
  this.callbacks = []; /* queue when wait for a response*/
}

function initFTP() {
  Object.assign(ftpclient.prototype, ftpservice);
  const client = new ftpclient();
  return client;
}

export { initFTP };